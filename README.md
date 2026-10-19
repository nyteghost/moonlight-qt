# Vibelight

Vibelight is a fork of [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt), the open source PC client for NVIDIA GameStream and [Sunshine](https://github.com/LizardByte/Sunshine). All of the streaming, decoding and input work is Moonlight's. This fork adds a few conveniences for remote-desktop use and renames the app.

> **This is an unofficial fork.** It is not affiliated with or endorsed by the Moonlight project. Please don't report problems with this fork to the upstream project. Upstream is licensed under GPLv3, and so is this fork (see [LICENSE](LICENSE)).

## What's different from upstream Moonlight

### Floating "Shortcuts" button
While streaming, press **Ctrl+Alt+Shift+H** to show a small always-on-top **Shortcuts** button over the stream window. It is hidden the rest of the time. Drag it anywhere. A click opens a dropdown of shortcuts you can click instead of pressing key combinations:

- **Sent to the remote PC:** Ctrl+Alt+Del, Alt+Tab, Alt+F4, Task Manager (Ctrl+Shift+Esc), Win, Win+D, Win+E, Win+L, Win+R, Win+Tab, Win+Shift+S, Print Screen.
- **Vibelight's own actions** (the Ctrl+Alt+Shift+key hotkeys): release/grab mouse (Z), fullscreen (X), performance stats (S), mouse mode (M), show/hide cursor (C), minimize (D), paste clipboard as typing (V), pointer region lock (L), system key capture (K), quit stream (Q), quit stream and exit app (E).
- **Displays & windows** (sent to the remote PC): projection modes (Win+P), move a window to the left or right monitor (Win+Shift+Left/Right), snap left/right (Win+Left/Right), maximize (Win+Up), and reset the graphics driver (Win+Ctrl+Shift+B). These move windows and change display modes on the remote PC. Choosing which monitor is *streamed* is a host setting (Sunshine/Vibepollo), not something the client can switch.
- **Hints and feedback:** at the start of a stream (the first three times) the button appears for a few seconds saying how to open it. After you choose a shortcut, the button briefly shows "Sent: <shortcut>". If your custom shortcuts file has a problem, you also get a short warning at the start of the stream.
- **Pinned & recent:** right-click any shortcut to pin it (up to 10, marked with `*`). Pinned and your four most recently used shortcuts appear in a column on the left.
- **Closing the menu:** press Esc (it won't be sent to the remote PC), click anywhere outside it, or press Ctrl+Alt+Shift+H again.
- **Custom shortcuts** appear in extra columns (see below), up to 28 entries.
- Quit actions need a second click to confirm.
- Pressing **Ctrl+Alt+Shift+H** also frees the mouse first, which is needed in relative mouse mode. Press it again, press Esc, or pick an action to hide the button again.

Notes and limitations:
- The button is available on Windows, macOS and X11. It is not shown on Wayland, KMS/DRM or Steam Link. Set the environment variable `ML_DISABLE_SHORTCUT_MENU` to turn it off.
- Windows may block injected Ctrl+Alt+Del depending on the host software.
- This feature is new and has had limited testing, especially in exclusive fullscreen.

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
The update indicator now checks the latest GitHub release of this repository (`nyteghost/moonlight-qt`) instead of upstream Moonlight. It only shows an update when a release exists whose tag (for example `v6.3.0`) is newer than the version in `app/version.txt`. On Windows (and macOS) clicking the indicator downloads the matching portable zip (or `.dmg`) from the release into your Downloads folder, checks it against GitHub's published SHA-256 checksum, and offers to open the folder. It doesn't replace the running app: close Vibelight, unzip into a new folder, and keep your `portable.dat` file there to keep your settings. Platforms without a downloadable package just open the release page. It is only active on Windows, macOS, Steam Link and AppImage builds.

### Look and feel
The app's colors follow the Vibepollo web UI: a near-black navy background (`#060918`), dark navy toolbar and panels (`#0D1424`), and an indigo accent (`#6366F1`). PC and game tiles are rounded cards whose border turns indigo on hover or keyboard focus, the top bar is flat with a thin bottom border, and each section of the Settings page is a rounded dark card with an indigo heading. The overall screen layout (grids, toolbar buttons, settings columns) is still Moonlight's.

### Rename
The visible app name is now **Vibelight** (window titles, messages, command-line help, the Windows `.exe`, installer and portable package names). Some things intentionally still say Moonlight:
- Internal identifiers (settings location, registry keys, cache folders, log file names) are unchanged, so settings and pairing data are compatible with upstream Moonlight.
- Linux and macOS binary and package names are unchanged.
- Translations of changed strings fall back to English.
- Some in-app links and licenses still point to upstream.

Files changed in this fork: `app/streaming/shortcutmenu.{h,cpp}` (new), plus small edits in `app/streaming/session.{h,cpp}`, `app/streaming/input/{input.h,input.cpp,keyboard.cpp}`, `app/app.pro`, the Windows packaging scripts and WiX files, and user-visible strings.

## Support and diagnostics
Settings has a **Support** section showing the exact version and build (for example `6.2.0 (1a2b3c4)`), a **Save diagnostics file** button, and **Open log folder**. The diagnostics file (saved to your Downloads folder) lists the version, Windows version, CPU, graphics and gamepad information, plus the end of the latest log. Logs are written to `%TEMP%` as `Moonlight-<number>.log` (next to `Vibelight.exe` in portable mode); the app keeps the 10 newest.

## Staying in sync with upstream Moonlight
The **Sync upstream** workflow runs weekly (or by hand from the Actions tab). When the original Moonlight has new commits it merges them into an `upstream-sync/<date>` branch and opens a pull request into `master` for review. If the merge conflicts, or changes workflow files, it opens an issue instead. To let it handle workflow changes and start builds on its pull requests automatically, add a repository secret named `SYNC_TOKEN` (a personal access token with `repo` and `workflow` scopes).

## Releasing
Releases are optional. To publish one:

1. Edit `app/version.txt` (for example `6.2.1`) and commit it.
2. Tag and push: `git tag v6.2.1 && git push origin v6.2.1`.
3. The **Release** workflow builds the app and creates a GitHub Release with portable zips (`Vibelight-6.2.1-Windows-x64-portable.zip`, `...-arm64-...`) and a macOS `.dmg`. It stops early if the tag doesn't match `app/version.txt`.

The in-app update indicator only shows something once a release like this exists with a higher version than the installed one.

## Downloads
Until you publish a release, build artifacts come from GitHub Actions: open the **Actions** tab of this repository, pick a run of the **Build** workflow, and download the `Vibelight-Windows-x64-*` artifact (or `-arm64-`). It is an unpacked folder. Create an empty file called `portable.dat` next to `Vibelight.exe` to run it in portable mode, where settings are stored next to the program.

## Credits
The changes in this fork (the floating Shortcuts button, custom shortcuts, the release workflow, the rename, the icon and this README) were written with [Claude](https://claude.com) by Anthropic. Moonlight itself is the work of the Moonlight project and its contributors.

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
 - Clickable, draggable shortcuts button (added in this fork)

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
