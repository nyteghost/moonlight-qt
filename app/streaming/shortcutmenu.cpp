#include "shortcutmenu.h"
#include "streaming/input/input.h"
#include "path.h"

#include <QtGlobal>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QSettings>

#include <Limelight.h>

#include "streaming/session.h"
#include "streaming/video/decoder.h"

#include <cstdlib>
#include <cstring>

namespace {

const int kCollapsedWidth = 190;
const int kHeaderHeight = 28;
const int kPadding = 8;
const int kTitleHeight = 24;
const int kRowHeight = 26;
const int kColumnWidth = 290;
const int kDefaultTopMargin = 6;
const int kDragThresholdPx = 4;

// Palette matches the Vibepollo web UI: near-black navy surfaces with an indigo accent
const SDL_Color kTextColor = {0xE2, 0xE8, 0xF0, 0xFF};
const SDL_Color kHintColor = {0x8B, 0x95, 0xAD, 0xFF};
const SDL_Color kTitleColor = {0xA5, 0xB4, 0xFC, 0xFF};
const SDL_Color kDangerColor = {0xF0, 0x70, 0x7A, 0xFF};
const SDL_Color kHeaderTextColor = {0xFF, 0xFF, 0xFF, 0xFF};
const int kMaxRowsPerColumn = 14;
const int kMaxCustomColumns = 2; // keeps the menu narrower than a 1920px screen
const int kFirstCustomColumn = 3; // columns 0-2 are built in

bool pointInRect(const SDL_Rect& r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

} // namespace

bool ShortcutMenu::isSupportedOnThisPlatform()
{
#ifdef STEAM_LINK
    return false;
#else
    if (qEnvironmentVariableIsSet("ML_DISABLE_SHORTCUT_MENU")) {
        return false;
    }

    // We need a windowing system that supports multiple top-level windows that can be
    // positioned by the application and kept on top.
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver != nullptr &&
           (strcmp(driver, "windows") == 0 ||
            strcmp(driver, "x11") == 0 ||
            strcmp(driver, "cocoa") == 0);
#endif
}

ShortcutMenu::ShortcutMenu(SDL_Window* parentWindow, SdlInputHandler* inputHandler)
    : m_Parent(parentWindow),
      m_Input(inputHandler),
      m_Window(nullptr),
      m_Font(nullptr),
      m_SmallFont(nullptr),
      m_FontData(Path::readDataFile("ModeSeven.ttf")),
      m_TtfInitialized(false),
      m_NumColumns(2),
      m_ExpandedWidth(0),
      m_ExpandedHeight(0),
      m_Expanded(false),
      m_Shown(false),
      m_Revealed(false),
      m_HoverIndex(-1),
      m_HeaderHover(false),
      m_ConfirmIndex(-1),
      m_RecaptureOnClose(false),
      m_HasCustomPosition(false),
      m_OffsetX(0),
      m_OffsetY(kDefaultTopMargin),
      m_MouseDown(false),
      m_Dragging(false),
      m_DownGlobalX(0), m_DownGlobalY(0),
      m_GrabLocalX(0), m_GrabLocalY(0),
      m_TransientUntil(0),
      m_TransientOwnsReveal(false),
      m_SwallowEscUp(false),
      m_StartTicks(SDL_GetTicks()),
      m_LastTimerSecond(-1),
      m_CheckActive(false),
      m_CheckEndsAt(0),
      m_CheckStartFrames(0),
      m_CheckStartDropped(0),
      m_HintOverridden(false),
      m_SavedHint(nullptr)
{

    if (m_FontData.isEmpty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: font failed to load");
        return;
    }

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: TTF_Init() failed: %s", TTF_GetError());
        return;
    }
    m_TtfInitialized = true;

    m_Font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()), 1, 15);
    m_SmallFont = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()), 1, 12);
    if (m_Font == nullptr || m_SmallFont == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: TTF_OpenFont() failed: %s", TTF_GetError());
        return;
    }

    buildItems();
    layoutItems();

    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    flags |= SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR;
#endif
    SDL_Window* window = SDL_CreateWindow("Vibelight Shortcuts",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          kCollapsedWidth, kHeaderHeight, flags);
    if (window == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: SDL_CreateWindow() failed: %s", SDL_GetError());
        return;
    }
    m_Window = window;
}

ShortcutMenu::~ShortcutMenu()
{
    endDrag();
    overrideMinimizeOnFocusLoss(false);

    if (m_Window != nullptr) {
        SDL_DestroyWindow(m_Window);
    }
    if (m_Font != nullptr) {
        TTF_CloseFont(m_Font);
    }
    if (m_SmallFont != nullptr) {
        TTF_CloseFont(m_SmallFont);
    }
    if (m_TtfInitialized) {
        TTF_Quit();
    }
}

