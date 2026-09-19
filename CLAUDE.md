# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

`lifenzdotorg/commsbus` — **Commsbus**, a fork of [sonosaurus/sonobus](https://github.com/sonosaurus/sonobus) (SonoBus), a JUCE app for low-latency peer-to-peer audio over a network.

The fork has diverged from upstream in four deliberate ways. Keep these in mind before "fixing" something back toward upstream behaviour:

1. **Standalone application only.** The VST3 / AU / AAX / LV2 / VSTi plugin targets were removed. `juce_add_plugin` is still used (the JUCE standalone wrapper is built on it) but `Standalone` is the only format.
2. **Private group is the default, direct connection is second.** `PRIVATE GROUP` is tab 0 and the view the Connect panel opens on; `DIRECT` — which upstream shipped commented out entirely — is tab 1. A group survives an address change at either end, which a direct address does not, so it is the better default for an unattended bridge. **`PUBLIC GROUPS` was removed outright** — the tab, the widgets, `PublicGroupsListModel`, `publicGroupLogin`, the `setWatchPublicGroups`/`getPublicGroupInfos` processor API, the `AooPublicGroupInfo` struct, the two `aooClientPublicGroup*` listener callbacks, and the `groupIsPublic`/`isPublic` flag that ran through `AooServerConnectionInfo`, `joinServerGroup`, `suggestNewGroupToPeers` and `peerSuggestedNewGroup`. `AOONET_CLIENT_GROUP_PUBLIC_ADD_EVENT`/`_DEL_EVENT` are still received from AOO and deliberately ignored.
3. **4 independent mono input channel groups by default**, clamped to the device's input count, instead of upstream's single group spanning every input.
4. **Unattended operation.** Auto-reconnect defaults on, direct peers reconnect themselves, macOS start-at-login is available, and only one instance runs at a time.
5. **No metronome, file playback, soundboard or recording.** All removed outright, along with the mixer strip that carried the first three.

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

Owns the major sub-views, each its own file pair: `ConnectView`, `OptionsView`, `PeersContainerView` (one row per peer), `ChannelGroupsView` (local input strips, and the received strips inside each peer row), `BusesView` (the BUSES panel), `ChatView`, `LatencyMatchView`, `SuggestNewGroupView`. Effect editors are header-only `*View.h` files sharing `EffectsBaseView.h`.

Custom widgets are prefixed `Sono*` — prefer reusing them over raw JUCE widgets.

Tab order is `PRIVATE GROUP`, `DIRECT`, `RECENTS` (`recentsTabPosition = 2`).
In `ConnectView::resized()`, wide layouts pull RECENTS out of the tab strip into
its own panel. Everything that has to find a tab does so **by name**
(`getTabNames().indexOf(TRANS("RECENTS"))`, and `showPrivateGroupTab()` likewise),
never by index or by counting tabs — upstream's hard-coded `removeTab(0)` /
`moveTab(2,0)` and its `getNumTabs() < 3` tests all assumed a tab order this fork
no longer has. Keep it name-based if you add or remove tabs.

Below the `Connection Server` field on the PRIVATE GROUP tab,
`mServerHostHintLabel` points out that every Commsbus instance is itself a
connection server on port 10999, so a pair of them needs nothing on the internet.
That is true of upstream SonoBus too (`startAooServer()` is called unconditionally
for the desktop standalone), just never surfaced.

### Dante bridge model

Commsbus exists to carry Dante audio point-to-point over a WAN: Dante Virtual
Soundcard presents the Dante network as an ordinary audio device at each end, and
Commsbus bridges between two such devices. The main window is framed accordingly
-- **TRANSMIT** (local device inputs going out over the network) above
**RECEIVE** (streams arriving from the far end going out to the local device).
The transmit section is always visible; it is not the optional input mixer panel
it was in SonoBus.

Everything is mono per channel by default: `DEFAULT_MONO_CHANNEL_GROUPS` mono
input groups each landing on their own output channel, and `panDestChannels` /
`monDestChannels` default to 1. Stereo pairing is not used in this application.

The receive strips carry **level only** -- name, mute, level, meter and
destination. Panning is laid out and made visible in
`updateLayoutForInput`/`updateInputModeChannelViews` (transmit) but never in
`updateLayoutForRemotePeer`/`updatePeerModeChannelViews` (receive).
`ChannelGroupView` still *owns* the pan widgets, because the same class serves
both sides -- the receive path simply leaves them out of the FlexBox and hidden.
Do not "restore" them on the receive side.

**No per-channel effects UI on either side.** The `FX` and `M.FX` buttons are
gone from both the transmit and receive strips, and the `In Reverb` button is
gone from the transmit header row. `fxButton`/`monfxButton` still exist on
`ChannelGroupView` but are `setVisible(false)` in all four update paths and are
in no FlexBox; `mInReverbButton`, `showInputReverbView` and `inReverbCalloutBox`
were deleted outright. The effects *DSP* (compressor, expander, EQ, limiter,
reverb send, `ChannelGroupEffectsView` and friends) is still compiled and still
runs if a `ChannelGroupParams` arrives with an effect enabled -- it is simply
unreachable from the UI. The `Use Input FX Limiter` toggle is gone from Options
for the same reason.

The main output `FX` button went the same way: `mEffectsButton`,
`showEffectsConfig` and `effectsCalloutBox` are deleted, so the global reverb
panel (`mEffectsContainer`, `mReverbEnabledButton`, `mReverbModelChoice` and the
reverb knobs) is built in the editor constructor and attached to its APVTS
parameters but never shown. The `paramMainReverb*` parameters still exist. There
is now **no effects UI anywhere in the application**.

**Output buses** (`OutputBus`, `MAX_OUTPUT_BUSES`) are the receive-side mixing
stage. A received channel group either goes straight out to device channels
(`panDestStartIndex`/`panDestChannels`, the `busAssign == -1` case) or is summed
into a bus, which applies its own level and lands on its own device channels.
That is how several incoming streams get combined onto one Dante destination.
`busAssign` lives on `ChannelGroupParams` so it persists with the group; the
buses themselves persist under the `OutputBuses` child of the state tree.
Buses are mono: one summed stream onto one Dante output.

`BusesView` (`Source/BusesView.{h,cpp}`) is the **BUSES** panel at the bottom of
the receive area -- one row per bus with its name (double-click to rename), how
many streams feed it, its master level, its output channel and a remove button,
plus a `+` to add one. A bus can also be created from a receive row's destination
menu, so `BusesView` does not own the bus list: it polls `getNumOutputBuses()`
via `refreshIfBusesChanged()` (from the editor's 1s timer, `channelLayoutChanged`
and `internalSizesChanged`) and rebuilds its rows when the count moves. Removing
a bus reverts everything feeding it to direct out -- see `removeOutputBus`.

In `processBlock` the bus stage sits inside the peer loop: the bus rows of
`mBusBuffer` are cleared before the loop, assigned groups sum into their row
instead of panning to `tempBuffer`, and after the loop each bus is mixed into
`tempBuffer` at its destination with its gain. The bus list is snapshotted under
`mBusLock` at the top of the block so a UI edit cannot change routing mid-block.

### Removed subsystems

The metronome (`Metronome.*`), the file playback transport (`WaveformTransportComponent.h`, `AudioTransportSource`, `loadURLIntoTransport`) and the whole Soundboard subsystem (`Soundboard*.*`, `SampleEditView.*`, `SonoPlaybackProgressButton.*`) are gone, along with ~12 parameters (`paramMet*`, `paramSendFileAudio`, `paramSendSoundboardAudio`, `paramSyncMet*`), their toolbar buttons, menu commands and translation entries.

Two things survived that look like they belong to those features but do not:

- **`BeatToggleGrid.{cpp,h}`** is *not* metronome code — it backs `PatchMatrixView`, the peer send/receive routing matrix. Keep it.
- **`images/lgc_bar.wav`** is the `LatencyMeasurer` pulse, not a metronome click. It stays in the binary-data list; the actual click samples (`bar_click.wav`, `beat_click.wav`) were removed.

`CommsbusAudioProcessor` no longer derives from `ChangeListener` — the only thing that used it was the transport.

### Resources and localization

Images, fonts, click samples and translations are compiled in via `juce_add_binary_data(Commsbus_SBData ...)` in `CMakeLists.txt` — **a new asset in `images/` or `localization/` is invisible until added to that list**. Translations are JUCE `LocalisedStrings` files `localization/localized_<lang>.txt`, loaded by resource name in `CommsbusAudioProcessorEditor.cpp`; a same-named file in the user settings folder wins, so translations can be tested without rebuilding. Changing an English source string orphans its translation key in all 11 files — update or remove them together. `localization/tsv/tsvtostrings.py` converts a spreadsheet export (it is Python 2).

## macOS code signing and permissions

macOS grants microphone and local-network access **per code-signing identity**,
not per path. JUCE's build leaves the app ad-hoc "linker-signed", whose
designated requirement is the exact binary hash -- so every rebuild looks like a
different app and every permission is asked for again.

`CMakeLists.txt` therefore re-signs the app bundle as a POST_BUILD step with
`COMMSBUS_CODESIGN_IDENTITY`, auto-detected at configure time from the first
`Developer ID Application` identity in the keychain. That gives a designated
requirement keyed to the certificate and bundle id rather than a hash, so grants
survive rebuilds. Override it with
`cmake -DCOMMSBUS_CODESIGN_IDENTITY="..."`, `"-"` for plain ad-hoc, or `OFF` to
leave JUCE's signature alone. With no Developer ID available, a self-signed
codesigning certificate from Keychain Access works just as well *on that machine*
-- what matters is that the identity is stable, not that it is trusted.

Signing uses hardened runtime (`--options=runtime`) and
`scripts/Commsbus-mac.entitlements`, which carries
`com.apple.security.device.audio-input` -- without that entitlement the hardened
runtime denies microphone access outright. `release/codesign.sh` signs again with
a secure timestamp for notarization; it signs the app only, since the plugin
formats were removed.

`NSMicrophoneUsageDescription` and `NSLocalNetworkUsageDescription` are both in
the merged plist (`MacPList` in `CMakeLists.txt`). Changing the bundle id, the
signing identity or the team resets every grant.

## Auto-update

`VersionInfo.cpp` points at `lifenzdotorg/commsbus` releases, **not** upstream — pointing it at `sonosaurus/sonobus` would offer to install SonoBus over a Commsbus install. The check also defaults **off** (`shouldCheckForNewVersion` in `SonoStandaloneFilterWindow.h`), since Commsbus is meant to run unattended.

## Release and packaging

`release/` holds the pipeline: `buildmac.sh`/`buildwin.sh`, `distmac.sh <version>`/`distwin.sh`, `codesign.sh`, `notarize-app.sh`/`notarizedmg.sh`, `makedmg.sh`/`makepkgdmg.sh`, `wininstaller.iss`, and `update_package_version.py`. All plugin-copying steps were removed — these scripts now package the standalone app only. `snap/snapcraft.yaml` builds the Linux snap and now sources from the fork. macOS entitlements are in `scripts/Commsbus-mac.entitlements` (plus a sandboxed variant).
