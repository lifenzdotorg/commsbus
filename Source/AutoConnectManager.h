// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

class CommsbusAudioProcessor;

/**
 * A peer Commsbus should stay directly connected to, by address.
 *
 * This is deliberately independent of the group/rendezvous server: an entry is
 * just a host and port, so a fixed set of machines can find each other with no
 * infrastructure beyond the network between them.
 */
struct DirectPeerEntry
{
    DirectPeerEntry() = default;
    DirectPeerEntry (const juce::String & host_, int port_, const juce::String & name_ = {})
        : host (host_), port (port_), name (name_) {}

    juce::String host;
    int          port = 0;
    juce::String name;      // optional label shown in the UI
    bool         enabled = true;

    bool isValid() const    { return host.isNotEmpty() && port > 0; }

    bool operator== (const DirectPeerEntry & o) const
    {
        return host == o.host && port == o.port;
    }

    juce::ValueTree getValueTree() const;
    void setFromValueTree (const juce::ValueTree & v);
};

/**
 * Keeps Commsbus connected to its configured direct peers.
 *
 * The manager polls on a timer rather than reacting to connect/disconnect
 * events. That is the whole point: a poll recovers from every way a link can go
 * away -- the peer rebooting, a cable being pulled, DHCP handing out a new
 * address, Commsbus itself restarting -- without needing an event for each case.
 * Reconnection backs off so a permanently-absent peer does not generate traffic
 * forever, and resets as soon as the peer reappears.
 */
class AutoConnectManager : private juce::Timer
{
public:
    explicit AutoConnectManager (CommsbusAudioProcessor & processor);
    ~AutoConnectManager() override;

    /** Begin polling. Safe to call repeatedly. */
    void start();

    /** Stop polling. Existing connections are left alone. */
    void stop();

    bool isRunning() const                      { return isTimerRunning(); }

    /** When true, start() is called automatically once the app has finished launching. */
    bool getAutoConnectOnLaunch() const         { return autoConnectOnLaunch; }
    void setAutoConnectOnLaunch (bool b)        { autoConnectOnLaunch = b; }

    //==============================================================================
    int getNumPeers() const                     { return peers.size(); }
    DirectPeerEntry getPeer (int index) const;
    void setPeers (const juce::Array<DirectPeerEntry> & newPeers);
    juce::Array<DirectPeerEntry> getPeers() const;

    /** Adds a peer if no entry with the same host/port exists. Returns true if added. */
    bool addPeer (const DirectPeerEntry & entry);
    bool removePeer (int index);

    /** Forget all backoff state and try every enabled peer on the next tick. */
    void retryNow();

    //==============================================================================
    juce::ValueTree getValueTree() const;
    void setFromValueTree (const juce::ValueTree & v);

    static const char * directPeersKey;

private:
    void timerCallback() override;
    void attemptConnections();

    /** True if this address already has a peer slot in the processor. */
    bool isPeerPresent (const DirectPeerEntry & entry) const;

    struct RetryState
    {
        int  failures = 0;
        juce::uint32 nextAttemptMs = 0;
    };

    CommsbusAudioProcessor & processor;

    juce::Array<DirectPeerEntry> peers;
    juce::Array<RetryState>      retryStates;   // parallel to peers
    mutable juce::CriticalSection peersLock;

    bool autoConnectOnLaunch = true;

    static constexpr int pollIntervalMs   = 2000;
    static constexpr int baseBackoffMs    = 2000;
    static constexpr int maxBackoffMs     = 30000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoConnectManager)
};