namespace {

// Names accepted in the custom shortcuts file, mapped to Windows virtual-key codes
bool lookupKeyName(const QString& rawName, unsigned short& vk)
{
    const QString name = rawName.trimmed().toLower();
    if (name.isEmpty()) {
        return false;
    }

    static const struct { const char* name; unsigned short vk; } kNames[] = {
        {"ctrl", 0xA2}, {"control", 0xA2}, {"shift", 0xA0}, {"alt", 0xA4},
        {"win", 0x5B}, {"windows", 0x5B}, {"super", 0x5B}, {"meta", 0x5B}, {"cmd", 0x5B},
        {"esc", 0x1B}, {"escape", 0x1B}, {"tab", 0x09}, {"enter", 0x0D}, {"return", 0x0D},
        {"space", 0x20}, {"backspace", 0x08}, {"delete", 0x2E}, {"del", 0x2E},
        {"insert", 0x2D}, {"ins", 0x2D}, {"home", 0x24}, {"end", 0x23},
        {"pageup", 0x21}, {"pgup", 0x21}, {"pagedown", 0x22}, {"pgdn", 0x22},
        {"left", 0x25}, {"up", 0x26}, {"right", 0x27}, {"down", 0x28},
        {"printscreen", 0x2C}, {"prtsc", 0x2C}, {"pause", 0x13}, {"capslock", 0x14},
        {"numlock", 0x90}, {"scrolllock", 0x91}, {"menu", 0x5D}, {"apps", 0x5D},
        {"plus", 0xBB}, {"=", 0xBB}, {"minus", 0xBD}, {"-", 0xBD},
        {"comma", 0xBC}, {",", 0xBC}, {"period", 0xBE}, {".", 0xBE},
        {"slash", 0xBF}, {"/", 0xBF}, {"semicolon", 0xBA}, {";", 0xBA},
        {"grave", 0xC0}, {"backtick", 0xC0}, {"`", 0xC0},
        {"backslash", 0xDC}, {"\\", 0xDC}, {"[", 0xDB}, {"]", 0xDD}, {"'", 0xDE},
    };
    for (const auto& entry : kNames) {
        if (name == QLatin1String(entry.name)) {
            vk = entry.vk;
            return true;
        }
    }

    // F1 - F24
    if (name.size() >= 2 && name[0] == 'f') {
        bool ok = false;
        int n = name.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24) {
            vk = (unsigned short)(0x70 + n - 1);
            return true;
        }
    }

    // A - Z and 0 - 9
    if (name.size() == 1) {
        QChar c = name[0];
        if (c >= 'a' && c <= 'z') {
            vk = (unsigned short)(0x41 + (c.unicode() - 'a'));
            return true;
        }
        if (c >= '0' && c <= '9') {
            vk = (unsigned short)(0x30 + (c.unicode() - '0'));
            return true;
        }
    }

    return false;
}

} // namespace

bool ShortcutMenu::parseKeyCombo(const QString& combo, std::vector<unsigned short>& out)
{
    out.clear();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QStringList parts = combo.split('+', Qt::SkipEmptyParts);
#else
    const QStringList parts = combo.split('+', QString::SkipEmptyParts);
#endif
    if (parts.isEmpty() || parts.size() > 6) {
        return false;
    }

    for (const QString& part : parts) {
        unsigned short vk;
        if (!lookupKeyName(part, vk)) {
            out.clear();
            return false;
        }
        out.push_back(vk);
    }
    return true;
}

QString ShortcutMenu::configFilePath()
{
    // Portable installs (portable.dat next to the exe) keep the file next to the program,
    // matching how the rest of the settings behave.
    QString dir;
    if (QFile::exists(QDir::currentPath() + "/portable.dat")) {
        dir = QDir::currentPath();
    }
    else {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }

    QDir().mkpath(dir);
    return QDir(dir).filePath("vibelight-shortcuts.json");
}

void ShortcutMenu::buildItems()
{
    m_Items.clear();
    m_ConfigProblem.clear();
    m_ColumnTitles.assign(3, std::string());
    m_ColumnTitles[0] = "Send to remote PC";
    m_ColumnTitles[1] = "Vibelight (Ctrl+Alt+Shift+key)";
    m_ColumnTitles[2] = "Displays & windows";

    // Windows virtual-key codes
    const unsigned short CTRL = 0xA2, SHIFT = 0xA0, ALT = 0xA4, WIN = 0x5B;

    auto remote = [this](const char* label, const char* hint, std::vector<unsigned short> keys) {
        Item item;
        item.label = label;
        item.hint = hint;
        item.kind = KindHostKeys;
        item.keys = keys;
        item.clientCombo = -1;
        item.dangerous = false;
        item.column = 0;
        item.rect = {0, 0, 0, 0};
        m_Items.push_back(item);
    };
    auto display = [this](const char* label, const char* hint, std::vector<unsigned short> keys) {
        Item item;
        item.label = label;
        item.hint = hint;
        item.kind = KindHostKeys;
        item.keys = keys;
        item.clientCombo = -1;
        item.dangerous = false;
        item.column = 2;
        item.rect = {0, 0, 0, 0};
        m_Items.push_back(item);
    };
    auto client = [this](const char* label, const char* hint, SdlInputHandler::KeyCombo combo, bool dangerous) {
        Item item;
        item.label = label;
        item.hint = hint;
        item.kind = KindClient;
        item.clientCombo = (int)combo;
        item.dangerous = dangerous;
        item.column = 1;
        item.rect = {0, 0, 0, 0};
        m_Items.push_back(item);
    };

    // Column 0: keys sent to the remote PC
    remote("Ctrl+Alt+Del",             "",              {CTRL, ALT, 0x2E});
    remote("Alt+Tab",                  "",              {ALT, 0x09});
    remote("Alt+F4",                   "",              {ALT, 0x73});
    remote("Task Manager",             "Ctrl+Shift+Esc",{CTRL, SHIFT, 0x1B});
    remote("Win (Start menu)",         "",              {WIN});
    remote("Show desktop",             "Win+D",         {WIN, 0x44});
    remote("File Explorer",            "Win+E",         {WIN, 0x45});
    remote("Lock PC",                  "Win+L",         {WIN, 0x4C});
    remote("Run",                      "Win+R",         {WIN, 0x52});
    remote("Task view",                "Win+Tab",       {WIN, 0x09});
    remote("Snipping tool",            "Win+Shift+S",   {WIN, SHIFT, 0x53});
    remote("Print Screen",             "",              {0x2C});

    // Column 1: Vibelight's own client-side shortcuts
    client("Release/grab mouse",       "Z", SdlInputHandler::KeyComboUngrabInput, false);
    client("Toggle fullscreen",        "X", SdlInputHandler::KeyComboToggleFullScreen, false);
    client("Performance stats",        "S", SdlInputHandler::KeyComboToggleStatsOverlay, false);
    client("Mouse mode",               "M", SdlInputHandler::KeyComboToggleMouseMode, false);
    client("Show/hide cursor",         "C", SdlInputHandler::KeyComboToggleCursorHide, false);
    client("Minimize",                 "D", SdlInputHandler::KeyComboToggleMinimize, false);
    client("Paste clipboard as typing","V", SdlInputHandler::KeyComboPasteText, false);
    client("Pointer region lock",      "L", SdlInputHandler::KeyComboTogglePointerRegionLock, false);
    client("System key capture",       "K", SdlInputHandler::KeyComboToggleKeyboardGrab, false);
    {
        Item item;
        item.label = "Check connection";
        item.hint = "10 s, suggests bitrate";
        item.kind = KindConnectionCheck;
        item.clientCombo = -1;
        item.dangerous = false;
        item.column = 1;
        item.rect = {0, 0, 0, 0};
        m_Items.push_back(item);
    }
    client("Quit stream",              "Q", SdlInputHandler::KeyComboQuit, true);
    client("Quit stream + exit app",   "E", SdlInputHandler::KeyComboQuitAndExit, true);

    // Column 2: display and multi-monitor shortcuts sent to the remote PC
    // (0x50 = P, 0x25 = Left arrow, 0x27 = Right arrow, 0x42 = B)
    display("Projection modes",         "Win+P",          {WIN, 0x50});
    display("Window to left monitor",   "Win+Shift+Left", {WIN, SHIFT, 0x25});
    display("Window to right monitor",  "Win+Shift+Right",{WIN, SHIFT, 0x27});
    display("Snap window left",         "Win+Left",       {WIN, 0x25});
    display("Snap window right",        "Win+Right",      {WIN, 0x27});
    display("Maximize window",          "Win+Up",         {WIN, 0x26});
    display("Reset graphics driver",    "Win+Ctrl+Shift+B",{WIN, CTRL, SHIFT, 0x42});

    // Columns 3+: the user's own shortcuts
    addCustomItems();

    // Column 0 (everything else moves right): pinned and recently used shortcuts
    addTopColumn();
}

