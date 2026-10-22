#include "systemproperties.h"
#include "utils.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QStringList>
#include <QLibraryInfo>
#include <QSettings>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QUrlQuery>

#include "path.h"

#include "streaming/session.h"
#include "streaming/streamutils.h"
#include "streaming/vibelight_logic.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

class SystemPropertyQueryThread : public QThread
{
public:
    SystemPropertyQueryThread(SystemProperties* properties)
        : QThread(properties), m_Properties(properties)
    {
        setObjectName("System Properties Async Query Thread");
    }

private:
    void run() override
    {
        bool hasHardwareAcceleration;
        bool rendererAlwaysFullScreen;
        bool supportsHdr;
        QSize maximumResolution;

        Session::getDecoderInfo(m_Properties->testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen, supportsHdr, maximumResolution);

        // Propagate the decoder properties to the SystemProperties singleton and emit any change signals on the main thread
        QMetaObject::invokeMethod(m_Properties, "updateDecoderProperties",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, hasHardwareAcceleration),
                                  Q_ARG(bool, rendererAlwaysFullScreen),
                                  Q_ARG(QSize, maximumResolution),
                                  Q_ARG(bool, supportsHdr));
    }

private:
    SystemProperties* m_Properties;
};

SystemProperties::SystemProperties()
{
    versionString = QString(VERSION_STR);
#ifdef BUILD_COMMIT
    buildString = QString("%1 (%2)").arg(VERSION_STR, BUILD_COMMIT);
#else
    buildString = versionString;
#endif
    hasDesktopEnvironment = WMUtils::isRunningDesktopEnvironment();
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";
    usesMaterial3Theme = QLibraryInfo::version() >= QVersionNumber(6, 5, 0);

#ifdef Q_OS_DARWIN
    isDarwin = true;
#else
    isDarwin = false;
#endif

    QString nativeArch = QSysInfo::currentCpuArchitecture();

#ifdef Q_OS_WIN32
    {
        USHORT processArch, machineArch;

        // Use IsWow64Process2() because it doesn't lie on ARM64
        if (IsWow64Process2(GetCurrentProcess(), &processArch, &machineArch)) {
            switch (machineArch) {
            case IMAGE_FILE_MACHINE_I386:
                nativeArch = "i386";
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                nativeArch = "x86_64";
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                nativeArch = "arm64";
                break;
            }
        }

        isWow64 = nativeArch != QSysInfo::buildCpuArchitecture();
    }
#else
    isWow64 = false;
#endif

    if (nativeArch == "i386") {
        friendlyNativeArchName = "x86";
    }
    else if (nativeArch == "x86_64") {
        friendlyNativeArchName = "x64";
    }
    else {
        friendlyNativeArchName = nativeArch.toUpper();
    }

    // Assume we can probably launch a browser if we're in a GUI environment
    hasBrowser = hasDesktopEnvironment;

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    // These will be queried asynchronously to avoid blocking the UI
    hasHardwareAcceleration = true;
    rendererAlwaysFullScreen = false;
    supportsHdr = true;
    maximumResolution = QSize(0, 0);
}

SystemProperties::~SystemProperties()
{
    waitForAsyncLoad();
}

void SystemProperties::updateDecoderProperties(bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr)
{
    SDL_assert(testWindow);

    if (hasHardwareAcceleration != this->hasHardwareAcceleration) {
        this->hasHardwareAcceleration = hasHardwareAcceleration;
        emit hasHardwareAccelerationChanged();
    }

    if (rendererAlwaysFullScreen != this->rendererAlwaysFullScreen) {
        this->rendererAlwaysFullScreen = rendererAlwaysFullScreen;
        emit rendererAlwaysFullScreenChanged();
    }

    if (maximumResolution != this->maximumResolution) {
        this->maximumResolution = maximumResolution;
        emit maximumResolutionChanged();
    }

    if (supportsHdr != this->supportsHdr) {
        this->supportsHdr = supportsHdr;
        emit supportsHdrChanged();
    }

    SDL_DestroyWindow(testWindow);
    testWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorNativeResolutions.value(displayIndex);
}

QRect SystemProperties::getSafeAreaResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorSafeAreaResolutions.value(displayIndex);
}

int SystemProperties::getRefreshRate(int displayIndex)
{
    // Returns 0 if out of bounds
    return monitorRefreshRates.value(displayIndex);
}

