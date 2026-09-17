// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell

#pragma once


class CommsbusCommands
{
public:
    
    enum {
        MuteAllInput = 1,
        MuteAllPeers,
        Connect,
        Disconnect,
        ShowOptions,
        RecordToggle,
        CheckForNewVersion,
        LoadSetupFile,
        SaveSetupFile,
        ChatToggle,
        ShowViewMenu,
        ShowConnectMenu,
        ShowGroupMenu,
        ToggleFullInfoView,
        ToggleAllMonitorDelay,
        CopyGroupLink,
        GroupLatencyMatch,
        VDONinjaVideoLink,
        SuggestNewGroup,
        ResetAllJitterBuffers
    };
    
};