void ShortcutMenu::addCustomItems()
{
    const QString path = configFilePath();

    // Create a starter file the first time so there is something to edit
    if (!QFile::exists(path)) {
        QFile starter(path);
        if (starter.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            starter.write(
                "{\n"
                "  \"_help\": \"Add your own shortcuts below. Each needs a \\\"label\\\" and either \\\"keys\\\" (for example \\\"Ctrl+Alt+T\\\") or \\\"text\\\" (typed on the remote PC). Optional: \\\"confirm\\\": true asks for a second click. Save the file, then press Ctrl+Alt+Shift+H again to see your changes.\",\n"
                "  \"shortcuts\": []\n"
                "}\n");
        }
    }

    std::vector<Item> custom;

    auto addInfo = [this, &custom](const std::string& message) {
        m_ConfigProblem = message;
        Item item;
        item.label = message;
        item.kind = KindInfo;
        item.clientCombo = -1;
        item.dangerous = true;
        item.column = kFirstCustomColumn;
        item.rect = {0, 0, 0, 0};
        custom.push_back(item);
    };

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        addInfo("Can't read custom shortcuts file");
    }
    else {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (doc.isNull() || !doc.isObject()) {
            addInfo("Custom shortcuts file isn't valid JSON");
        }
        else {
            const QJsonArray array = doc.object().value("shortcuts").toArray();
            for (const QJsonValue& value : array) {
                if (!value.isObject()) {
                    continue;
                }

                const QJsonObject obj = value.toObject();
                const QString label = obj.value("label").toString().trimmed();
                const QString keys = obj.value("keys").toString().trimmed();
                const QString text = obj.value("text").toString();
                if (label.isEmpty() || (keys.isEmpty() && text.isEmpty())) {
                    continue;
                }

                Item item;
                item.label = label.toStdString();
                item.clientCombo = -1;
                item.dangerous = obj.value("confirm").toBool(false);
                item.column = kFirstCustomColumn;
                item.rect = {0, 0, 0, 0};

                if (!keys.isEmpty()) {
                    if (!parseKeyCombo(keys, item.keys)) {
                        item.kind = KindInfo;
                        m_ConfigProblem = (label + " has an unknown key").toStdString();
                        item.label = (label + " (unknown key)").toStdString();
                        item.dangerous = true;
                    }
                    else {
                        item.kind = KindHostKeys;
                        item.hint = keys.toStdString();
                    }
                }
                else {
                    item.kind = KindHostText;
                    item.text = text.toStdString();
                    item.hint = "types text";
                }

                custom.push_back(item);
            }
        }
    }

    // Always offer a way to open the file
    {
        Item item;
        item.label = "Edit custom shortcuts...";
        item.hint = "opens file";
        item.kind = KindOpenConfig;
        item.clientCombo = -1;
        item.dangerous = false;
        item.column = kFirstCustomColumn;
        item.rect = {0, 0, 0, 0};
        custom.push_back(item);
    }

    // Flow the custom items into as many extra columns as needed
    const int maxItems = kMaxRowsPerColumn * kMaxCustomColumns;
    if ((int)custom.size() > maxItems) {
        // Keep the "Edit" item, drop the overflow
        Item edit = custom.back();
        custom.resize(maxItems - 1);
        custom.push_back(edit);
    }

    for (size_t i = 0; i < custom.size(); i++) {
        custom[i].column = kFirstCustomColumn + (int)(i / kMaxRowsPerColumn);
        m_Items.push_back(custom[i]);
    }

    const int customColumns = ((int)custom.size() + kMaxRowsPerColumn - 1) / kMaxRowsPerColumn;
    m_ColumnTitles.resize(kFirstCustomColumn + customColumns);
    for (int c = 0; c < customColumns; c++) {
        m_ColumnTitles[kFirstCustomColumn + c] = (c == 0) ? "Custom" : "Custom (cont.)";
    }
}

