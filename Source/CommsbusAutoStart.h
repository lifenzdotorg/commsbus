// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

// NB: deliberately juce_core rather than JuceHeader.h. JuceHeader.h does a
// "using namespace juce", which makes juce::Point ambiguous with Carbon's
// ::Point in any .mm that imports Cocoa -- including this header's macOS
// implementation. CrossPlatformUtils.h avoids JuceHeader.h for the same reason.
#include <juce_core/juce_core.h>

/**
 * Start-at-login / keep-running support.
 *
 * On macOS this is backed by a per-user launchd agent written to
 * ~/Library/LaunchAgents. Using launchd rather than a plain login item buys two
 * things Commsbus specifically wants: the app comes back after a reboot without
 * anyone logging into the UI and clicking something, and launchd relaunches it
 * if it dies unexpectedly (but not if the operator quits it deliberately).
 *
 * Every function is safe to call on platforms where this is unimplemented;
 * isSupported() returns false there and setEnabled() fails with a message.
 */
namespace CommsbusAutoStart
{
    /** True if start-at-login can be configured on this platform/build. */
    bool isSupported();

    /** True if the agent is currently installed. */
    bool isEnabled();

    /**
     * Installs or removes the launch agent.
     * @param shouldBeEnabled   desired state
     * @param errorMessage      set to a human-readable reason when this returns false
     * @returns true on success
     */
    bool setEnabled (bool shouldBeEnabled, juce::String & errorMessage);

    /** Path of the backing agent/registration, for display in the UI. Empty if unsupported. */
    juce::String getRegistrationPath();
}
