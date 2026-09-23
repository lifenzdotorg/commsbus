// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "MonitorOutput.h"

const String MonitorOutput::noneId ("none");
const String MonitorOutput::defaultId ("default");

// How much audio the FIFO aims to hold, on top of one monitor block. Enough to
// ride out the two callbacks landing at unrelated moments; small enough that a
// soloed stream still feels immediate.
static constexpr double targetLatencySeconds = 0.015;

// Largest speed correction applied to absorb clock drift (0.4%, inaudible on a
// listening feed).
static constexpr double maxSpeedCorrection = 0.004;


MonitorOutput::MonitorOutput()
{
}

MonitorOutput::~MonitorOutput()
{
    close();
}

bool MonitorOutput::looksLikeDante (const String & name)
{
    return name.containsIgnoreCase("dante") || name.containsIgnoreCase("DVS");
}

AudioIODeviceType * MonitorOutput::getDeviceType()
{
    auto & types = mTypeHolder.getAvailableDeviceTypes();
    if (types.isEmpty()) return nullptr;

    auto * type = types.getFirst();
    type->scanForDevices();
    return type;
}

StringArray MonitorOutput::getOutputDeviceNames (const String & mainDeviceName)
{
    StringArray names;
    if (auto * type = getDeviceType()) {
        for (auto & name : type->getDeviceNames(false)) {
            if (name != mainDeviceName) {
                names.add(name);
            }
        }
    }
    return names;
}

void MonitorOutput::close()
{
    if (mDevice) {
        mDevice->stop();
        mDevice->close();
        mDevice.reset();
    }
    mOpen = false;
}

void MonitorOutput::setDevice (const String & deviceId, const String & mainDeviceName, double sampleRate)
{
    close();

    if (deviceId == noneId) return;

    auto * type = getDeviceType();
    if (!type) return;

    const auto names = type->getDeviceNames(false);
    String name;

    if (deviceId == defaultId || deviceId.isEmpty()) {
        // The system default output, unless that is the main device or looks like
        // Dante -- then the first output that is neither. Nothing suitable means no
        // monitor, rather than risking the Dante network.
        const int defindex = type->getDefaultDeviceIndex(false);
        if (isPositiveAndBelow(defindex, names.size())
            && names[defindex] != mainDeviceName && !looksLikeDante(names[defindex])) {
            name = names[defindex];
        }
        else {
            for (auto & n : names) {
                if (n != mainDeviceName && !looksLikeDante(n)) {
                    name = n;
                    break;
                }
            }
        }
    }
    else if (names.contains(deviceId)) {
        name = deviceId;
    }

    if (name.isEmpty()) return;

    std::unique_ptr<AudioIODevice> device (type->createDevice(name, {}));
    if (!device) return;

    const auto numouts = device->getOutputChannelNames().size();
    if (numouts <= 0) return;

    BigInteger outs;
    outs.setRange(0, jmin(2, numouts), true);

    // Match the main device's rate when the monitor device can, so the
    // interpolator is only correcting drift; otherwise let it run at its own.
    double rate = 0.0;
    if (sampleRate > 0.0 && device->getAvailableSampleRates().contains(sampleRate)) {
        rate = sampleRate;
    }

    const auto err = device->open({}, outs, rate, device->getDefaultBufferSize());
    if (err.isNotEmpty()) {
        DBG("Monitor output failed to open " << name << ": " << err);
        return;
    }

    mDevice = std::move(device);
    mNeedsReset = true;
    mDevice->start(this);
    mOpen = mDevice->isPlaying();
}

String MonitorOutput::getOpenDeviceName() const
{
    return (mDevice && mOpen.load()) ? mDevice->getName() : String();
}

void MonitorOutput::setSourceSampleRate (double sampleRate)
{
    if (sampleRate > 0.0 && sampleRate != mSourceRate.load()) {
        mSourceRate = sampleRate;
        mNeedsReset = true;
    }
}