void ShortcutMenu::rebuildItems()
{
    // Re-read the custom shortcuts file so edits show up without restarting the stream
    buildItems();
    layoutItems();
}

std::string ShortcutMenu::itemId(const Item& item)
{
    char prefix = item.kind == KindHostKeys ? 'k' : (item.kind == KindHostText ? 't' : 'c');
    return std::string(1, prefix) + ":" + item.label;
}

void ShortcutMenu::addTopColumn()
{
    QSettings settings;
    const QStringList pins = settings.value("vibelight/pinnedShortcuts").toStringList();
    const QStringList recents = settings.value("vibelight/recentShortcuts").toStringList();

    auto findItem = [this](const QString& id) -> const Item* {
        for (const Item& item : m_Items) {
            if (item.kind != KindHostKeys && item.kind != KindHostText && item.kind != KindClient) {
                continue;
            }
            if (QString::fromStdString(itemId(item)) == id) {
                return &item;
            }
        }
        return nullptr;
    };

    std::vector<Item> top;
    for (const QString& id : pins) {
        if (const Item* found = findItem(id)) {
            Item copy = *found;
            copy.pinned = true;
            top.push_back(copy);
        }
    }

    int recentCount = 0;
    for (const QString& id : recents) {
        if (recentCount >= 4) {
            break;
        }
        if (pins.contains(id)) {
            continue;
        }
        if (const Item* found = findItem(id)) {
            top.push_back(*found);
            recentCount++;
        }
    }

    // Mark pinned shortcuts in their normal columns too
    for (Item& item : m_Items) {
        if (item.kind == KindHostKeys || item.kind == KindHostText || item.kind == KindClient) {
            item.pinned = pins.contains(QString::fromStdString(itemId(item)));
        }
    }

    if (top.empty()) {
        return;
    }

    for (Item& item : m_Items) {
        item.column++;
    }
    m_ColumnTitles.insert(m_ColumnTitles.begin(), "Pinned & recent");

    for (Item& item : top) {
        item.column = 0;
        item.rect = {0, 0, 0, 0};
        m_Items.push_back(item);
    }
}

void ShortcutMenu::togglePin(int index)
{
    if (index < 0 || index >= (int)m_Items.size()) {
        return;
    }

    const Item& item = m_Items[index];
    if (item.kind != KindHostKeys && item.kind != KindHostText && item.kind != KindClient) {
        return;
    }

    const QString id = QString::fromStdString(itemId(item));
    QSettings settings;
    QStringList pins = settings.value("vibelight/pinnedShortcuts").toStringList();
    if (pins.contains(id)) {
        pins.removeAll(id);
    }
    else if (pins.size() < 10) {
        pins.append(id);
    }
    else {
        showTransient("You can pin up to 10 shortcuts", 2000);
        return;
    }
    settings.setValue("vibelight/pinnedShortcuts", pins);

    // Rebuild so the pinned column appears/disappears, then resize the open menu
    m_HoverIndex = -1;
    m_ConfirmIndex = -1;
    rebuildItems();
    if (m_Expanded) {
        SDL_SetWindowSize(m_Window, m_ExpandedWidth, m_ExpandedHeight);
        updatePosition();
    }
    render();
}

void ShortcutMenu::recordRecent(const Item& item)
{
    const QString id = QString::fromStdString(itemId(item));
    QSettings settings;
    QStringList recents = settings.value("vibelight/recentShortcuts").toStringList();
    recents.removeAll(id);
    recents.prepend(id);
    while (recents.size() > 8) {
        recents.removeLast();
    }
    settings.setValue("vibelight/recentShortcuts", recents);
}

void ShortcutMenu::showTransient(const std::string& message, unsigned int durationMs)
{
    if (m_Window == nullptr || m_Expanded) {
        // Never disturb an open menu
        return;
    }

    if (m_TransientUntil == 0) {
        m_TransientOwnsReveal = !m_Revealed;
    }
    m_TransientMessage = message;
    m_TransientUntil = SDL_GetTicks() + durationMs;
    if (m_TransientUntil == 0) {
        m_TransientUntil = 1;
    }

    // Size the button to fit the message
    int textWidth = 0, textHeight = 0;
    if (TTF_SizeUTF8(m_Font, message.c_str(), &textWidth, &textHeight) != 0) {
        textWidth = (int)message.size() * 9;
    }
    SDL_SetWindowSize(m_Window, qMax(kCollapsedWidth, textWidth + 28), kHeaderHeight);

    m_Revealed = true;
    updatePosition();
    render();
}

void ShortcutMenu::endTransient()
{
    bool hide = m_TransientOwnsReveal;
    m_TransientUntil = 0;
    m_TransientOwnsReveal = false;
    m_TransientMessage.clear();

    if (m_Window == nullptr) {
        return;
    }

    if (!m_Expanded) {
        SDL_SetWindowSize(m_Window, kCollapsedWidth, kHeaderHeight);
    }

    if (hide && !m_Expanded) {
        m_Revealed = false;
    }
    updatePosition();
    render();
}

