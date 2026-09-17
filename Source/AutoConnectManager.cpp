// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "AutoConnectManager.h"
#include "CommsbusAudioProcessor.h"

using namespace juce;

const char * AutoConnectManager::directPeersKey = "DirectPeers";

static const Identifier directPeerKey       ("DirectPeer");
static const Identifier directPeerHostKey   ("host");
static const Identifier directPeerPortKey   ("port");
static const Identifier directPeerNameKey   ("name");
static const Identifier directPeerEnabledKey("enabled");
static const Identifier autoConnectKey      ("autoConnectOnLaunch");

//==============================================================================
ValueTree DirectPeerEntry::getValueTree() const
{
    ValueTree v (directPeerKey);
    v.setProperty (directPeerHostKey,    host,    nullptr);
    v.setProperty (directPeerPortKey,    port,    nullptr);
    v.setProperty (directPeerNameKey,    name,    nullptr);
    v.setProperty (directPeerEnabledKey, enabled, nullptr);
    return v;
}

void DirectPeerEntry::setFromValueTree (const ValueTree & v)
{
    host    = v.getProperty (directPeerHostKey, host).toString();
    port    = v.getProperty (directPeerPortKey, port);
    name    = v.getProperty (directPeerNameKey, name).toString();
    enabled = v.getProperty (directPeerEnabledKey, enabled);
}

//==============================================================================
AutoConnectManager::AutoConnectManager (CommsbusAudioProcessor & proc)
    : processor (proc)
{
}

AutoConnectManager::~AutoConnectManager()
{
    stopTimer();
}

void AutoConnectManager::start()
{
    if (! isTimerRunning())
        startTimer (pollIntervalMs);
}

void AutoConnectManager::stop()
{
    stopTimer();
}

DirectPeerEntry AutoConnectManager::getPeer (int index) const
{
    const ScopedLock sl (peersLock);
    return isPositiveAndBelow (index, peers.size()) ? peers.getReference (index) : DirectPeerEntry();
}

Array<DirectPeerEntry> AutoConnectManager::getPeers() const
{
    const ScopedLock sl (peersLock);
    return peers;
}

void AutoConnectManager::setPeers (const Array<DirectPeerEntry> & newPeers)
{
    const ScopedLock sl (peersLock);
    peers = newPeers;
    retryStates.clearQuick();
    retryStates.resize (peers.size());
}

bool AutoConnectManager::addPeer (const DirectPeerEntry & entry)
{
    if (! entry.isValid())
        return false;

    const ScopedLock sl (peersLock);

    if (peers.contains (entry))
        return false;

    peers.add (entry);
    retryStates.resize (peers.size());
    return true;
}

bool AutoConnectManager::removePeer (int index)
{
    const ScopedLock sl (peersLock);

    if (! isPositiveAndBelow (index, peers.size()))
        return false;

    peers.remove (index);
    retryStates.remove (index);
    return true;
}

void AutoConnectManager::retryNow()
{
    const ScopedLock sl (peersLock);

    for (auto & rs : retryStates)
    {
        rs.failures = 0;
        rs.nextAttemptMs = 0;
    }
}

//==============================================================================
bool AutoConnectManager::isPeerPresent (const DirectPeerEntry & entry) const
{
    const int num = processor.getNumberRemotePeers();

    for (int i = 0; i < num; ++i)
    {
        String host;
        int port = 0;

        if (processor.getRemotePeerAddressInfo (i, host, port)
             && port == entry.port
             && host == entry.host)
        {
            return true;
        }
    }

    return false;
}

void AutoConnectManager::attemptConnections()
{
    // Snapshot under the lock, then act without it: connectRemotePeer() reaches
    // into the processor's own locking and must not be called with ours held.
    Array<DirectPeerEntry> snapshot;
    Array<RetryState>      states;
    {
        const ScopedLock sl (peersLock);
        snapshot = peers;
        states   = retryStates;
    }

    const auto now = Time::getMillisecondCounter();
    bool statesChanged = false;

    for (int i = 0; i < snapshot.size(); ++i)
    {
        const auto & entry = snapshot.getReference (i);

        if (! entry.enabled || ! entry.isValid())
            continue;

        auto & rs = states.getReference (i);

        if (isPeerPresent (entry))
        {
            // Connected (or at least known): clear any accumulated backoff so a
            // later drop is retried immediately rather than at the old interval.
            if (rs.failures != 0 || rs.nextAttemptMs != 0)
            {
                rs.failures = 0;
                rs.nextAttemptMs = 0;
                statesChanged = true;
            }
            continue;
        }

        if (now < rs.nextAttemptMs)
            continue;

        processor.connectRemotePeer (entry.host, entry.port, entry.name, {}, true);

        rs.failures = jmin (rs.failures + 1, 16);
        const int backoff = jmin (maxBackoffMs, baseBackoffMs * (1 << jmin (rs.failures - 1, 5)));
        rs.nextAttemptMs = now + (uint32) backoff;
        statesChanged = true;
    }

    if (statesChanged)
    {
        const ScopedLock sl (peersLock);

        // The peer list may have been edited while we were unlocked; only copy
        // back retry state if it still lines up.
        if (states.size() == retryStates.size())
            retryStates = states;
    }
}

void AutoConnectManager::timerCallback()
{
    attemptConnections();
}

//==============================================================================
ValueTree AutoConnectManager::getValueTree() const
{
    const ScopedLock sl (peersLock);

    ValueTree v (directPeersKey);
    v.setProperty (autoConnectKey, autoConnectOnLaunch, nullptr);

    for (const auto & p : peers)
        v.appendChild (p.getValueTree(), nullptr);

    return v;
}

void AutoConnectManager::setFromValueTree (const ValueTree & v)
{
    if (! v.isValid())
        return;

    Array<DirectPeerEntry> loaded;

    for (int i = 0; i < v.getNumChildren(); ++i)
    {
        auto child = v.getChild (i);

        if (! child.hasType (directPeerKey))
            continue;

        DirectPeerEntry entry;
        entry.setFromValueTree (child);

        if (entry.isValid())
            loaded.add (entry);
    }

    {
        const ScopedLock sl (peersLock);
        autoConnectOnLaunch = v.getProperty (autoConnectKey, autoConnectOnLaunch);
    }

    setPeers (loaded);
}
