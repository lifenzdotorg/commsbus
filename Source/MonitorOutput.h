// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

#include <atomic>

/**
    A second, independent audio output device for local listening -- the
    monitor / solo output.

    Commsbus's main audio device is normally Dante Virtual Soundcard, so
    everything written to it lands on the Dante network. Anything that is only
    for the operator's ears (a soloed stream, the local inputs) goes here
    instead, typically to the built-in speakers or headphones, and can never
    leak onto a Dante output.

    The two devices run on unrelated clocks. The main audio callback pushes a
    stereo stream into a lock-free FIFO, and this device's own callback pulls it
    back out through an interpolator whose speed is nudged by the FIFO fill
    level, so drift between the clocks is absorbed without clicks. A different
    sample rate on the monitor device is handled by the same resampling.
*/
class MonitorOutput : private AudioIODeviceCallback
{
public:
    /** Device ids understood by setDevice(), besides a literal device name. */
    static const String noneId;     // monitor output off
    static const String defaultId;  // the system default output, if it isn't the main device

    MonitorOutput();
    ~MonitorOutput() override;

    //== message thread ==================================================

    /** Output device names available to the monitor, excluding the main device. */
    StringArray getOutputDeviceNames (const String & mainDeviceName);

    /** Opens (or closes, for noneId) the monitor device. The main device is never
        chosen by defaultId, so the default can never route onto Dante. */
    void setDevice (const String & deviceId, const String & mainDeviceName, double sampleRate);

    void close();

    /** Name of the device actually open, or empty. */
    String getOpenDeviceName() const;
    bool isOpen() const { return mOpen.load(); }

    //== main audio thread ===============================================

    /** Called from the main device's prepareToPlay. */
    void setSourceSampleRate (double sampleRate);

    /** Feeds the first two channels of buffer to the monitor. Lock-free; drops
        the audio if the monitor is closed or the FIFO is full. */
    void pushSamples (const AudioBuffer<float> & buffer, int numSamples);

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const AudioIODeviceCallbackContext & context) override;
    void audioDeviceAboutToStart (AudioIODevice * device) override;
    void audioDeviceStopped() override;

    static bool looksLikeDante (const String & name);
    AudioIODeviceType * getDeviceType();

    AudioDeviceManager mTypeHolder; // only used as a factory for the platform device types
    std::unique_ptr<AudioIODevice> mDevice;

    static constexpr int fifoSize = 1 << 15;
    AbstractFifo mFifo { fifoSize };
    AudioBuffer<float> mFifoBuffer { 2, fifoSize };

    // monitor-device-callback state
    AudioBuffer<float> mScratch { 2, 8192 };
    LagrangeInterpolator mInterp[2];
    bool mPriming = true;
    double mSmoothedFill = 0.0;
    double mDestRate = 48000.0;
    int mDestBlock = 512;

    std::atomic<double> mSourceRate { 48000.0 };
    std::atomic<bool> mOpen { false };
    std::atomic<bool> mNeedsReset { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorOutput)
};