void ShortcutMenu::tick()
{
    if (m_CheckActive && (int)(SDL_GetTicks() - m_CheckEndsAt) >= 0) {
        finishConnectionCheck();
    }

    if (m_TransientUntil != 0 && (int)(SDL_GetTicks() - m_TransientUntil) >= 0) {
        endTransient();
    }

    // Redraw once per second while the button is visible so the stream timer advances
    if (m_Revealed && m_Shown) {
        int second = (int)((SDL_GetTicks() - m_StartTicks) / 1000);
        if (second != m_LastTimerSecond) {
            m_LastTimerSecond = second;
            render();
        }
    }
}

std::string ShortcutMenu::elapsedString() const
{
    unsigned int total = (SDL_GetTicks() - m_StartTicks) / 1000;
    unsigned int hours = total / 3600;
    unsigned int minutes = (total / 60) % 60;
    unsigned int seconds = total % 60;

    char buf[32];
    if (hours > 0) {
        SDL_snprintf(buf, sizeof(buf), "%u:%02u:%02u", hours, minutes, seconds);
    }
    else {
        SDL_snprintf(buf, sizeof(buf), "%02u:%02u", minutes, seconds);
    }
    return buf;
}

void ShortcutMenu::startConnectionCheck()
{
    Session* session = Session::get();
    CONNECTION_SNAPSHOT snapshot = {};
    if (session == nullptr || !session->getConnectionSnapshot(snapshot)) {
        showTransient("Connection check isn't available for this stream", 4000);
        return;
    }

    m_CheckStartFrames = snapshot.totalFrames;
    m_CheckStartDropped = snapshot.networkDroppedFrames;
    m_CheckEndsAt = SDL_GetTicks() + 10000;
    m_CheckActive = true;
    showTransient("Checking connection for 10 s...", 12000);
}

void ShortcutMenu::finishConnectionCheck()
{
    m_CheckActive = false;

    Session* session = Session::get();
    CONNECTION_SNAPSHOT snapshot = {};
    if (session == nullptr || !session->getConnectionSnapshot(snapshot)) {
        showTransient("Connection check failed", 4000);
        return;
    }

    unsigned int frames = snapshot.totalFrames - m_CheckStartFrames;
    unsigned int dropped = snapshot.networkDroppedFrames - m_CheckStartDropped;
    if (frames < 120) {
        // The stream was paused, minimized or too idle for a meaningful result
        showTransient("Not enough video to judge. Try again while something is moving", 6000);
        return;
    }

    double dropPercent = 100.0 * dropped / frames;
    double currentMbps = session->getConfiguredBitrateKbps() / 1000.0;
    double suggestedMbps = currentMbps;

    if (dropPercent >= 3.0) {
        suggestedMbps = currentMbps * 0.6;
    }
    else if (dropPercent >= 1.0) {
        suggestedMbps = currentMbps * 0.8;
    }
    else if (dropPercent < 0.3 && (snapshot.rttMs == 0 || snapshot.rttMs < 30) &&
             snapshot.videoMbps >= currentMbps * 0.8) {
        // Clean, and the stream is actually using what it was given: there may be room to grow
        suggestedMbps = qMin(currentMbps * 1.25, 150.0);
    }
    suggestedMbps = qMax(5.0, (double)(int)(suggestedMbps + 0.5));

    char buf[160];
    if (dropPercent >= 1.0) {
        SDL_snprintf(buf, sizeof(buf), "Dropping %.1f%% of frames. Try %d Mbps (now %d)",
                     dropPercent, (int)suggestedMbps, (int)(currentMbps + 0.5));
    }
    else if (suggestedMbps > currentMbps + 0.5) {
        SDL_snprintf(buf, sizeof(buf), "Connection is clean (%.1f%% dropped). Could try %d Mbps (now %d)",
                     dropPercent, (int)suggestedMbps, (int)(currentMbps + 0.5));
    }
    else {
        SDL_snprintf(buf, sizeof(buf), "Connection OK (%.1f%% dropped). %d Mbps is a good fit",
                     dropPercent, (int)(currentMbps + 0.5));
    }

    // Remember the suggestion so the Settings page can offer it
    QSettings settings;
    settings.setValue("vibelight/suggestedBitrateKbps", (int)suggestedMbps * 1000);

    showTransient(buf, 10000);
}

void ShortcutMenu::showHint()
{
    if (m_Window == nullptr) {
        return;
    }

    if (!m_ConfigProblem.empty()) {
        showTransient("Custom shortcuts: " + m_ConfigProblem, 6000);
        return;
    }

    // Only remind the first few times so it doesn't get annoying
    QSettings settings;
    int shown = settings.value("vibelight/hintShownCount", 0).toInt();
    if (shown >= 3) {
        return;
    }
    settings.setValue("vibelight/hintShownCount", shown + 1);
    showTransient("Press Ctrl+Alt+Shift+H for shortcuts", 5000);
}

void ShortcutMenu::layoutItems()
{
    m_NumColumns = 1;
    for (const Item& item : m_Items) {
        m_NumColumns = qMax(m_NumColumns, item.column + 1);
    }

    std::vector<int> rowY(m_NumColumns, kHeaderHeight + kPadding + kTitleHeight);
    int tallest = rowY[0];

    for (Item& item : m_Items) {
        item.rect.x = kPadding + item.column * (kColumnWidth + kPadding);
        item.rect.y = rowY[item.column];
        item.rect.w = kColumnWidth;
        item.rect.h = kRowHeight;
        rowY[item.column] += kRowHeight;
        tallest = qMax(tallest, rowY[item.column]);
    }

    m_ExpandedWidth = kPadding * (m_NumColumns + 1) + kColumnWidth * m_NumColumns;
    m_ExpandedHeight = tallest + kPadding;
}

