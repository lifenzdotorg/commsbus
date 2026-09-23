// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

// juce_core/juce_events rather than JuceHeader.h, like CommsbusAutoStart.h
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <cstdlib>
#include <memory>

#include "CommsbusAutoStart.h"

/**
    Restarts Commsbus if its message thread hangs.

    launchd's KeepAlive (see CommsbusAutoStart) brings the app back after a crash
    or a non-zero exit, but a hang -- a deadlock, a wedged device driver call --
    leaves the process alive and doing nothing, forever. This background thread
    posts a heartbeat to the message thread every few seconds; if none has been
    answered for hangTimeoutMs, it exits the process with a non-zero status so
    launchd starts a fresh copy.

    It only does that when the login agent is installed. Without it nothing would
    restart the app, and a hung window someone can see is better than one that
    silently vanishes.

    The exit is std::_Exit: no destructors, since the hang may be holding the very
    locks they would take. The single-instance lock is an fcntl lock, which the
    kernel drops with the process.
*/
class CommsbusWatchdog  : private juce::Thread
{
public:
    static constexpr juce::uint32 hangTimeoutMs = 60000;
    static constexpr int exitCodeOnHang = 70; // EX_SOFTWARE

    CommsbusWatchdog()
        : juce::Thread ("Commsbus watchdog"),
          state (std::make_shared<State>())
    {
        state->lastBeat = juce::Time::getMillisecondCounter();
        startThread();
    }

    ~CommsbusWatchdog() override
    {
        stopThread (3000);
    }

private:
    struct State
    {
        std::atomic<juce::uint32> lastBeat { 0 };
        std::atomic<bool> beatPending { false };
    };

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (2000);
            if (threadShouldExit())
                break;

            if (! state->beatPending.exchange (true))
            {
                // the lambda holds the state, not this, so it is safe to run
                // after the watchdog has gone
                auto st = state;
                juce::MessageManager::callAsync ([st]
                {
                    st->lastBeat = juce::Time::getMillisecondCounter();
                    st->beatPending = false;
                });
            }

            const auto silentFor = juce::Time::getMillisecondCounter() - state->lastBeat.load();

            if (silentFor > hangTimeoutMs && CommsbusAutoStart::isEnabled())
            {
                juce::Logger::outputDebugString ("Commsbus watchdog: message thread unresponsive, exiting for launchd to restart");
                std::_Exit (exitCodeOnHang);
            }
        }
    }

    std::shared_ptr<State> state;

    JUCE_DECLARE_NON_COPYABLE (CommsbusWatchdog)
};