void SystemProperties::startAsyncLoad()
{
    if (systemPropertyQueryThread) {
        // Already started/completed
        return;
    }

    // This isn't actually asynchronous (due to the need to synchronize with
    // SdlGamepadKeyNavigation), but we don't query it in the constructor
    // because it's expensive.
    unmappedGamepads = SdlInputHandler::getUnmappedGamepads();
    if (!unmappedGamepads.isEmpty()) {
        emit unmappedGamepadsChanged();
    }

    // We initialize the video subsystem and test window on the main thread
    // because some platforms (macOS) do not support window creation on
    // non-main threads.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Update display related attributes (max FPS, native resolution, etc).
    //
    // NB: SDL3 will forcefully refresh displays when a window is created,
    // so we place this after the window creation to ensure we don't pay
    // the penalty for mode enumeration twice.
    refreshDisplays();

    systemPropertyQueryThread = new SystemPropertyQueryThread(this);
    systemPropertyQueryThread->start();
}

void SystemProperties::waitForAsyncLoad()
{
    if (systemPropertyQueryThread) {
        systemPropertyQueryThread->wait();
    }
}

void SystemProperties::refreshDisplays()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    monitorNativeResolutions.clear();
    monitorSafeAreaResolutions.clear();
    monitorRefreshRates.clear();

    SDL_DisplayMode bestMode;
    for (int displayIndex = 0; displayIndex < SDL_GetNumVideoDisplays(); displayIndex++) {
        SDL_DisplayMode desktopMode;
        SDL_Rect safeArea;

        if (StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea)) {
            if (desktopMode.w <= 8192 && desktopMode.h <= 8192) {
                // Keep these lists compact because their QML consumers iterate until
                // the first empty entry. Inserting by SDL display index is invalid if
                // an earlier display was skipped (for example, a >8K virtual display).
                monitorNativeResolutions.append(QRect(0, 0, desktopMode.w, desktopMode.h));
                monitorSafeAreaResolutions.append(QRect(0, 0, safeArea.w, safeArea.h));
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Skipping resolution over 8K: %dx%d",
                            desktopMode.w, desktopMode.h);
            }

            // Start at desktop mode and work our way up
            bestMode = desktopMode;
            int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
            for (int i = 0; i < numDisplayModes; i++) {
                SDL_DisplayMode mode;
                if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                    if (mode.w == desktopMode.w && mode.h == desktopMode.h) {
                        if (mode.refresh_rate > bestMode.refresh_rate) {
                            bestMode = mode;
                        }
                    }
                }
            }

            // Try to normalize values around our our standard refresh rates.
            // Some displays/OSes report values that are slightly off.
            if (bestMode.refresh_rate >= 58 && bestMode.refresh_rate <= 62) {
                monitorRefreshRates.append(60);
            }
            else if (bestMode.refresh_rate >= 28 && bestMode.refresh_rate <= 32) {
                monitorRefreshRates.append(30);
            }
            else {
                monitorRefreshRates.append(bestMode.refresh_rate);
            }
        }
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// The warnings, errors and feature lines (clipboard, file drop) from the newest log, newest last.
static QString recentProblemLines(int maxLines)
{
    QDir logDir(Path::getLogDir());
    QFileInfoList logs = logDir.entryInfoList(QStringList("Moonlight-*.log"), QDir::Files, QDir::Time);
    if (logs.isEmpty()) {
        return QString();
    }

    QFile log(logs.first().absoluteFilePath());
    if (!log.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const qint64 maxBytes = 1024 * 1024;
    if (log.size() > maxBytes) {
        log.seek(log.size() - maxBytes);
        log.readLine();
    }

    QStringList matches;
    while (!log.atEnd()) {
        const QString line = QString::fromUtf8(log.readLine()).trimmed();
        if (line.contains("[clipboard]") || line.contains("[file]") ||
                line.contains("warn", Qt::CaseInsensitive) || line.contains("error", Qt::CaseInsensitive) ||
                line.contains("fail", Qt::CaseInsensitive)) {
            matches << line;
            if (matches.size() > maxLines) {
                matches.removeFirst();
            }
        }
    }
    return matches.join('\n');
}

bool SystemProperties::copyRecentProblems()
{
    const QString text = "Vibelight " + buildString + " on " + QSysInfo::prettyProductName() + "\n" + recentProblemLines(80);
    QGuiApplication::clipboard()->setText(text);
    return true;
}

QString SystemProperties::saveDiagnostics()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    QDir().mkpath(dir);

    QString path = QDir(dir).filePath(
        QString("Vibelight-diagnostics-%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&out);
    stream << "Vibelight diagnostics\n";
    stream << "Created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    stream << "Version: " << buildString << "\n";
    stream << "Qt runtime: " << qVersion() << "\n";
    stream << "OS: " << QSysInfo::prettyProductName() << " (kernel " << QSysInfo::kernelVersion() << ")\n";
    stream << "CPU architecture: " << QSysInfo::currentCpuArchitecture() << " (native: " << friendlyNativeArchName << ")\n";
    stream << "Running under WOW64: " << (isWow64 ? "yes" : "no") << "\n";
    stream << "Portable mode: " << (QFile::exists(QDir::currentPath() + "/portable.dat") ? "yes" : "no") << "\n";
    stream << "Wayland: " << (isRunningWayland ? "yes" : "no") << "\n";
    stream << "Hardware acceleration: " << (hasHardwareAcceleration ? "yes" : "no") << "\n";
    stream << "HDR supported: " << (supportsHdr ? "yes" : "no") << "\n";
    stream << "Maximum resolution: " << maximumResolution.width() << "x" << maximumResolution.height() << "\n";
    stream << "Unmapped gamepads: " << (unmappedGamepads.isEmpty() ? QString("none") : unmappedGamepads) << "\n";

    // Vibelight settings (keys and values only, nothing secret is stored under these)
    stream << "\n---- Vibelight settings ----\n";
    {
        QSettings settings;
        settings.beginGroup("vibelight");
        const QStringList keys = settings.allKeys();
        for (const QString& key : keys) {
            stream << key << " = " << settings.value(key).toString() << "\n";
        }
        settings.endGroup();
    }

    stream << "\n---- Recent problems (warnings, errors, clipboard and file lines) ----\n";
    stream << recentProblemLines(80) << "\n";

    // The newest log is the one for this run. Only the end is included to keep the file small.
    QDir logDir(Path::getLogDir());
    QFileInfoList logs = logDir.entryInfoList(QStringList("Moonlight-*.log"), QDir::Files, QDir::Time);
    stream << "\n---- Latest log: " << (logs.isEmpty() ? QString("none found") : logs.first().fileName()) << " ----\n";
    if (!logs.isEmpty()) {
        QFile log(logs.first().absoluteFilePath());
        if (log.open(QIODevice::ReadOnly)) {
            const qint64 maxBytes = 512 * 1024;
            if (log.size() > maxBytes) {
                log.seek(log.size() - maxBytes);
                log.readLine(); // drop the partial first line
                stream << "[... earlier output omitted ...]\n";
            }
            stream << QString::fromUtf8(log.readAll());
        }
    }

    stream.flush();
    out.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    return QDir::toNativeSeparators(path);
}

void SystemProperties::openLogFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Path::getLogDir()));
}