void MonitorOutput::pushSamples (const AudioBuffer<float> & buffer, int numSamples)
{
    if (!mOpen.load() || numSamples <= 0 || buffer.getNumChannels() == 0) return;

    if (mFifo.getFreeSpace() < numSamples) return; // monitor stalled; drop rather than block

    int start1, size1, start2, size2;
    mFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < 2; ++ch) {
        const int srcch = jmin(ch, buffer.getNumChannels() - 1);
        if (size1 > 0) mFifoBuffer.copyFrom(ch, start1, buffer, srcch, 0, size1);
        if (size2 > 0) mFifoBuffer.copyFrom(ch, start2, buffer, srcch, size1, size2);
    }

    mFifo.finishedWrite(size1 + size2);
}

void MonitorOutput::audioDeviceAboutToStart (AudioIODevice * device)
{
    mDestRate = device->getCurrentSampleRate();
    mDestBlock = device->getCurrentBufferSizeSamples();
    mNeedsReset = true;
}

void MonitorOutput::audioDeviceStopped()
{
    mOpen = false;
}

void MonitorOutput::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                      float* const* outputChannelData, int numOutputChannels,
                                                      int numSamples, const AudioIODeviceCallbackContext &)
{
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (outputChannelData[ch]) FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }

    if (numOutputChannels <= 0 || mDestRate <= 0.0) return;

    const double baseratio = mSourceRate.load() / mDestRate;
    const double target = mSourceRate.load() * targetLatencySeconds + mDestBlock * baseratio;

    if (mNeedsReset.exchange(false)) {
        mFifo.finishedRead(mFifo.getNumReady()); // flush
        mInterp[0].reset();
        mInterp[1].reset();
        mPriming = true;
    }

    int ready = mFifo.getNumReady();

    if (mPriming) {
        if (ready < target) return;
        mPriming = false;
        mSmoothedFill = ready;
    }

    // after a stall on the main side the FIFO can be far too full: drop the excess
    if (ready > 4.0 * target) {
        mFifo.finishedRead(ready - (int) target);
        ready = mFifo.getNumReady();
        mSmoothedFill = ready;
    }

    mSmoothedFill += 0.05 * (ready - mSmoothedFill);
    const double error = (mSmoothedFill - target) / jmax(1.0, target);
    const double speed = baseratio * (1.0 + jlimit(-maxSpeedCorrection, maxSpeedCorrection, error * maxSpeedCorrection));

    const int needed = (int) std::ceil(numSamples * speed) + 8;
    if (needed > mScratch.getNumSamples()) return;

    if (ready < needed) {
        // underrun: go quiet and refill
        mInterp[0].reset();
        mInterp[1].reset();
        mPriming = true;
        return;
    }

    int start1, size1, start2, size2;
    mFifo.prepareToRead(needed, start1, size1, start2, size2);
    for (int ch = 0; ch < 2; ++ch) {
        if (size1 > 0) mScratch.copyFrom(ch, 0, mFifoBuffer, ch, start1, size1);
        if (size2 > 0) mScratch.copyFrom(ch, size1, mFifoBuffer, ch, start2, size2);
    }

    int used = 0;
    if (numOutputChannels >= 2 && outputChannelData[0] && outputChannelData[1]) {
        used = mInterp[0].process(speed, mScratch.getReadPointer(0), outputChannelData[0], numSamples, needed, 0);
        mInterp[1].process(speed, mScratch.getReadPointer(1), outputChannelData[1], numSamples, needed, 0);
    }
    else if (outputChannelData[0]) {
        // mono device: fold the two sides together
        mScratch.addFrom(0, 0, mScratch, 1, 0, needed);
        mScratch.applyGain(0, 0, needed, 0.5f);
        used = mInterp[0].process(speed, mScratch.getReadPointer(0), outputChannelData[0], numSamples, needed, 0);
        mInterp[1].reset();
    }

    mFifo.finishedRead(jlimit(0, needed, used));
}
