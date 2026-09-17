// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "CommsbusAutoStart.h"

#include <juce_core/system/juce_TargetPlatform.h>

#if ! JUCE_MAC

// Start-at-login is currently implemented for macOS only (launchd user agent).
// Linux would want a systemd user unit and Windows a Run key or scheduled task;
// until then the UI hides the option rather than offering something that does
// nothing.

namespace CommsbusAutoStart
{

bool isSupported()                  { return false; }
bool isEnabled()                    { return false; }
juce::String getRegistrationPath()  { return {}; }

bool setEnabled (bool, juce::String & errorMessage)
{
    errorMessage = TRANS ("Starting automatically at login is not supported on this platform yet.");
    return false;
}

}

#endif