QString SystemProperties::whatsNewText()
{
    QFile file(":/whatsnew.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QStringList SystemProperties::shortcutMenuHotkeyChoices()
{
    QStringList choices;
    for (const char* p = vibelight::menuHotkeyChoices(); *p != '\0'; p++) {
        choices << QString(QChar(*p));
    }
    return choices;
}

QString SystemProperties::shortcutMenuHotkey()
{
    QSettings settings;
    return QString(QChar(vibelight::sanitizeMenuHotkey(
                             settings.value("vibelight/menuHotkey", "H").toString().toStdString())));
}

void SystemProperties::setShortcutMenuHotkey(const QString& letter)
{
    QSettings settings;
    settings.setValue("vibelight/menuHotkey",
                      QString(QChar(vibelight::sanitizeMenuHotkey(letter.toStdString()))));
}

bool SystemProperties::shortcutMenuAutoHide()
{
    return QSettings().value("vibelight/menuAutoHide", true).toBool();
}

void SystemProperties::setShortcutMenuAutoHide(bool hide)
{
    QSettings().setValue("vibelight/menuAutoHide", hide);
}

QString SystemProperties::problemReportUrl()
{
    QString body;
    body += "**What happened?**\n\n\n";
    body += "**What did you expect to happen?**\n\n\n";
    body += "**Steps to reproduce**\n\n\n";
    body += "---\n";
    body += "Please drag the diagnostics file Vibelight just saved into this issue.\n\n";
    body += "- Version: " + buildString + "\n";
    body += "- OS: " + QSysInfo::prettyProductName() + "\n";
    body += "- CPU architecture: " + QSysInfo::currentCpuArchitecture() + "\n";
    body += QString("- Hardware acceleration: %1\n").arg(hasHardwareAcceleration ? "yes" : "no");
    body += QString("- Portable mode: %1\n").arg(QFile::exists(QDir::currentPath() + "/portable.dat") ? "yes" : "no");

    QUrl url("https://github.com/nyteghost/vibelight/issues/new");
    QUrlQuery query;
    query.addQueryItem("title", "Problem: ");
    query.addQueryItem("body", body);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QString SystemProperties::differencesText()
{
    QFile file(":/differences.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool SystemProperties::shouldShowWhatsNew()
{
    if (whatsNewText().isEmpty()) {
        return false;
    }
    QSettings settings;
    return settings.value("vibelight/lastSeenVersion").toString() != versionString;
}

void SystemProperties::markWhatsNewSeen()
{
    QSettings settings;
    settings.setValue("vibelight/lastSeenVersion", versionString);
}

int SystemProperties::suggestedBitrateKbps()
{
    QSettings settings;
    return settings.value("vibelight/suggestedBitrateKbps", 0).toInt();
}
