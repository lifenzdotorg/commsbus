# Commsbus

Commsbus is a stripped-down, always-on fork of
[SonoBus](https://github.com/sonosaurus/sonobus) for streaming high-quality,
low-latency peer-to-peer audio between machines over a local network or the
internet.

Where SonoBus is a general-purpose tool that also ships as a DAW plugin,
Commsbus is built to be one thing and be reliable at it: carrying **Dante audio
point-to-point over a WAN**. Dante Virtual Soundcard presents the Dante network
as an ordinary audio device at each end, and a Commsbus instance at each end
bridges between them. A fixed set of machines find each other directly, come up
on their own after a reboot, and stay connected without anyone driving the UI.

The main window is split accordingly:

- **TRANSMIT** (top) -- the local Dante inputs being sent out over the network,
  one mono channel each, with the channel names you give them.
- **RECEIVE** (bottom) -- the streams arriving from the far end, each with a
  level, routed either straight out to a Dante output or into a **bus** that
  combines several streams before going out to Dante.

## How it differs from SonoBus

| | SonoBus | Commsbus |
|---|---|---|
| Formats | Standalone + VST3 / AU / AAX / LV2 / VSTi | Standalone application only |
| Connection | Private group, public group, or direct | **Private group** (default) or direct; public groups removed |
| Default input layout | One group spanning every input channel | **4 independent mono channel groups** |
| Metronome / file playback / soundboard | Included | **Removed** |
| Recording | Included | **Removed** |
| Receive-side mixing | Pan/FX per peer | **Level + bus routing only** |
| Effects | Per-channel and main reverb/compressor/EQ | **No effects UI at all** |
| After a reboot | Launched by hand | Starts automatically (macOS launch agent) |
| After a dropout | Reconnected by hand | Reconnected automatically, with backoff |
| Auto-update | On, pointed at SonoBus releases | Off, pointed at Commsbus releases |

**Private groups** are the default. Both ends point at the same connection
server and find each other by group name, which survives an address change at
either end. Every Commsbus instance *is* a connection server, on port 10999 —
so for a pair of sites that can reach each other, point both at one of the two
machines and nothing has to touch the internet. The Connect panel says so under
the server field.

**Direct peers** are the second option, configured on the DIRECT tab and stored
with the rest of the application state. Commsbus polls them and reconnects any
that are missing, so a peer that reboots, changes address, or drops off the
network rejoins by itself.

**Only one copy runs at a time.** A second launch activates the running window
and exits, so a copy started at login and a copy started by hand cannot fight
over the audio device. Pass `--allow-multiple` if you really do want two.

**Start at login** is a toggle in Options. On macOS it installs a per-user
launchd agent at `~/Library/LaunchAgents/org.lifenz.commsbus.plist` with
`RunAtLoad` and `KeepAlive`, so Commsbus comes back after a reboot and is
relaunched if it exits abnormally -- but not if you quit it deliberately. This is
macOS-only for now; the toggle is hidden on other platforms.

**IMPORTANT TIPS**

Commsbus does not use any echo cancellation, or automatic noise
reduction in order to maintain the highest audio quality. As a result, if you have a live microphone signal you will need to also use headphones to prevent echos and/or feedback.

For best results, and to achieve the lowest latencies, connect your computer with wired ethernet to your router if you can. Although it will work with WiFi, the added network jitter and packet loss will require you to use a bigger safety buffer to maintain a quality audio signal, which results in higher latencies.

Commsbus does NOT currently use any encryption for the data
communication, so while it is unlikely that it will be
intercepted, please keep that in mind. All audio is sent directly between users peer-to-peer, the connection server is only used so that the users in a group can find each other.


# Installing

Commsbus has no binary releases yet -- build it from source as below.

# Building

Commsbus is a fork of
[github.com/sonosaurus/sonobus](https://github.com/sonosaurus/sonobus).

To build from source on macOS and Windows, all of the dependencies are a part of this GIT repository, including prebuilt Opus libraries. 
The build now uses [CMake](https://cmake.org) 3.15 or above on macOS, Windows, and Linux platforms, see
details below.

### On macOS

Make sure you have [CMake](https://cmake.org) >= 3.15 and XCode. Then run:
```
./setupcmake.sh
./buildcmake.sh
``` 
The resulting application will end up under `build/Commsbus_artefacts/Release/Standalone`
when the build completes. If you would rather have an Xcode project to look
at, use `./setupcmakexcode.sh` instead and use the Xcode project that gets
produced at `buildXcode/Commsbus.xcodeproj`.

#### Code signing, and why it matters

macOS grants microphone and local-network access per *code-signing identity*,
not per application path. JUCE leaves the built app ad-hoc "linker-signed",
whose identity is the exact binary hash — so every rebuild looks like a brand
new application and macOS asks for every permission again.

The build therefore re-signs the app bundle, using the first
`Developer ID Application` identity it finds in your keychain. Grants then
survive rebuilds. To pick a different one:

```
cmake -DCOMMSBUS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" ...
```

Pass `"-"` for plain ad-hoc signing, or `OFF` to leave JUCE's signature alone.

**If you have no Developer ID**, a self-signed certificate works just as well on
your own machine — what matters is that the identity is *stable*, not that it is
trusted. In Keychain Access choose *Certificate Assistant → Create a
Certificate…*, name it, set the identity type to *Self Signed Root* and the
certificate type to *Code Signing*, then build with
`-DCOMMSBUS_CODESIGN_IDENTITY="<that name>"`. You will still need a Developer ID
to distribute the app to anyone else.

### On Windows

You will need [CMake](https://cmake.org) >= 3.15, and  Visual Studio 2017
installed. You'll also need Cygwin installed if you want to use the scripts
below, but you can also use CMake in other ways if you prefer.

```
./setupcmakewin.sh
./buildcmake.sh
``` 
The resulting application will end up under `build/Commsbus_artefacts/Release/Standalone`
when the build completes. The MSVC project/solution can be found in
build/Commsbus_artefacts as well after the cmake setup step.


### On Linux

The first thing to do in a terminal is go to the Linux directory:

    cd linux

And read the [BUILDING.md](linux/BUILDING.md) file for
further instructions.


# License and 3rd Party Software

Commsbus was written by Jesse Chappell, and it is licensed under the GPLv3, the full license text is in the LICENSE file. Some of the dependencies have their own more permissive licenses.

It is built using JUCE 6 (slightly modified on a public fork), and AOO (Audio over OSC), which also uses the Opus codec. I'm using the very handy tool `git-subrepo` to include the source code for my forks of those software libraries in this repository.


My github forks of these that are referenced via `git-subrepo` in this repository are:

> https://github.com/essej/JUCE  in the sono6good branch.

> https://github.com/essej/aoo.git   in the sono branch.


If you want to run your own connection server instead of using the default
one at aoo.sonobus.net, you can build the headless aooserver code at

> https://github.com/essej/aooserver

The standalone Commsbus application also provides a connection server internally,
which you can connect to on port 10999, or port forward TCP/UDP 10999 from your internet
router to the machine you are running it on.


# Thanks

Thanks for everyone involved in testing, especially to Christof Ressi for
the AOO library.

### Software development credits:

- For designing and implementing the Soundboard feature:
    - Sten Wessel
    - Hannah Schellekens

### Documentation credits:
 - Michael Eskin
 - Tony Becker

### Translation credits:
 - RelationLife (Taewook Yang)
 
