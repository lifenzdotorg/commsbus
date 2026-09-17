// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "CommsbusSingleInstance.h"

#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_MAC

#import <Cocoa/Cocoa.h>

bool activateExistingCommsbusInstance()
{
    @autoreleasepool
    {
        NSString * bundleId = [[NSBundle mainBundle] bundleIdentifier];

        if (bundleId == nil)
            return false;

        NSRunningApplication * self_ = [NSRunningApplication currentApplication];
        NSArray<NSRunningApplication *> * others =
            [NSRunningApplication runningApplicationsWithBundleIdentifier: bundleId];

        for (NSRunningApplication * app in others)
        {
            if ([app isEqual: self_] || [app isTerminated])
                continue;

            // -activate arrived in macOS 14 and supersedes -activateWithOptions:.
            // Probe for it so this compiles and behaves correctly on both.
            if ([app respondsToSelector: @selector (activate)])
            {
                [app performSelector: @selector (activate)];
            }
            else
            {
                JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
                [app activateWithOptions: NSApplicationActivateAllWindows];
                JUCE_END_IGNORE_WARNINGS_GCC_LIKE
            }

            return true;
        }
    }

    return false;
}

#endif
