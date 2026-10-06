// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

#include "CommsbusAudioProcessor.h"

/**
 * The CHANNELS settings tab: one trim dropdown per channel group, for every
 * local input group (transmit) and every received group of every visible peer
 * (receive). It replaces the level sliders that used to sit on the channel
 * strips -- the operator picks a fixed trim, the way a Dante Controller user
 * would, rather than riding a fader.
 *
 * The choice writes the group's ChannelGroupParams gain through the processor
 * (setInputGroupGain / setRemotePeerChannelGain), so it persists with the input
 * groups and the peer cache exactly as the old sliders did. The list rebuilds
 * itself whenever groups or peers change.
 */
class ChannelTrimView : public Component, private Timer
{
public:
    explicit ChannelTrimView(CommsbusAudioProcessor & proc);
    ~ChannelTrimView() override;

    /** Rebuilds the rows if the groups or peers changed, then refreshes values. */
    void refresh(bool forceRebuild = false);

    void resized() override;
    void paint(Graphics & g) override;
    void visibilityChanged() override;

    /** The trim steps offered, in dB. */
    static const Array<float> & getTrimSteps();

private:
    struct Item
    {
        bool isHeader = false;
        int peerIndex = -1;          // -1 for a local input group
        int group = 0;
        String peerName;             // to detect the peer list shifting under us
        std::unique_ptr<Label> label;
        std::unique_ptr<ComboBox> trim;
    };

    void timerCallback() override;

    String computeSignature() const;
    void rebuild();
    void updateValues();
    void trimChanged(Item * item);

    float getGainFor(const Item & item) const;

    Item * addHeader(const String & text);
    Item * addRow(const String & text, int peerIndex, int group, const String & peerName);

    CommsbusAudioProcessor & processor;

    std::unique_ptr<Label> mIntroLabel;
    OwnedArray<Item> mItems;
    String mSignature;

    static constexpr int headerHeight = 30;
    static constexpr int rowHeight = 32;
    static constexpr int introHeight = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelTrimView)
};
