#include "autoupdatechecker.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent)
{
    m_Nam = new QNetworkAccessManager(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Vibelight version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
    if (!m_Nam) {
        Q_ASSERT(m_Nam);
        return;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // We'll get a callback when this is finished. Vibelight checks the latest
    // published release of its own GitHub repository instead of upstream Moonlight.
    QUrl url("https://api.github.com/repos/nyteghost/moonlight-qt/releases/latest");
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Vibelight-UpdateChecker"); // Required by the GitHub API
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif
    m_Nam->get(request);
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(QString& string, QVector<int>& version)
{
    // Accept release tags such as "v6.2.1" or "6.2.1-vibelight.1" by dropping a leading
    // "v" and ignoring anything after the numeric part.
    QString numeric = string.trimmed();
    if (numeric.startsWith('v') || numeric.startsWith('V')) {
        numeric.remove(0, 1);
    }
    int end = 0;
    while (end < numeric.size() && (numeric[end].isDigit() || numeric[end] == '.')) {
        end++;
    }
    numeric.truncate(end);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = numeric.split('.', Qt::SkipEmptyParts);
#else
    QStringList list = numeric.split('.', QString::SkipEmptyParts);
#endif
    for (const QString& component : std::as_const(list)) {
        version.append(component.toInt());
    }
}

QString AutoUpdateChecker::getPlatform()
{
#if defined(STEAM_LINK)
    return QStringLiteral("steamlink");
#elif defined(APP_IMAGE)
    return QStringLiteral("appimage");
#elif defined(Q_OS_DARWIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 changed this from 'osx' to 'macos'. Use the old one
    // to be consistent (and not require another entry in the manifest).
    return QStringLiteral("osx");
#else
    return QSysInfo::productType();
#endif
}

void AutoUpdateChecker::pickDownloadAsset(const QJsonArray& assets)
{
    m_DownloadUrl.clear();
    m_DownloadName.clear();
    m_DownloadSha256.clear();
    m_DownloadSize = 0;

    QString wanted;
#if defined(Q_OS_WIN32)
    // Release assets are named Vibelight-<version>-Windows-<x64|arm64>-portable.zip
    QString arch = QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive) ? "arm64" : "x64";
    wanted = QString("-Windows-%1-portable.zip").arg(arch);
#elif defined(Q_OS_DARWIN)
    wanted = "-macOS.dmg";
#else
    // No downloadable package for the other platforms; the button opens the release page
    return;
#endif

    for (const QJsonValue& value : assets) {
        QJsonObject asset = value.toObject();
        QString name = asset["name"].toString();
        QString url = asset["browser_download_url"].toString();

        if (!name.endsWith(wanted, Qt::CaseInsensitive)) {
            continue;
        }

        // Only ever download over HTTPS from GitHub, and never trust a file name
        // from the network to contain a path.
        QUrl parsed(url);
        if (parsed.scheme() != "https" || parsed.host() != "github.com" ||
                name != QFileInfo(name).fileName()) {
            continue;
        }

        m_DownloadUrl = url;
        m_DownloadName = name;
        m_DownloadSize = (qint64)asset["size"].toDouble();

        // GitHub reports a digest like "sha256:abcd..." for release assets
        QString digest = asset["digest"].toString();
        if (digest.startsWith("sha256:", Qt::CaseInsensitive)) {
            m_DownloadSha256 = digest.mid(7).toLower();
        }
        return;
    }
}

QString AutoUpdateChecker::uniqueDownloadPath(const QString& fileName) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    QDir().mkpath(dir);

    QFileInfo info(fileName);
    QString base = info.completeBaseName();
    QString suffix = info.suffix();

    QString path = QDir(dir).filePath(fileName);
    for (int i = 1; QFile::exists(path) && i < 1000; i++) {
        path = QDir(dir).filePath(QString("%1 (%2).%3").arg(base).arg(i).arg(suffix));
    }
    return path;
}

void AutoUpdateChecker::downloadUpdate()
{
    if (m_DownloadUrl.isEmpty()) {
        emit downloadFailed(tr("No download is available for this platform."));
        return;
    }
    if (m_DownloadReply != nullptr) {
        // Already downloading
        return;
    }

    m_DownloadPath = uniqueDownloadPath(m_DownloadName);
    m_DownloadFile.setFileName(m_DownloadPath + ".part");
    if (!m_DownloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(tr("Unable to write to %1").arg(QDir::toNativeSeparators(m_DownloadPath)));
        return;
    }

    if (m_DownloadNam == nullptr) {
        m_DownloadNam = new QNetworkAccessManager(this);
        m_DownloadNam->setStrictTransportSecurityEnabled(true);
        m_DownloadNam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }

    QNetworkRequest request{QUrl(m_DownloadUrl)};
    request.setRawHeader("User-Agent", "Vibelight-UpdateChecker");
    m_DownloadReply = m_DownloadNam->get(request);

    connect(m_DownloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_DownloadReply != nullptr && m_DownloadFile.isOpen()) {
            m_DownloadFile.write(m_DownloadReply->readAll());
        }
    });
    connect(m_DownloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        // GitHub's redirect target may not report a total; fall back to the asset size
        emit downloadProgress(received, total > 0 ? total : m_DownloadSize);
    });
    connect(m_DownloadReply, &QNetworkReply::finished, this, [this]() {
        finishDownload();
    });
}

