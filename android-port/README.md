# Sayri — Android Port (SDL2)

This is the Android build for **Sayri**, a C-based AI chat assistant
(originally a desktop app for pulsarOS that talks to a local/remote
**Ollama** server). The UI layer is the original SDL2 C code — no
Flutter/Kotlin rewrite — bridged to Android using SDL2's official
`android-project` template.

```
android-port/
├── build.gradle                 # top-level Gradle config (AGP 8.1.1)
├── settings.gradle              # includes ':app'
├── gradle.properties
├── gradlew / gradlew.bat
├── gradle/wrapper/
└── app/
    ├── build.gradle             # AGP config: namespace, SDK, ABI, CMake
    ├── proguard-rules.pro
    ├── jni/
    │   ├── CMakeLists.txt       # add_subdirectory(SDL/SDL_image/SDL_ttf/src)
    │   └── src/
    │       ├── CMakeLists.txt   # builds your C sources into libmain.so
    │       └── *.c / *.h        # the Sayri sources (see below)
    └── src/main/
        ├── AndroidManifest.xml
        ├── assets/font/default.ttf
        ├── java/org/libsdl/app/ # SDL's real Java glue (SDLActivity, etc.)
        └── res/                 # launcher icon + AppTheme
```

## How the port works

* SDL's official `SDLActivity.java` (and its companion `SDL.java`,
  `SDLSurface.java`, `SDLAudioManager.java`, `SDLControllerManager.java`,
  `HIDDevice*.java`) are copied verbatim from the SDL `android-project`
  template. They handle the surface, touch/keyboard input, and call
  `SDL_main()` in `libmain.so`.
* The C entry point `app/jni/src/main_android.c` provides `SDL_main()`.
* The three modules that cannot work on Android are replaced with
  `_android` variants:
  * `ipc_android.c` — Unix-socket IPC is a **no-op stub**.
  * `ollama_android.c` — removes the auto-bootstrap (fork/exec/download);
    only a **remote Ollama server** is supported.
  * `history_android.c` — stores chat history in the app's **internal
    storage** via `SDL_AndroidGetInternalStoragePath()`.
* The font is loaded from the Android asset `font/default.ttf`.

## Prerequisites

* Android SDK (API 34) and NDK (r23+), Gradle 8.1.1.
* Native source trees for **SDL2**, **SDL2_image**, **SDL2_ttf**, plus a
  prebuilt **libcurl** for Android. See the next section.

## Place the native dependencies

The `jni/CMakeLists.txt` expects these directories (they are **not**
included in the repo and must be fetched by you):

1. **SDL2** — `app/jni/SDL/`
   ```sh
   git clone --depth 1 --branch release-2.30.x https://github.com/libsdl-org/SDL.git app/jni/SDL
   ```
2. **SDL2_image** — `app/jni/SDL_image/`
   ```sh
   git clone --depth 1 --branch release-2.30.x https://github.com/libsdl-org/SDL_image.git app/jni/SDL_image
   ```
3. **SDL2_ttf** — `app/jni/SDL_ttf/`
   ```sh
   git clone --depth 1 --branch release-2.30.x https://github.com/libsdl-org/SDL_ttf.git app/jni/SDL_ttf
   ```
   SDL_image and SDL_ttf pull their own third-party deps (libpng, libjpeg,
   libwebp, freetype) **via their CMake configuration**, so no manual
   install is needed if you use their bundled download logic.
4. **libcurl** for Android — `app/jni/curl/`
   ```
   app/jni/curl/include/curl/curl.h
   app/jni/curl/lib/<ABI>/libcurl.a      (or libcurl.so)
   ```
   Example using the curl source + Android NDK cross-compile, or a
   prebuilt NDK curl package for `arm64-v8a` (and `armeabi-v7a` if you
   re-enable that ABI in `app/build.gradle`).

> The version of SDL must match the Java glue. This port uses the
> `release-2.30.x` template (SDLActivity reports 2.30.12) — use matching
> SDL2 source so the C/Java version check passes.

## Build

```sh
# from android-port/
./gradlew assembleDebug
# or install to a connected device/emulator
./gradlew installDebug
```

The `assembleDebug` step compiles the C sources with the NDK (CMake),
packages `libSDL2.so`, `libSDL2_ttf.so`, `libSDL2_image.so` and
`libmain.so`, then builds the APK.

### ABI filters

`app/build.gradle` currently builds **`arm64-v8a` only**. To support
32-bit devices, add `armeabi-v7a` to `abiFilters` **and** make sure you
have `libcurl.a` for that ABI under `app/jni/curl/lib/armeabi-v7a/`.

## Notes / limits of this port

* **Remote Ollama only.** The desktop app auto-downloaded and launched a
  local Ollama binary; that is impossible on Android. Set the server host
  in the app's Settings to a reachable Ollama instance.
* **IPC (Unix sockets) is disabled** — not applicable on Android.
* History is saved to app-internal storage.
* The Back button closes overlays first, then exits (handled in
  `main_android.c`).