void ShortcutMenu::showWithoutActivation()
{
#ifdef SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
#endif
    SDL_ShowWindow(m_Window);
}

void ShortcutMenu::overrideMinimizeOnFocusLoss(bool enable)
{
    // While the pointer is over our window, a click will move keyboard focus away from the
    // stream window. SDL would normally minimize a full-screen window in that case, which
    // is the last thing we want when the user is just clicking a shortcut.
    if (enable == m_HintOverridden) {
        return;
    }

    if (enable) {
        const char* old = SDL_GetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS);
        m_SavedHint = old != nullptr ? SDL_strdup(old) : nullptr;
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    }
    else {
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, m_SavedHint);
        SDL_free(m_SavedHint);
        m_SavedHint = nullptr;
    }
    m_HintOverridden = enable;
}

void ShortcutMenu::updatePosition()
{
    if (m_Window == nullptr) {
        return;
    }

    if (!m_Revealed) {
        if (m_Shown) {
            SDL_HideWindow(m_Window);
            m_Shown = false;
        }
        return;
    }

    Uint32 parentFlags = SDL_GetWindowFlags(m_Parent);
    if (parentFlags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
        if (m_Shown) {
            SDL_HideWindow(m_Window);
            m_Shown = false;
        }
        return;
    }

    int px, py, pw, ph, w, h;
    SDL_GetWindowPosition(m_Parent, &px, &py);
    SDL_GetWindowSize(m_Parent, &pw, &ph);
    SDL_GetWindowSize(m_Window, &w, &h);

    int x, y;
    if (m_HasCustomPosition) {
        x = px + m_OffsetX;
        y = py + m_OffsetY;
    }
    else {
        x = px + (pw - w) / 2;
        y = py + kDefaultTopMargin;
    }

    // Keep the whole menu inside the stream window so it can't get lost off-screen
    // when the stream window is resized or moved to another display.
    x = qMax(px, qMin(x, px + pw - w));
    y = qMax(py, qMin(y, py + ph - h));

    int cx, cy;
    SDL_GetWindowPosition(m_Window, &cx, &cy);
    if (cx != x || cy != y) {
        SDL_SetWindowPosition(m_Window, x, y);
    }

    if (!m_Shown) {
        showWithoutActivation();
        m_Shown = true;

        // Make sure the stream window keeps focus after we appear
        SDL_RaiseWindow(m_Parent);
        render();
    }

#if SDL_VERSION_ATLEAST(2, 0, 16)
    // Re-stack above the (possibly topmost, full-screen) stream window without activating
    SDL_SetWindowAlwaysOnTop(m_Window, SDL_TRUE);
#endif
}

void ShortcutMenu::setExpanded(bool expanded)
{
    if (expanded && m_TransientUntil != 0) {
        // The user opened the menu: keep the button, drop the message
        m_TransientUntil = 0;
        m_TransientOwnsReveal = false;
        m_TransientMessage.clear();
    }

    m_Expanded = expanded;
    m_HoverIndex = -1;
    m_ConfirmIndex = -1;

    if (expanded) {
        SDL_SetWindowSize(m_Window, m_ExpandedWidth, m_ExpandedHeight);
    }
    else {
        SDL_SetWindowSize(m_Window, kCollapsedWidth, kHeaderHeight);
    }

    updatePosition();
    render();
}

void ShortcutMenu::close(bool refocusStream)
{
    if (m_Expanded) {
        setExpanded(false);
    }

    if (!m_HeaderHover) {
        overrideMinimizeOnFocusLoss(false);
    }

    if (refocusStream) {
        SDL_RaiseWindow(m_Parent);
    }

    if (m_RecaptureOnClose) {
        m_RecaptureOnClose = false;
        m_Input->setCaptureActive(true);
    }
}

void ShortcutMenu::dismiss(bool refocusStream)
{
    // Collapse the dropdown, give the mouse back, and hide the button again
    close(refocusStream);

    endDrag();
    m_HeaderHover = false;
    m_Revealed = false;
    m_TransientUntil = 0;
    m_TransientOwnsReveal = false;
    if (!m_Expanded) {
        SDL_SetWindowSize(m_Window, kCollapsedWidth, kHeaderHeight);
    }
    updatePosition();
    overrideMinimizeOnFocusLoss(false);
}

void ShortcutMenu::toggleFromHotkey()
{
    if (m_Window == nullptr) {
        return;
    }

    if (m_Revealed && m_TransientUntil == 0) {
        // Pressing the hotkey again hides the button
        dismiss(true);
        return;
    }

    // A transient message is showing: the hotkey turns it into the full menu
    m_TransientUntil = 0;
    m_TransientOwnsReveal = false;
    m_TransientMessage.clear();

    // Pick up any edits to the custom shortcuts file
    rebuildItems();

    m_Revealed = true;

    // Free the mouse so the user can actually reach the menu
    m_RecaptureOnClose = m_Input->isCaptureActive();
    if (m_RecaptureOnClose) {
        m_Input->setCaptureActive(false);
    }

    overrideMinimizeOnFocusLoss(true);
    setExpanded(true);
    SDL_RaiseWindow(m_Window);
}