void AutoUpdateChecker::failDownload(const QString& message)
{
    if (m_DownloadFile.isOpen()) {
        m_DownloadFile.close();
    }
    QFile::remove(m_DownloadFile.fileName());

    if (m_DownloadReply != nullptr) {
        m_DownloadReply->deleteLater();
        m_DownloadReply = nullptr;
    }

    emit downloadFailed(message);
}

void AutoUpdateChecker::finishDownload()
{
    QNetworkReply* reply = m_DownloadReply;
    if (reply == nullptr) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        failDownload(tr("Download failed: %1").arg(reply->errorString()));
        return;
    }

    m_DownloadFile.write(reply->readAll());
    m_DownloadFile.close();

    // Verify the download against the digest GitHub published, when there is one
    if (!m_DownloadSha256.isEmpty()) {
        QFile partial(m_DownloadFile.fileName());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!partial.open(QIODevice::ReadOnly) || !hash.addData(&partial) ||
                hash.result().toHex().toLower() != m_DownloadSha256.toUtf8()) {
            failDownload(tr("The downloaded file didn't match its checksum, so it was deleted."));
            return;
        }
    }

    if (!QFile::rename(m_DownloadFile.fileName(), m_DownloadPath)) {
        failDownload(tr("Unable to save %1").arg(QDir::toNativeSeparators(m_DownloadPath)));
        return;
    }

    reply->deleteLater();
    m_DownloadReply = nullptr;
    m_DownloadedFile = m_DownloadPath;
    emit downloadFinished(QDir::toNativeSeparators(m_DownloadedFile));
}

void AutoUpdateChecker::revealDownload()
{
    QString path = m_DownloadedFile.isEmpty() ? m_DownloadPath : m_DownloadedFile;
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }
}

int AutoUpdateChecker::compareVersion(QVector<int>& version1, QVector<int>& version2) {
    for (int i = 0;; i++) {
        int v1Val = 0;
        int v2Val = 0;

        // Treat missing decimal places as 0
        if (i < version1.count()) {
            v1Val = version1[i];
        }
        if (i < version2.count()) {
            v2Val = version2[i];
        }
        if (i >= version1.count() && i >= version2.count()) {
            // Equal versions
            return 0;
        }

        if (v1Val < v2Val) {
            return -1;
        }
        else if (v1Val > v2Val) {
            return 1;
        }
    }
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

    // Delete the QNetworkAccessManager to free resources and
    // prevent the bearer plugin from polling in the background.
    m_Nam->deleteLater();
    m_Nam = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif

        // Read all data and queue the reply for deletion
        QString jsonString = stream.readAll();
        reply->deleteLater();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qWarning() << "GitHub release response malformed:" << error.errorString();
            return;
        }

        QJsonObject release = jsonDoc.object();
        if (!release["tag_name"].isString() || !release["html_url"].isString()) {
            qWarning() << "GitHub release response missing tag_name or html_url";
            return;
        }

        pickDownloadAsset(release["assets"].toArray());

        QString latestVersion = release["tag_name"].toString();
        qDebug() << "Latest Vibelight release is:" << latestVersion;

        QVector<int> latestVersionQuad;
        parseStringToVersionQuad(latestVersion, latestVersionQuad);
        if (latestVersionQuad.isEmpty()) {
            qWarning() << "Unable to parse release tag as a version:" << latestVersion;
            return;
        }

        int res = compareVersion(m_CurrentVersionQuad, latestVersionQuad);
        if (res < 0) {
            // m_CurrentVersionQuad < latestVersionQuad
            qDebug() << "Update available";
            QString displayVersion = latestVersion;
            if (displayVersion.startsWith('v') || displayVersion.startsWith('V')) {
                displayVersion.remove(0, 1);
            }
            emit onUpdateAvailable(displayVersion, release["html_url"].toString());
        }
        else if (res > 0) {
            qDebug() << "Latest release is older than the current version";
        }
        else {
            qDebug() << "Latest release matches the current version";
        }
    }
    else if (reply->error() == QNetworkReply::ContentNotFoundError) {
        // The repository has no published releases yet
        qDebug() << "No Vibelight releases published yet";
        reply->deleteLater();
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error();
        reply->deleteLater();
    }
}
