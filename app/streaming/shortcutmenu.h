#pragma once

#include "SDL_compat.h"
#include <SDL_ttf.h>

#include <QByteArray>
#include <QString>
#include <string>
#include <vector>

class SdlInputHandler;

// A small, free-floating, always-on-top "Shortcuts" button that lives in its own
// borderless SDL window next to the stream window. It stays hidden until the user
// presses Ctrl+Alt+Shift+H, and hides again when H is pressed again or an action is chosen. Clicking it opens a dropdown of
// clickable shortcuts (keys sent to the remote PC, plus Moonlight's own
// Ctrl+Alt+Shift+<key> actions). The button can be dragged anywhere on screen.
//
// All methods must be called from the SDL main (event loop) thread.
class ShortcutMenu
{
public:
    ShortcutMenu(SDL_Window* parentWindow, SdlInputHandler* inputHandler);
    ~ShortcutMenu();

    static bool isSupportedOnThisPlatform();

    bool isValid() const { return m_Window != nullptr; }

    // Returns true if the event belongs to the menu's window and has been consumed.
    bool handleEvent(const SDL_Event& event);

    // Re-apply position/visibility after the stream window moved, resized, minimized, etc.
    void updatePosition();

    // Show/hide the button from the keyboard (also frees the mouse if it was captured).
    void toggleFromHotkey();

    // Briefly shows how to reach the menu when a stream starts (the first few times),
    // or warns about a problem in the custom shortcuts file.
    void showHint();

    // Call regularly from the event loop; hides transient messages when they expire.
    void tick();
    bool needsTick() const { return m_TransientUntil != 0 || m_CheckActive || m_Revealed || m_ScreenshotAt != 0 || m_GraphEnabled; }

private:
    enum Kind {
        KindHostKeys,   // press and release a chord on the remote PC
        KindHostText,   // type a string on the remote PC
        KindClient,     // one of Vibelight's own Ctrl+Alt+Shift+key actions
        KindOpenConfig, // open the custom shortcuts file in the default editor
        KindConnectionCheck, // measure the stream for 10 seconds and suggest a bitrate
        KindClipboard,  // type one of the recent local clipboard entries on the remote PC
        KindClearClips, // forget the clipboard history
        KindScreenshot, // save the current stream picture as a PNG
        KindToggleGraph, // show/hide the small latency graph
        KindInfo        // non-clickable message (for example a config error)
    };

    struct Item {
        std::string label;
        std::string hint;
        Kind kind;
        std::vector<unsigned short> keys; // Windows VK codes for KindHostKeys
        std::string text;                 // for KindHostText
        int clientCombo;                  // SdlInputHandler::KeyCombo for KindClient
        bool dangerous;
        int column;
        SDL_Rect rect;
        bool pinned = false;
    };

    void buildItems();
    void addCustomItems();
    void rebuildItems();
    static QString configFilePath();
    static bool parseKeyCombo(const QString& combo, std::vector<unsigned short>& out);
    void layoutItems();
    void addTopColumn();
    static std::string itemId(const Item& item);
    void togglePin(int index);
    void recordRecent(const Item& item);
    void showTransient(const std::string& message, unsigned int durationMs);

    // Clipboard history (kept in memory only, never written to disk)
    void pollClipboard();
    void addClipboardColumn();

    // Screenshot of the stream (Windows)
    void captureScreenshot();

    // Small latency graph in its own click-through window (Windows)
    void toggleGraph();
    void updateGraphWindow();
    void destroyGraphWindow();
    void sampleGraph();
    void renderGraph();
    void startConnectionCheck();
    void finishConnectionCheck();
    std::string elapsedString() const;
    void endTransient();
    void setExpanded(bool expanded);
    void close(bool refocusStream);
    void dismiss(bool refocusStream);
    void execute(const Item& item);
    int hitTest(int x, int y) const;
    void handleClick(int x, int y);

    void render();
    void drawText(SDL_Surface* dst, TTF_Font* font, const char* text, SDL_Color color,
                  int x, int y, bool rightAlign = false);
    void drawChevron(SDL_Surface* dst, int cx, int cy, bool up, Uint32 color);

    void showWithoutActivation();
    void overrideMinimizeOnFocusLoss(bool enable);

    void beginDrag(int localX, int localY);
    void updateDrag();
    void endDrag();

    SDL_Window* m_Parent;
    SdlInputHandler* m_Input;
    SDL_Window* m_Window;
    TTF_Font* m_Font;
    TTF_Font* m_SmallFont;
    QByteArray m_FontData;
    bool m_TtfInitialized;

    std::vector<Item> m_Items;
    std::vector<std::string> m_ColumnTitles;
    int m_NumColumns;
    int m_ExpandedWidth;
    int m_ExpandedHeight;

    bool m_Expanded;
    bool m_Shown;
    bool m_Revealed; // The button is only visible after Ctrl+Alt+Shift+H
    int m_HoverIndex;
    bool m_HeaderHover;
    int m_ConfirmIndex;
    bool m_RecaptureOnClose;

    // Position of the menu window relative to the stream window's top-left.
    // Until the user drags the button it is docked at the top center.
    bool m_HasCustomPosition;
    int m_OffsetX;
    int m_OffsetY;

    // Drag state
    bool m_MouseDown;
    bool m_Dragging;
    int m_DownGlobalX, m_DownGlobalY;
    int m_GrabLocalX, m_GrabLocalY;

    // A short message shown in place of the button label ("Sent: Alt+Tab"). The button
    // is revealed just for the message when it was hidden before.
    unsigned int m_TransientUntil; // SDL tick deadline, 0 when no message is showing
    std::string m_TransientMessage;
    bool m_TransientOwnsReveal;
    bool m_SwallowEscUp;
    unsigned int m_ExpandedAt; // SDL tick when the menu last opened

    // Stream timer shown next to the button label
    unsigned int m_StartTicks;
    int m_LastTimerSecond;

    // Connection check state
    bool m_CheckActive;
    unsigned int m_CheckEndsAt;
    unsigned int m_CheckStartFrames;
    unsigned int m_CheckStartDropped;
    std::string m_ConfigProblem;

    bool m_HintOverridden;
    char* m_SavedHint;

    // Host summary shown in the open menu's header ("PC name | 1920x1080@60 | HEVC")
    std::string m_HostInfo;

    // Clipboard history, newest first
    std::vector<std::string> m_ClipHistory;
    unsigned int m_LastClipPoll = 0;

    // Screenshot scheduled for this SDL tick (0 = none), so the menu can get out of the picture
    unsigned int m_ScreenshotAt = 0;

    // Latency graph
    bool m_GraphEnabled = false;
    SDL_Window* m_GraphWindow = nullptr;
    bool m_GraphShown = false;
    unsigned int m_GraphHiddenUntil = 0;
    unsigned int m_GraphLastSample = 0;
    bool m_GraphHavePrev = false;
    unsigned int m_GraphPrevFrames = 0;
    unsigned int m_GraphPrevDropped = 0;
    std::vector<int> m_GraphRtt;
    std::vector<float> m_GraphDrop;
};
