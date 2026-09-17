// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "CommsbusSingleInstance.h"

#include <juce_core/system/juce_TargetPlatform.h>

#if ! JUCE_MAC

// The single-instance lock itself is portable (juce::InterProcessLock); only
// raising the existing window is platform-specific, and that is macOS-only for
// now. Elsewhere the second instance just exits quietly.
bool activateExistingCommsbusInstance()
{
    return false;
}

#endif