void ShortcutMenu::execute(const Item& itemRef)
{
    // Copy what we need, since dismiss() may trigger changes
    Item item = itemRef;

    if (item.kind == KindInfo) {
        return;
    }

    if (item.kind == KindOpenConfig) {
        // Hide the button, then open the file in whatever editor the OS prefers
        dismiss(false);
        QDesktopServices::openUrl(QUrl::fromLocalFile(configFilePath()));
        return;
    }

    // Hide the button and give focus (and mouse capture, if we took it) back before
    // acting so that the action sees the same state the user had.
    dismiss(true);

    if (item.kind == KindConnectionCheck) {
        startConnectionCheck();
        return;
    }

    switch (item.kind) {
    case KindClient:
        m_Input->performSpecialKeyCombo((SdlInputHandler::KeyCombo)item.clientCombo);
        break;
    case KindHostKeys:
        m_Input->sendHostKeyCombo(item.keys.data(), (int)item.keys.size());
        break;
    case KindHostText:
        // Make sure no held keys interfere with the text being typed
        m_Input->raiseAllKeys();
        LiSendUtf8TextEvent(item.text.c_str(), (unsigned int)item.text.size());
        break;
    default:
        break;
    }

    // Confirm what happened, except for actions that end the stream
    if (!(item.kind == KindClient && item.dangerous)) {
        recordRecent(item);
        showTransient((item.kind == KindHostText ? "Typed: " : "Sent: ") + item.label, 2000);
    }
}

int ShortcutMenu::hitTest(int x, int y) const
{
    if (!m_Expanded) {
        return -1;
    }

    for (size_t i = 0; i < m_Items.size(); i++) {
        if (pointInRect(m_Items[i].rect, x, y)) {
            return (int)i;
        }
    }
    return -1;
}

void ShortcutMenu::handleClick(int x, int y)
{
    if (y < kHeaderHeight) {
        if (m_Expanded) {
            close(true);
        }
        else {
            m_RecaptureOnClose = false;
            setExpanded(true);
        }
        return;
    }

    int index = hitTest(x, y);
    if (index < 0) {
        return;
    }

    const Item& item = m_Items[index];
    if (item.kind == KindInfo) {
        return;
    }
    if (item.dangerous && m_ConfirmIndex != index) {
        // Require a second click for destructive actions
        m_ConfirmIndex = index;
        render();
        return;
    }

    execute(item);
}

void ShortcutMenu::beginDrag(int localX, int localY)
{
    m_MouseDown = true;
    m_Dragging = false;
    m_GrabLocalX = localX;
    m_GrabLocalY = localY;
    SDL_GetGlobalMouseState(&m_DownGlobalX, &m_DownGlobalY);

    // Keep receiving motion/button events even if the pointer leaves our small window
    SDL_CaptureMouse(SDL_TRUE);
}

void ShortcutMenu::updateDrag()
{
    int gx, gy;
    SDL_GetGlobalMouseState(&gx, &gy);

    if (!m_Dragging) {
        if (qAbs(gx - m_DownGlobalX) < kDragThresholdPx && qAbs(gy - m_DownGlobalY) < kDragThresholdPx) {
            return;
        }
        m_Dragging = true;
    }

    int px, py;
    SDL_GetWindowPosition(m_Parent, &px, &py);

    m_HasCustomPosition = true;
    m_OffsetX = gx - m_GrabLocalX - px;
    m_OffsetY = gy - m_GrabLocalY - py;
    updatePosition();
}

void ShortcutMenu::endDrag()
{
    if (m_MouseDown) {
        SDL_CaptureMouse(SDL_FALSE);
    }
    m_MouseDown = false;
    m_Dragging = false;
}

bool ShortcutMenu::handleEvent(const SDL_Event& event)
{
    if (m_Window == nullptr) {
        return false;
    }

    const Uint32 id = SDL_GetWindowID(m_Window);

    // Esc closes the open menu no matter which window has the keyboard focus, and the
    // key press never reaches the remote PC.
    if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.keysym.sym == SDLK_ESCAPE) {
        if (event.type == SDL_KEYDOWN && m_Expanded) {
            dismiss(true);
            m_SwallowEscUp = true;
            return true;
        }
        if (m_SwallowEscUp) {
            if (event.type == SDL_KEYUP) {
                m_SwallowEscUp = false;
            }
            return true;
        }
    }

    switch (event.type) {
    case SDL_WINDOWEVENT:
        if (event.window.windowID != id) {
            return false;
        }

        switch (event.window.event) {
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            render();
            break;
        case SDL_WINDOWEVENT_ENTER:
            overrideMinimizeOnFocusLoss(true);
            break;
        case SDL_WINDOWEVENT_LEAVE:
            m_HoverIndex = -1;
            m_HeaderHover = false;
            if (!m_Expanded && !m_MouseDown) {
                overrideMinimizeOnFocusLoss(false);
            }
            render();
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            // Clicking somewhere else closes the menu and hides the button again
            if (m_Expanded && !m_MouseDown) {
                m_RecaptureOnClose = false;
                dismiss(false);
            }
            break;
        }
        return true;

    case SDL_MOUSEMOTION:
        if (event.motion.windowID != id) {
            return false;
        }

        if (m_MouseDown) {
            updateDrag();
        }
        else {
            int hover = hitTest(event.motion.x, event.motion.y);
            bool headerHover = event.motion.y < kHeaderHeight;
            if (hover != m_HoverIndex || headerHover != m_HeaderHover) {
                m_HoverIndex = hover;
                m_HeaderHover = headerHover;
                render();
            }
        }
        return true;

    case SDL_MOUSEBUTTONDOWN:
        if (event.button.windowID != id) {
            return false;
        }
        if (event.button.button == SDL_BUTTON_RIGHT && m_Expanded && event.button.y >= kHeaderHeight) {
            // Right-click pins or unpins a shortcut
            togglePin(hitTest(event.button.x, event.button.y));
        }
        else if (event.button.button == SDL_BUTTON_LEFT) {
            if (event.button.y < kHeaderHeight) {
                // Might become a drag or a click; decided on button up
                beginDrag(event.button.x, event.button.y);
            }
            else {
                handleClick(event.button.x, event.button.y);
            }
        }
        return true;

    case SDL_MOUSEBUTTONUP:
        if (event.button.windowID != id) {
            return false;
        }
        if (event.button.button == SDL_BUTTON_LEFT && m_MouseDown) {
            bool wasDragging = m_Dragging;
            endDrag();
            if (!wasDragging) {
                handleClick(event.button.x, event.button.y);
            }
        }
        return true;

    case SDL_MOUSEWHEEL:
        return event.wheel.windowID == id;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (event.key.windowID != id) {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && m_Expanded) {
            dismiss(true);
        }
        return true;

    default:
        return false;
    }
}

