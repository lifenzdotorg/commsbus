# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

`lifenzdotorg/commsbus` — **Commsbus**, a fork of [sonosaurus/sonobus](https://github.com/sonosaurus/sonobus) (SonoBus), a JUCE app for low-latency peer-to-peer audio over a network.

The fork has diverged from upstream in four deliberate ways. Keep these in mind before "fixing" something back toward upstream behaviour:

1. **Standalone application only.** The VST3 / AU / AAX / LV2 / VSTi plugin targets were removed. `juce_add_plugin` is still used (the JUCE standalone wrapper is built on it) but `Standalone` is the only format.
2. **Direct connection is the default.** The DIRECT tab — commented out upstream — is tab 0 and the default view. The group/rendezvous-server path still exists as a fallback.
3. **4 independent mono input channel groups by default**, clamped to the device's input count, instead of upstream's single group spanning every input.
4. **Unattended operation.** Auto-reconnect defaults on, direct peers reconnect themselves, and macOS start-at-login is available.

Licensed GPLv3 (with an App Store exception — see `LICENSE_EXCEPTION`). Source files carry an SPDX header; `scripts/prependheader.sh` adds it to new files. Upstream authorship (Jesse Chappell) is retained in all headers.

### Naming convention after the rebrand

`SonoBus`/`Sonobus`/`sonobus` → `Commsbus`/`commsbus` everywhere. The **`Sono*` prefix was deliberately kept** — `SonoLookAndFeel`, `SonoTextButton`, `SonoCallOutBox`, `SonoStandaloneFilterApp/Window`, the `SonoAudio` namespace. That prefix is an internal widget/wrapper toolkit, not the product brand, and renaming it would balloon the diff for no user-visible gain.

Three upstream endpoints are intentionally preserved verbatim and must not be renamed: `aoo.sonobus.net` (the rendezvous server, `DEFAULT_SERVER_HOST`), `go.sonobus.net/sblaunch` (the group-invite URL), and the `sonosaurus/sonobus` reference in a comment in `VersionInfo.cpp`.

## Build

CMake >= 3.15. Two steps on every platform — `setupcmake*.sh` only configures, `buildcmake.sh` compiles.

```bash
./setupcmake.sh && ./buildcmake.sh
```

```bash
./setupcmake.sh debug && ./buildcmake.sh debug
```

- **macOS Xcode project**: `./setupcmakexcode.sh` → `buildXcode/Commsbus.xcodeproj`. `APPLE_TEAMID` bakes in the signing team.
- **Windows**: `./setupcmakewin.sh` (VS2019 x64) or `./setupcmakewin32.sh` (VS2017 32-bit → `build32/`).
- **Linux**: `linux/deb_get_prereqs.sh` or `linux/fedora_get_prereqs.sh`, then `cd linux && ./build.sh`. Opus must be installed system-wide; it is the only dependency not vendored.

The app lands in `build/Commsbus_artefacts/Release/Standalone/Commsbus.app`. Bundle ID `org.lifenz.Commsbus`, URL scheme `commsbus://`.

**There is no test suite.** Verification is: it compiles, and you run the app. When changing anything non-trivial, build before claiming it works.

### Vendored JUCE is patched for modern macOS SDKs

`deps/juce/modules/juce_gui_basics/native/juce_Windowing_mac.mm` carries a Commsbus patch. `CGWindowListCreateImage` is *obsoleted* (not merely deprecated) in the macOS 15+ SDK, so `createNSWindowSnapshot` no longer compiles at all — upstream's `-Wdeprecated-declarations` suppression is not enough, and this JUCE version has no ScreenCaptureKit fallback. The patch returns a null image on SDK 15+. Without it the build fails in JUCE's own `juceaide` during **configure**, before any Commsbus source is reached. Upstream SonoBus has the same problem on these SDKs.

### Mobile is not maintained in this fork

iOS/Android build from `mobile/CommsbusMobile.jucer` via Projucer, not CMake. **The rebrand deliberately skipped `mobile/` and `Source/android/`** — they still say SonoBus, and the Android Java package is still `com/sonosaurus/sonobus`. Renaming a Java package means moving directories and regenerating the Projucer output; half-doing it is worse than not touching it. Treat mobile as unmigrated. Note also that adding a file to `CMakeLists.txt` does not add it to the mobile build.

### Version bumps

Duplicated in three places: `project(Commsbus VERSION ...)` plus `BUILDVERSION` in `CMakeLists.txt`, `version=` in `mobile/CommsbusMobile.jucer`, and `source-tag` in `snap/snapcraft.yaml`.

## Vendored dependencies (`deps/`)

`deps/aoo`, `deps/juce` and `deps/ff_meters` are **git-subrepos**, not submodules — full source committed in-tree, synced with [`git-subrepo`](https://github.com/ingydotnet/git-subrepo). Each has a `.gitrepo` recording its upstream. `aoo` and `juce` are forks (`essej/aoo` branch `sono`, `essej/JUCE` branch `sono7good`), so upstream JUCE/AOO docs may not match.

A **stale top-level `JUCE/` directory** is the older `sono6good` (JUCE 6) subrepo. Nothing references it; both `CMakeLists.txt` and the `.jucer` use `deps/juce`.

Opus is vendored as prebuilt static libs under `deps/mac`, `deps/windows`, `deps/ios`; Linux links system `libopus`.

## Generated sources — do not hand-edit

Faust output: `Source/faustCompressor.h`, `faustExpander.h`, `faustLimiter.h`, `faustParametricEQ.h`, `Source/zitaRev.h`. Inputs are the `.dsp` files in `scripts/`; regenerate with `scripts/makecomp.sh`, `makeeq.sh`, `makezita.sh`.

## Architecture

Processor/editor split, but both files are large (≈9.8k and ≈6k lines) and hold most of the logic.

### `CommsbusAudioProcessor` (`Source/CommsbusAudioProcessor.{h,cpp}`)

Owns all audio processing, network state, and persistence.

- **Networking is AOO** (`deps/aoo`) over one shared `DatagramSocket`. Each participant is a `RemotePeer` holding an `aoo::isink`/`aoo::isource` pair, plus extra pairs offset by `LATENCY_ID_OFFSET`/`ECHO_ID_OFFSET` for round-trip latency measurement. `mAooClient` talks to the rendezvous server purely for peer discovery; audio always flows peer-to-peer. `mAooServer` is the internal connection server the app can host on port 10999.
- **Five threads**, nested classes near the top of the .cpp: `SendThread` (realtime, woken via `mSendWaitable`/`notifySendThread()`), `RecvThread` (realtime, polls the socket), `EventThread` (20ms poll → `handleEvents()`), and `ServerThread`/`ClientThread` running the blocking AOO loops. Several distinct locks guard different state (`mCoreLock`, `mEndpointsLock`, `mClientLock`, `mRemotesLock`, `mSourceFormatLock`) — respect the existing ordering.
- **`ChannelGroup`** (`Source/ChannelGroup.{h,cpp}`) is the reusable audio strip: gain, multichannel panning, compressor, expander/gate, parametric EQ, limiter, polarity invert, reverb sends, monitor level and delay. The *same* class serves local input groups and each remote peer's received audio, so a change affects both sides.
- **Default input layout** is set in `prepareToPlay` when `mInputChannelGroupCount == 0`: `DEFAULT_MONO_CHANNEL_GROUPS` (4) mono groups, one per hardware input, clamped to the device's input count.
- **Parameters**: ~38 automatable APVTS parameters declared as `static String paramXxx`. Everything else (per-peer settings, channel group layouts, recents, soundboard, direct peers) lives in the `ValueTree` state tree (`"CommsbusAoO"`). `getStateInformationWithOptions()`/`setStateInformationWithOptions()` are the real implementations; the plain overrides delegate with flags controlling what is included, which is how app state and exported presets differ.

### Unattended operation

- **`AutoConnectManager`** (`Source/AutoConnectManager.{h,cpp}`) holds the direct-peer list (`DirectPeerEntry`: host, port, label), persists it under the `DirectPeers` child of the state tree, and runs a 2s `Timer`. Each tick it compares configured peers against `getNumberRemotePeers()`/`getRemotePeerAddressInfo()` and reconnects any that are missing, with exponential backoff (2s→30s) that resets the moment a peer reappears. It deliberately **polls rather than reacting to events** — one mechanism recovers from every way a link can die (peer reboot, cable pull, address change, app restart). It is built entirely on the processor's public API. Started from `setStateInformationWithOptions` on first state restore, via `startAutoConnect()`.
- **`CommsbusAutoStart`** (`Source/CommsbusAutoStart.h`, `CommsbusAutoStartMac.mm`, `CommsbusAutoStartGeneric.cpp`) installs a per-user launchd agent at `~/Library/LaunchAgents/org.lifenz.commsbus.plist` with `RunAtLoad` and `KeepAlive{SuccessfulExit=false}` — so it returns after a reboot and after a crash, but not after a deliberate quit. macOS only; `isSupported()` returns false elsewhere and the Options toggle is hidden.

- **Single-instance guard** (`Source/CommsbusSingleInstance.h`, `CommsbusSingleInstanceMac.mm`, `CommsbusSingleInstanceGeneric.cpp`, enforced in `SonoStandaloneFilterApp.cpp::initialise`). With the login agent installed it is easy to get launchd's copy and a hand-launched copy both open, fighting over the audio device and the UDP port. A `juce::InterProcessLock` taken with `enter(0)` refuses the second one, which activates the running instance (macOS) and quits with status 0 — deliberately 0, so launchd's `KeepAlive{SuccessfulExit=false}` does not treat it as a crash and restart it. The check sits after command-line handling and before any window or audio device is created, so `--version`/`--help` still work in a second process. `--allow-multiple` bypasses it.

  The lock is an `fcntl` record lock under `~/Library/Caches/com.juce.locks` — per-user (two macOS users can each run one), and released by the kernel even on `SIGKILL`, so a crash never leaves a stale lock.

  `CommsbusAutoStart.h` includes `<juce_core/juce_core.h>` rather than `JuceHeader.h` **on purpose**: `JuceHeader.h` does `using namespace juce`, which makes `juce::Point` ambiguous with Carbon's `::Point` in any `.mm` that imports Cocoa. `CrossPlatformUtils.h` and `CommsbusSingleInstance.h` avoid `JuceHeader.h` for the same reason. Don't "tidy" this.

### `CommsbusAudioProcessorEditor` (`Source/CommsbusAudioProcessorEditor.{h,cpp}`)

Owns the major sub-views, each its own file pair: `ConnectView`, `OptionsView`, `PeersContainerView` (one row per peer), `ChannelGroupsView` (local input strips), `ChatView`, `SoundboardView`, `LatencyMatchView`, `SampleEditView`, `SuggestNewGroupView`. Effect editors are header-only `*View.h` files sharing `EffectsBaseView.h`.

Custom widgets are prefixed `Sono*` — prefer reusing them over raw JUCE widgets.

In `ConnectView::resized()`, wide layouts pull RECENTS out of the tab strip into its own panel. That code looks the tab up **by name** (`getTabNames().indexOf(TRANS("RECENTS"))`), not by index — DIRECT now occupies index 0, and the original hard-coded `removeTab(0)`/`moveTab(2,0)` would move the wrong tab. Keep it name-based if you add tabs.

### Resources and localization

Images, fonts, click samples and translations are compiled in via `juce_add_binary_data(Commsbus_SBData ...)` in `CMakeLists.txt` — **a new asset in `images/` or `localization/` is invisible until added to that list**. Translations are JUCE `LocalisedStrings` files `localization/localized_<lang>.txt`, loaded by resource name in `CommsbusAudioProcessorEditor.cpp`; a same-named file in the user settings folder wins, so translations can be tested without rebuilding. Changing an English source string orphans its translation key in all 11 files — update or remove them together. `localization/tsv/tsvtostrings.py` converts a spreadsheet export (it is Python 2).

## Auto-update

`VersionInfo.cpp` points at `lifenzdotorg/commsbus` releases, **not** upstream — pointing it at `sonosaurus/sonobus` would offer to install SonoBus over a Commsbus install. The check also defaults **off** (`shouldCheckForNewVersion` in `SonoStandaloneFilterWindow.h`), since Commsbus is meant to run unattended.

## Release and packaging

`release/` holds the pipeline: `buildmac.sh`/`buildwin.sh`, `distmac.sh <version>`/`distwin.sh`, `codesign.sh`, `notarize-app.sh`/`notarizedmg.sh`, `makedmg.sh`/`makepkgdmg.sh`, `wininstaller.iss`, and `update_package_version.py`. All plugin-copying steps were removed — these scripts now package the standalone app only. `snap/snapcraft.yaml` builds the Linux snap and now sources from the fork. macOS entitlements are in `scripts/Commsbus-mac.entitlements` (plus a sandboxed variant).
