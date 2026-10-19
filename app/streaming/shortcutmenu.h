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
    bool needsTick() const { return m_TransientUntil != 0; }

private:
    enum Kind {
        KindHostKeys,   // press and release a chord on the remote PC
        KindHostText,   // type a string on the remote PC
        KindClient,     // one of Vibelight's own Ctrl+Alt+Shift+key actions
        KindOpenConfig, // open the custom shortcuts file in the default editor
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
    std::string m_ConfigProblem;

    bool m_HintOverridden;
    char* m_SavedHint;
};