void ShortcutMenu::drawText(SDL_Surface* dst, TTF_Font* font, const char* text, SDL_Color color,
                            int x, int y, bool rightAlign)
{
    if (text == nullptr || text[0] == 0) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == nullptr) {
        return;
    }

    SDL_Rect dstRect = { rightAlign ? x - surface->w : x, y, surface->w, surface->h };
    SDL_BlitSurface(surface, nullptr, dst, &dstRect);
    SDL_FreeSurface(surface);
}

void ShortcutMenu::drawChevron(SDL_Surface* dst, int cx, int cy, bool up, Uint32 color)
{
    for (int i = 0; i < 5; i++) {
        int half = up ? i : 4 - i;
        SDL_Rect row = { cx - half, cy - 2 + i, half * 2 + 1, 1 };
        SDL_FillRect(dst, &row, color);
    }
}

void ShortcutMenu::render()
{
    if (m_Window == nullptr || !m_Shown) {
        return;
    }

    SDL_Surface* s = SDL_GetWindowSurface(m_Window);
    if (s == nullptr) {
        return;
    }

    Uint32 border = SDL_MapRGB(s->format, 0x1E, 0x26, 0x40);
    Uint32 background = SDL_MapRGB(s->format, 0x0D, 0x14, 0x24);
    Uint32 headerColor = m_HeaderHover ? SDL_MapRGB(s->format, 0x7A, 0x7D, 0xF5) : SDL_MapRGB(s->format, 0x63, 0x66, 0xF1);
    Uint32 hoverColor = SDL_MapRGB(s->format, 0x1A, 0x23, 0x40);
    Uint32 chevronColor = SDL_MapRGB(s->format, 0xFF, 0xFF, 0xFF);

    SDL_FillRect(s, nullptr, border);
    SDL_Rect inner = { 1, 1, s->w - 2, s->h - 2 };
    SDL_FillRect(s, &inner, background);

    // Header / button (also the drag handle)
    SDL_Rect header = { 1, 1, s->w - 2, kHeaderHeight - 2 };
    SDL_FillRect(s, &header, headerColor);
    bool showMessage = (m_TransientUntil != 0 && !m_Expanded);
    drawText(s, m_Font, showMessage ? m_TransientMessage.c_str() : "Shortcuts", kHeaderTextColor, 12,
             (kHeaderHeight - TTF_FontHeight(m_Font)) / 2);
    if (!showMessage) {
        drawChevron(s, s->w - 16, kHeaderHeight / 2, m_Expanded, chevronColor);

        // Time since the stream started, just right of the label
        int labelWidth = 0, labelHeight = 0;
        if (TTF_SizeUTF8(m_Font, "Shortcuts", &labelWidth, &labelHeight) != 0) {
            labelWidth = 85;
        }
        drawText(s, m_SmallFont, elapsedString().c_str(), kHeaderTextColor, 12 + labelWidth + 10,
                 (kHeaderHeight - TTF_FontHeight(m_SmallFont)) / 2);
    }
    if (m_Expanded) {
        drawText(s, m_SmallFont, "right-click to pin  |  Esc to close", kHeaderTextColor,
                 s->w - 34, (kHeaderHeight - TTF_FontHeight(m_SmallFont)) / 2, true);
    }

    if (m_Expanded) {
        for (int c = 0; c < m_NumColumns && c < (int)m_ColumnTitles.size(); c++) {
            drawText(s, m_SmallFont, m_ColumnTitles[c].c_str(), kTitleColor,
                     kPadding + c * (kColumnWidth + kPadding) + 4,
                     kHeaderHeight + kPadding + (kTitleHeight - TTF_FontHeight(m_SmallFont)) / 2);
        }

        for (size_t i = 0; i < m_Items.size(); i++) {
            const Item& item = m_Items[i];

            if ((int)i == m_HoverIndex && item.kind != KindInfo) {
                SDL_FillRect(s, const_cast<SDL_Rect*>(&item.rect), hoverColor);
            }

            bool confirming = ((int)i == m_ConfirmIndex);
            SDL_Color color = item.dangerous ? kDangerColor : kTextColor;
            std::string labelText = item.pinned ? "* " + item.label : item.label;
            const char* label = confirming ? "Click again to confirm" : labelText.c_str();

            drawText(s, m_Font, label, color,
                     item.rect.x + 6, item.rect.y + (item.rect.h - TTF_FontHeight(m_Font)) / 2);
            if (!confirming) {
                drawText(s, m_SmallFont, item.hint.c_str(), kHintColor,
                         item.rect.x + item.rect.w - 6,
                         item.rect.y + (item.rect.h - TTF_FontHeight(m_SmallFont)) / 2, true);
            }
        }
    }

    SDL_UpdateWindowSurface(m_Window);
}
