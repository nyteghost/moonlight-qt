# Vibelight

Vibelight is a fork of [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt), the open source PC client for NVIDIA GameStream and [Sunshine](https://github.com/LizardByte/Sunshine). All of the streaming, decoding and input work is Moonlight's. This fork adds a few conveniences for remote-desktop use and renames the app.

> **This is an unofficial fork.** It is not affiliated with or endorsed by the Moonlight project. Please don't report problems with this fork to the upstream project. Upstream is licensed under GPLv3, and so is this fork (see [LICENSE](LICENSE)).

## Quick start
1. Download the latest build (see [Downloads](#downloads)) and unzip it. Add an empty file named `portable.dat` next to `Vibelight.exe` if you want settings kept in that folder.
2. Pair with your host the same way as in Moonlight and start a stream.
3. While streaming, press **Ctrl+Alt+Shift+H** to open the shortcuts menu. Press Esc to close it.
4. If something goes wrong, open **Settings → Support → Save diagnostics file** and send the file along with a description of what happened.

## Recommended host: Vibebeam
Vibelight works with any Sunshine or GameStream host, but some features need a host that supports Apollo-style extras. The companion host is [Vibebeam](https://github.com/nyteghost/vibebeam), a fork of [Vibepollo](https://github.com/Nonary/Vibepollo) (itself based on Apollo and Sunshine). It is kept in step with Nonary's Vibepollo by a scheduled sync that opens a pull request when upstream changes, and its update checks use its own releases. Features that need such a host today: **Clipboard sync** (allow clipboard access for Vibelight in the host's web UI). Everything else works with stock Sunshine.

## What's different from upstream Moonlight

### Floating "Shortcuts" button
While streaming, the small round **Shortcuts button** is hidden. Press **Ctrl+Alt+Shift+H** (you can change the H in **Settings → Input Settings → Shortcuts menu hotkey**; it applies from the next stream) to show it, open its menu and free the mouse. Press the hotkey again, press Esc, click outside the menu, or pick a shortcut to close the menu and hide the button again. If you would rather the button always stay on top of the stream, turn off **Settings → Input Settings → Hide the shortcuts button until I press the hotkey**: you then click the button to open the menu, and the hotkey hides and shows it. The button wears the Vibelight icon (coral disc, crescent moon and sparkles). The menu opens on a short **top level** with your pinned and recent shortcuts and a list of categories (Remote PC keys, Displays & windows, Vibelight controls, Tools & session, My shortcuts, Clipboard history). Click a category to open its page and click **Back** (or press Esc) to return; Esc on the top level closes the menu. Drag the button anywhere; Vibelight remembers where you put it. Opening the menu also frees the mouse, which is needed in relative mouse mode (to click the button while the mouse is captured, hold Ctrl+Alt+Z first). Esc is never sent to the remote PC while the menu is open.

**Hold Ctrl+Alt+Z to release the mouse.** While you hold the left Ctrl and left Alt keys and press Z, the mouse is freed so you can move it outside the stream window. Let go and it is captured again (if the pointer is still over the window). The Z is not sent to the remote PC, and Ctrl+Alt+Shift shortcuts (including Ctrl+Alt+Shift+Z, the release/grab toggle) work the same. Turn it off in **Settings → Input Settings** ("Hold Ctrl+Alt+Z to release the mouse").

**What's in the menu**
- **Send to remote PC:** Ctrl+Alt+Del, Alt+Tab, Alt+F4, Task Manager (Ctrl+Shift+Esc), Win, Win+D, Win+E, Win+L, Win+R, Win+Tab, Win+Shift+S, Print Screen.
- **Vibelight (Ctrl+Alt+Shift+key actions):** release/grab mouse (Z), fullscreen (X), performance stats (S), mouse mode (M), show/hide cursor (C), minimize (D), paste clipboard as typing (V), pointer region lock (L), system key capture (K), quit stream (Q), quit stream and exit app (E). The two quit actions need a second click to confirm.
- **Displays & windows (sent to the remote PC):** projection modes (Win+P), move a window to the left or right monitor (Win+Shift+Left/Right), snap left/right (Win+Left/Right), maximize (Win+Up), reset the graphics driver (Win+Ctrl+Shift+B). These change windows and display modes on the remote PC. Choosing which monitor is *streamed* is a host setting (Sunshine/Vibepollo), not something the client can switch.
- **Pinned & recent:** right-click any shortcut to pin it (up to 10, marked with `*`). Pinned shortcuts and your four most recently used ones appear in a column on the left.
- **Custom shortcuts:** your own buttons on their own page, up to 28 entries (see below).
- **Typing mode:** how "Paste clipboard as typing" (Ctrl+Alt+Shift+V), clipboard history entries and your custom text shortcuts reach the remote PC. **Unicode** (the default) sends the text itself, which works on Windows hosts in any keyboard layout. **Key presses** sends real key events for US-keyboard characters, which is what to use for terminals and Linux hosts, where Unicode input can be ignored or trigger shortcuts like GTK Inspector. It assumes a US layout on the host, and characters with no key (accents, emoji) are still sent as Unicode. Pick it in the menu under Vibelight controls → Typing mode (a small page with the two choices). The choice is remembered separately for each PC; PCs you haven't set yet use Unicode.
- **Clipboard sync (text, both ways):** under **Clipboard** in the menu, **Send my clipboard to host** puts this PC's clipboard text on the host's clipboard, and **Get host clipboard** copies the host's clipboard text to this PC, so you can paste with a normal Ctrl+V on either side. It uses the clipboard feature of Apollo-style hosts such as **Vibepollo**; it won't work with stock Sunshine or GeForce Experience. On the host, allow clipboard access for this client in the host's web UI (the client's permissions). Text only; files can't be sent this way yet.
- **Clipboard history:** the last 10 things you copied on this PC, as a column on the right. Click one to type it on the remote PC. The list is kept in memory only, is never saved to disk, and is gone when the stream ends. **Clear history** empties it right away. Anything you copy while streaming can appear here, including passwords, so clear it if that matters.

**Feedback and tools**
- **Hint:** the first three streams, the button appears for a few seconds saying how to open it. If your custom shortcuts file has a problem, you get a short warning at the start of every stream.
- **"Sent" message:** after you choose a shortcut, the button briefly shows what was sent.
- **Stream timer:** the button shows how long the stream has been running.
- **Version:** the bottom-right corner of the open menu shows the Vibelight version and build, for example `Vibelight 6.2.0 (80f7297)`, so you can tell which build you're running without opening Settings.
- **Host info:** the open menu's header shows the PC name, stream resolution and frame rate, and codec (for example `DESKTOP-PC  |  1920x1080@60  |  HEVC 10-bit`) when there is room.
- **Screenshot of stream (Windows):** saves a PNG of the stream window to `Pictures\Vibelight`. The menu and graph hide first so they aren't in the picture. It copies what is on screen, so exclusive fullscreen or protected video can come out black.
- **Latency graph (Windows):** a small click-through panel in the bottom-left corner with the last 60 seconds of round-trip time (blue line) and dropped frames (red bars). Toggle it from the menu; it is off by default.
- **Check connection:** watches the stream for 10 seconds, reports how many frames the network dropped, and suggests a bitrate: lower if frames are being lost, higher if the connection is clean and the stream is using all the bitrate it's given, otherwise "good fit". This measures your real stream rather than running a speed test, so run it while something on screen is moving. The suggestion is saved, and Settings shows "Your last connection check suggested N Mbps" with a **Use** button under the bitrate slider.

**Notes and limitations**
- The button is available on Windows, macOS and X11. It is not shown on Wayland, KMS/DRM or Steam Link. Set the environment variable `ML_DISABLE_SHORTCUT_MENU` to turn it off.
- Windows may block injected Ctrl+Alt+Del depending on the host software.
- With every column in use, the menu is up to about 1,800 pixels wide, which can be cut off on smaller screens.
- This feature is new and has had limited testing, especially in exclusive fullscreen.

### Auto-reconnect
If the connection drops in the middle of a stream (a Wi-Fi blip, the host stalling), Vibelight shows "Connection lost. Reconnecting..." and tries up to 3 times to resume the stream instead of returning to the PC list. If every attempt fails you get the usual error. It never triggers when you quit the stream yourself or the host ends the session, or for host-side problems like encoder errors. Turn it off with **Settings → Host Settings → Automatically reconnect if the stream drops**.

### Custom shortcuts
Click **Edit custom shortcuts...** in the dropdown (or edit the file yourself) to add your own buttons. The file is `vibelight-shortcuts.json`. It lives in the app's config folder, or next to `Vibelight.exe` in portable mode. It is created the first time you open the menu. Changes show up the next time you press Ctrl+Alt+Shift+H, with no restart.

```json
{
  "shortcuts": [
    { "label": "Open Terminal", "keys": "Win+R" },
    { "label": "Zoom in",       "keys": "Ctrl+Plus" },
    { "label": "Type my email", "text": "me@example.com" },
    { "label": "Reboot PC",     "keys": "Ctrl+Alt+Del", "confirm": true }
  ]
}
```

- `keys` is a chord joined with `+`: modifiers `Ctrl`, `Shift`, `Alt`, `Win`; keys `A-Z`, `0-9`, `F1-F24`, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, arrows (`Left`, `Up`...), `PrintScreen`, `Plus`, `Minus`, `Comma`, `Period` and similar.
- `text` types the string on the remote PC. Don't put passwords here, because the file is plain text.
- `"confirm": true` requires a second click.
- A bad entry (unknown key name) shows up in red and can't be clicked. A broken file shows an error line instead of your shortcuts.

### Icon
The icon is an original design for this fork (a crescent moon and a spark of light on a warm coral background). It is not the Moonlight logo or any other company's logo. A 1024px copy is in `branding/vibelight-icon.png`.

### Updates
The update indicator now checks the latest GitHub release of this repository (`nyteghost/vibelight`) instead of upstream Moonlight. It only shows an update when a release exists whose tag (for example `v6.3.0`) is newer than the version in `app/version.txt`. On Windows (and macOS) clicking the indicator downloads the matching portable zip (or `.dmg`) from the release into your Downloads folder, checks it against GitHub's published SHA-256 checksum, and offers to open the folder. On a portable install (a `portable.dat` next to the exe, in a writable folder) it then offers **Install and restart**: Vibelight closes, a bundled PowerShell script (`app/update-vibelight.ps1`) unpacks the zip over the app folder and starts Vibelight again. Nothing is deleted, `portable.dat` and your settings are untouched, every replaced file is first copied to `.update-backup`, and if copying fails the old files are restored. The script's log is `%TEMP%\vibelight-update.log`. Otherwise (or if you decline) it opens the folder and you can unzip into a new folder yourself, keeping your `portable.dat`. Platforms without a downloadable package just open the release page. **Settings → Support → Check for updates** checks right now on any platform and says whether you're up to date, with a **Download update** button when a newer package is available. It is only active on Windows, macOS, Steam Link and AppImage builds.

### Look and feel
The app's colors follow the Vibepollo web UI: a near-black navy background (`#060918`), dark navy toolbar and panels (`#0D1424`), and an indigo accent (`#6366F1`). PC and game tiles are rounded cards whose border turns indigo on hover or keyboard focus, the top bar is flat with a thin bottom border, and each section of the Settings page is a rounded dark card with an indigo heading. The overall screen layout (grids, toolbar buttons, settings columns) is still Moonlight's. The in-stream shortcuts button and menu use their own warm palette instead, matching the coral Vibelight icon (warm near-black surfaces, coral accent, cream headings).

### Rename
The visible app name is now **Vibelight** (window titles, messages, command-line help, the Windows `.exe`, installer and portable package names). Some things intentionally still say Moonlight:
- Internal identifiers (settings location, registry keys, cache folders, log file names) are unchanged, so settings and pairing data are compatible with upstream Moonlight.
- Linux and macOS binary and package names are unchanged.
- Translations of changed strings fall back to English.
- Some in-app links and licenses still point to upstream.

Main files changed in this fork: `app/streaming/shortcutmenu.{h,cpp}` (new), `app/gui/CardGroupBox.qml` (new), `.github/workflows/{release,sync-upstream}.yml` (new), and edits in `app/streaming/session.{h,cpp}`, `app/streaming/input/*`, `app/streaming/video/{decoder.h,ffmpeg.*}` (connection check), `app/backend/{autoupdatechecker,systemproperties}.*`, `app/gui/*.qml`, `app/app.pro`, the Windows packaging scripts and WiX files, and user-visible strings.

### Sharper text
Blurry text when remoted in usually comes from the stream resolution not matching your screen, too low a bitrate, or the codec. Things to try, roughly in order: stream at your monitor's native resolution (fullscreen, or at least 1:1); raise the bitrate (and use **Check connection** to see how much your network can take); use HEVC or AV1 instead of H.264; try **YUV 4:4:4** if your host supports it (Sunshine only; it needs a higher bitrate); and on the host, prefer Windows display scaling of 100% or 150% over odd values like 125%.

## Search settings
The box at the top of **Settings** filters the sections as you type: only sections that contain every word you typed stay visible (for example `mouse`, or `bitrate slider`). Clear the box, or press Esc in it, to see everything again.

## Report a problem
**Settings → Support → Report a problem** saves a diagnostics file to your Downloads folder and opens a new GitHub issue in your browser with your version and system details filled in. Drag the diagnostics file into the issue. The file can contain PC names and local IP addresses, so look it over before sharing. The repository needs Issues turned on (**Settings → General → Features → Issues** on GitHub).

## Differences from Moonlight
**Settings → Support → Differences from Moonlight** lists everything this fork changes or adds, with a button that opens GitHub's side-by-side code comparison against the original `moonlight-stream/moonlight-qt`. The list is the file `app/differences.txt`, so update it when you add a feature.

## What's new window
The first time you run a new version (a different number in `app/version.txt`), a **What's new** window shows the notes from `app/whatsnew.txt`. It only appears once per version, and you can open it again any time from **Settings → Support → What's new**. Builds that keep the same version number don't show it again.

## Support and diagnostics
Settings has a **Support** section showing the exact version and build (for example `6.2.0 (1a2b3c4)`), a **Save diagnostics file** button, and **Open log folder**. The diagnostics file (saved to your Downloads folder) lists the version, Windows version, CPU, graphics and gamepad information, plus the end of the latest log. Logs are written to `%TEMP%` as `Moonlight-<number>.log` (next to `Vibelight.exe` in portable mode); the app keeps the 10 newest. Clipboard sync writes lines starting with `[clipboard]` to the log: the request (host, port, size), the HTTP status, network and TLS errors, timing, and the host's error text. It never logs the clipboard contents. After trying clipboard sync, save a diagnostics file and send it along with what you saw.

## Staying in sync with upstream Moonlight
The **Sync upstream** workflow runs weekly (or by hand from the Actions tab). When the original Moonlight has new commits it merges them into an `upstream-sync/<date>` branch and opens a pull request into `master` for review. If the merge conflicts, or changes workflow files, it opens an issue instead. To let it handle workflow changes and start builds on its pull requests automatically, add a repository secret named `SYNC_TOKEN` (a personal access token with `repo` and `workflow` scopes).

## Releasing
Releases are optional. To publish one:

1. Edit `app/version.txt` (for example `6.2.1`), update `app/whatsnew.txt` with what changed, and commit them.
2. Tag and push: `git tag v6.2.1 && git push origin v6.2.1`.
3. The **Release** workflow builds the app and creates a GitHub Release with portable zips (`Vibelight-6.2.1-Windows-x64-portable.zip`, `...-arm64-...`) and a macOS `.dmg`. It stops early if the tag doesn't match `app/version.txt`.

The in-app update indicator only shows something once a release like this exists with a higher version than the installed one.

## Downloads
Until you publish a release, build artifacts come from GitHub Actions: open the **Actions** tab of this repository, pick a run of the **Build** workflow, and download the `Vibelight-Windows-x64-*` artifact. It is an unpacked folder. Branch pushes only build Windows x64 so they finish quickly; runs on `master` (and manual runs from the Actions tab: **Build → Run workflow**) also build Windows arm64, macOS, the Linux AppImage and Steam Link. Create an empty file called `portable.dat` next to `Vibelight.exe` to run it in portable mode, where settings are stored next to the program.

## Credits
The changes in this fork (the floating Shortcuts button and its features, the update download, the card styling, diagnostics, the release and sync workflows, the rename, the icon and this README) were written with [Claude](https://claude.com) by Anthropic. Moonlight itself is the work of the Moonlight project and its contributors.

## Upstream Moonlight
 - [Moonlight PC](https://moonlight-stream.org) and its [source](https://github.com/moonlight-stream/moonlight-qt)
 - Mobile versions for [Android](https://github.com/moonlight-stream/moonlight-android) and [iOS](https://github.com/moonlight-stream/moonlight-ios)
 - [Discord server](https://moonlight-stream.org/discord) and [Weblate translations](https://hosted.weblate.org/projects/moonlight/moonlight-qt/)

## Features
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - H.264, HEVC, and AV1 codec support (AV1 requires Sunshine and a supported host GPU)
 - YUV 4:4:4 support (Sunshine only)
 - HDR streaming support
 - 7.1 surround sound audio support
 - 10-point multitouch support (Sunshine only)
 - Gamepad support with force feedback and motion controls for up to 16 players
 - Support for both pointer capture (for games) and direct mouse control (for remote desktop)
 - Support for passing system-wide keyboard shortcuts like Alt+Tab to the host
 - Clickable, draggable shortcuts button with custom shortcuts, pins, a stream timer and a connection check (added in this fork)
 - In-app update download, diagnostics export and build info (added in this fork)

## Building

### Windows Build Requirements
* Qt 6.11 SDK or later (earlier versions may work but are not officially supported)
* [Visual Studio 2026](https://visualstudio.microsoft.com/downloads/) (Community edition is fine)
* Select **MSVC** option during Qt installation. MinGW is not supported.
* [7-Zip](https://www.7-zip.org/) (only if building installers for non-development PCs)
* Graphics Tools (only if running debug builds)
  * Install "Graphics Tools" in the Optional Features page of the Windows Settings app.
  * Alternatively, run `dism /online /add-capability /capabilityname:Tools.Graphics.DirectX~~~~0.0.1.0` and reboot.

### macOS Build Requirements
* Qt 6.11 SDK or later (earlier versions may work but are not officially supported)
* Xcode 15 or later (earlier versions may work but are not officially supported)
* [create-dmg](https://github.com/sindresorhus/create-dmg) (only if building DMGs for use on non-development Macs)

### Linux/Unix Build Requirements
* Qt 6 is recommended, but Qt 5.12 or later is also supported (replace `qmake6` with `qmake` when using Qt 5).
* GCC or Clang
* FFmpeg 4.0 or later
* Install the required packages:
  * Debian/Ubuntu:
    * Base Requirements: `libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev libavcodec-dev libavformat-dev libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev wayland-protocols libdrm-dev`
    * Qt 6 (Recommended): `qt6-base-dev qt6-declarative-dev libqt6svg6-dev qt6-wayland qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-qtquick`
    * Qt 5: `qtbase5-dev qt5-qmake qtdeclarative5-dev qtquickcontrols2-5-dev qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtquick2 qtwayland5`
  * RedHat/Fedora (RPM Fusion repo required):
    * Base Requirements: `openssl-devel SDL2-devel SDL2_ttf-devel ffmpeg-devel libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel alsa-lib-devel libdrm-devel`
    * Qt 6 (Recommended): `qt6-qtsvg-devel qt6-qtdeclarative-devel`
    * Qt 5: `qt5-qtsvg-devel qt5-qtquickcontrols2-devel`
* Building the Vulkan renderer requires a `libplacebo-dev`/`libplacebo-devel` version of at least v7.349.0 and FFmpeg 6.1 or later.

### Steam Link Build Requirements
* [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) cloned on your build system
* STEAMLINK_SDK_PATH environment variable set to the Steam Link SDK path

**Steam Link Hardware Limitations**  
Moonlight builds for Steam Link are subject to hardware limitations of the Steam Link device:
* Maximum resolution: **1080p (1920x1080)**
* Maximum framerate: **60 FPS**
* Maximum video bitrate: **40 Mbps**
* **HDR streaming is not supported** on the original hardware

### Docker containers
If you want to use Docker for building, look at [this repo](https://github.com/cgutman/moonlight-packaging) containing canonical containers
for different architectures, which handle building deps and extra linking for you.

### Build Setup Steps
1. Install the latest Qt SDK (and optionally, the Qt Creator IDE) from https://www.qt.io/download
    * You can install Qt via Homebrew on macOS, but you will need to use `brew install qt --with-debug` to be able to create debug builds of Moonlight.
    * You may also use your Linux distro's package manager for the Qt SDK as long as the packages are Qt 5.12 or later.
    * This step is not required for building on Steam Link, because the Steam Link SDK includes Qt 5.14.
2. Download submodules and dependencies
    * Run `git submodule update --init --recursive` from within `moonlight-qt/`.
    * On Windows and macOS, you must also run `setup-deps.ps1` (Windows) or `setup-deps.py` (macOS).
    * Perform these steps each time you pull new changes from the Git repository.
3. Open the project in Qt Creator or build from qmake on the command line.
    * To build a binary for use on non-development machines, use the scripts in the `scripts` folder.
        * For Windows builds, use `scripts\build-arch.bat` and `scripts\generate-bundle.bat`. Execute these scripts from the root of the repository within a Qt command prompt. Ensure  7-Zip binary directory is on your `%PATH%`.
        * For macOS builds, use `scripts/generate-dmg.sh`. Execute this script from the root of the repository and ensure Qt's `bin` folder is in your `$PATH`.
        * For Steam Link builds, run `scripts/build-steamlink-app.sh` from the root of the repository.
    * To build from the command line for development use on macOS or Linux, run `qmake6 moonlight-qt.pro` then `make debug` or `make release`.
        * The final binary will be placed in `app/moonlight`.
    * To create an embedded build for a single-purpose device, use `qmake6 "CONFIG+=embedded" moonlight-qt.pro` and build normally.
        * This build will lack windowed mode, Discord/Help links, and other features that don't make sense on an embedded device.
        * For platforms with poor GPU performance, add `"CONFIG+=gpuslow"` to prefer direct KMSDRM rendering over GL/Vulkan renderers. Direct KMSDRM rendering can use dedicated YUV/RGB conversion and scaling hardware rather than slower GPU shaders for these operations.

## Contribute
1. Fork us
2. Write code
3. Send Pull Requests

Check out our [website](https://moonlight-stream.org) for project links and information.
