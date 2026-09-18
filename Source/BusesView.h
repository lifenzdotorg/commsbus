// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

#include "CommsbusAudioProcessor.h"
#include "SonoLookAndFeel.h"
#include "SonoDrawableButton.h"
#include "GenericItemChooser.h"


/**
 * One row of the BUSES panel: name, how many incoming streams feed it, its
 * master level, and the device (Dante) output channel it lands on.
 */
class BusRowView : public Component
{
public:
    BusRowView();
    virtual ~BusRowView();

    void paint(Graphics & g) override;
    void resized() override;

    int busIndex = 0;

    SonoBigTextLookAndFeel sonoSliderLNF;

    std::unique_ptr<Label>      nameLabel;
    std::unique_ptr<Label>      sourcesLabel;
    std::unique_ptr<Slider>     levelSlider;
    std::unique_ptr<TextButton> destButton;
    std::unique_ptr<SonoDrawableButton> removeButton;

    FlexBox mainbox;

    Colour borderColor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BusRowView)
};


/**
 * The receive-side bus panel.
 *
 * Buses are created from a received stream's destination menu (that is where the
 * routing decision is made); this panel is where they are named, levelled, given
 * their Dante output channel, and removed. Removing a bus sends everything that
 * was feeding it straight out again -- see CommsbusAudioProcessor::removeOutputBus.
 */
class BusesView : public Component,
public Slider::Listener,
public Label::Listener
{
public:
    BusesView(CommsbusAudioProcessor & proc);
    virtual ~BusesView();

    class Listener {
    public:
        virtual ~Listener() {}
        virtual void busLayoutChanged(BusesView * comp) {}
    };

    void addListener(Listener * listener) { listeners.add(listener); }
    void removeListener(Listener * listener) { listeners.remove(listener); }

    void paint(Graphics & g) override;
    void resized() override;

    void sliderValueChanged(Slider * slider) override;
    void labelTextChanged(Label * label) override;

    void rebuildBusViews();
    void updateBusViews();

    /**
     * Picks up buses added or removed elsewhere -- the receive destination menu
     * can create one. Cheap enough to call from the editor's periodic timer.
     */
    void refreshIfBusesChanged();

    void updateLayout(bool notify = true);

    Rectangle<int> getMinimumContentBounds() const;

    void setNarrowMode(bool flag, bool update = false);

    std::function<AudioDeviceManager*()> getAudioDeviceManager;

protected:

    void addBusPressed();
    void removeBusPressed(int index);
    void showBusDestSelectionMenu(Component * source, int index);
    void configLevelSlider(Slider * slider);

    /** How many received channel groups, across all peers, feed this bus. */
    int countSourcesForBus(int busIndex) const;

    CommsbusAudioProcessor & processor;

    OwnedArray<BusRowView> mBusViews;

    std::unique_ptr<TextButton> mAddButton;

    SonoBigTextLookAndFeel addLnf;

    FlexBox busesBox;

    int  mLastBusCount = -1;
    int  mMinHeight = 0;
    int  mMinWidth = 0;
    bool isNarrow = false;

    Colour bgColor;
    Colour outlineColor;
    Colour regularTextColor;
    Colour dimTextColor;

    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BusesView)
};
