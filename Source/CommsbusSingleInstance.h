// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

// NB: juce_core rather than JuceHeader.h -- JuceHeader.h does a
// "using namespace juce", which makes juce::Point ambiguous with Carbon's
// ::Point in the .mm that implements this. Same reason as CrossPlatformUtils.h.
#include <juce_core/juce_core.h>

/**
 * Brings an already-running Commsbus to the front.
 *
 * Called when a second launch is refused, so that double-clicking the app when
 * launchd already started one surfaces the running window instead of appearing
 * to do nothing.
 *
 * @returns true if another instance was found and activated.
 */
bool activateExistingCommsbusInstance();
