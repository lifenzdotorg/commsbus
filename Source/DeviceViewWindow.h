// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

#include "CommsbusAudioProcessor.h"
#include "RoutingMatrixView.h"


/**
 * Dante Controller style Device View, one window per device, opened by
 * double-clicking a device header in the routing matrix.
 *
 * For this machine ("local" device) it has a Receive tab -- each output channel,
 * what is connected to it (stream@peer), a status tick, an Unsubscribe button,
 * and on the right an Available Channels tree of peers and their streams that
 * can be dragged onto a receive channel to patch it -- and a Transmit tab
 * listing the local input groups sent to the far end.
 *
 * For a peer it shows a read-only Transmit tab: that peer's streams and where
 * each one is patched here. (Its own receive routing lives at its end.)
 *
 * A peer window follows its peer by name, since peer indices shift as peers
 * come and go.
 */
class DeviceViewWindow : public DocumentWindow
{
public:
    /** peerName empty means this (local) device. */
    DeviceViewWindow(CommsbusAudioProcessor & proc, const juce::String & peerName,
                     std::function<AudioDeviceManager*()> getADM);
    ~DeviceViewWindow() override;

    void closeButtonPressed() override;

    /** Re-reads routing, peers and outputs. Called from the editor's timer and
        after any routing change made elsewhere. */
    void refresh();

    bool isLocalDevice() const { return mPeerName.isEmpty(); }
    const juce::String & getPeerName() const { return mPeerName; }

    /** Called after this window changed a patch. */
    std::function<void()> onRoutingChanged;

    /** Called when the close button is pressed; the owner deletes the window
        (asynchronously, not from inside this call). */
    std::function<void(DeviceViewWindow *)> onCloseRequested;

    class Content;

private:
    juce::String mPeerName;
    Content * mContent = nullptr; // owned by the window

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceViewWindow)
};
