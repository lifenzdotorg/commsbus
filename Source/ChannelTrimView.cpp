// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "ChannelTrimView.h"

const Array<float> & ChannelTrimView::getTrimSteps()
{
    static const Array<float> steps { -12.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 12.0f };
    return steps;
}

static String trimStepText(float db)
{
    if (db > 0.0f) return "+" + String(roundToInt(db)) + " dB";
    return String(roundToInt(db)) + " dB";
}

ChannelTrimView::ChannelTrimView(CommsbusAudioProcessor & proc)
: processor(proc)
{
    mIntroLabel = std::make_unique<Label>("intro", TRANS("Trim the level of each channel. Transmit trims apply to what is sent over the network; receive trims apply to what goes out to the local device. Changes take effect immediately and are saved."));
    mIntroLabel->setFont(Font(13));
    mIntroLabel->setColour(Label::textColourId, Colour(0xaaeeeeee));
    mIntroLabel->setJustificationType(Justification::topLeft);
    mIntroLabel->setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(mIntroLabel.get());

    setFocusContainerType(FocusContainerType::focusContainer);

    rebuild();
}

ChannelTrimView::~ChannelTrimView()
{
    stopTimer();
}

void ChannelTrimView::visibilityChanged()
{
    if (isVisible()) {
        refresh();
        startTimer(1000);
    } else {
        stopTimer();
    }
}

void ChannelTrimView::timerCallback()
{
    if (isShowing()) {
        refresh();
    }
}

String ChannelTrimView::computeSignature() const
{
    String sig;
    const int ingroups = processor.getInputGroupCount();
    sig << "I" << ingroups;
    for (int g = 0; g < ingroups; ++g) {
        int start = 0, count = 0;
        processor.getInputGroupChannelStartAndCount(g, start, count);
        sig << "|" << start << ":" << processor.getInputGroupName(g);
    }

    const int numpeers = processor.getNumberRemotePeers();
    for (int i = 0; i < numpeers; ++i) {
        if (!processor.isPeerVisible(i)) continue;
        const int groups = processor.getRemotePeerChannelGroupCount(i);
        sig << ";P" << i << ":" << processor.getRemotePeerDisplayName(i) << ":" << groups;
        for (int g = 0; g < groups; ++g) {
            sig << "|" << processor.getRemotePeerChannelGroupName(i, g);
        }
    }
    return sig;
}

void ChannelTrimView::refresh(bool forceRebuild)
{
    // never pull a dropdown out from under the operator
    for (auto * item : mItems) {
        if (item->trim && item->trim->isPopupActive()) return;
    }

    if (forceRebuild || computeSignature() != mSignature) {
        rebuild();
    } else {
        updateValues();
    }
}

ChannelTrimView::Item * ChannelTrimView::addHeader(const String & text)
{
    auto * item = mItems.add(new Item());
    item->isHeader = true;
    item->label = std::make_unique<Label>("hdr", text);
    item->label->setFont(Font(15, Font::bold));
    item->label->setColour(Label::textColourId, Colour(0xeeeeeeee));
    item->label->setJustificationType(Justification::bottomLeft);
    addAndMakeVisible(item->label.get());
    return item;
}

ChannelTrimView::Item * ChannelTrimView::addRow(const String & text, int peerIndex, int group, const String & peerName)
{
    auto * item = mItems.add(new Item());
    item->peerIndex = peerIndex;
    item->group = group;
    item->peerName = peerName;

    item->label = std::make_unique<Label>("name", text);
    item->label->setFont(Font(14));
    item->label->setJustificationType(Justification::centredLeft);
    item->label->setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(item->label.get());

    item->trim = std::make_unique<ComboBox>("trim");
    item->trim->setTitle(TRANS("Trim") + " " + text);
    item->trim->setTooltip(TRANS("Level trim for this channel"));
    const auto & steps = getTrimSteps();
    for (int s = 0; s < steps.size(); ++s) {
        item->trim->addItem(trimStepText(steps[s]), s + 1);
    }
    item->trim->setWantsKeyboardFocus(true);
    item->trim->onChange = [this, item]() { trimChanged(item); };
    addAndMakeVisible(item->trim.get());

    return item;
}

void ChannelTrimView::rebuild()
{
    mItems.clear();
    mSignature = computeSignature();

    // TRANSMIT: the local input groups
    addHeader(TRANS("TRANSMIT"));

    const int ingroups = processor.getInputGroupCount();
    for (int g = 0; g < ingroups; ++g) {
        int start = 0, count = 0;
        processor.getInputGroupChannelStartAndCount(g, start, count);
        String text;
        if (count > 1) {
            text << TRANS("In") << " " << (start + 1) << "-" << (start + count);
        } else {
            text << TRANS("In") << " " << (start + 1);
        }
        const auto name = processor.getInputGroupName(g);
        if (name.isNotEmpty()) {
            text << "  " << name;
        }
        addRow(text, -1, g, {});
    }
    if (ingroups == 0) {
        addHeader(TRANS("No inputs"))->label->setFont(Font(14, Font::italic));
    }

    // RECEIVE: each visible peer's received groups
    bool anypeer = false;
    const int numpeers = processor.getNumberRemotePeers();
    for (int i = 0; i < numpeers; ++i) {
        if (!processor.isPeerVisible(i)) continue;
        anypeer = true;

        const auto peername = processor.getRemotePeerDisplayName(i);
        addHeader(TRANS("RECEIVE from") + " " + (peername.isNotEmpty() ? peername : TRANS("(connecting)")));

        const int groups = processor.getRemotePeerChannelGroupCount(i);
        for (int g = 0; g < groups; ++g) {
            String text;
            text << TRANS("Stream") << " " << (g + 1);
            const auto name = processor.getRemotePeerChannelGroupName(i, g);
            if (name.isNotEmpty()) {
                text << "  " << name;
            }
            addRow(text, i, g, peername);
        }
    }
    if (!anypeer) {
        addHeader(TRANS("RECEIVE"));
        addHeader(TRANS("Nobody connected"))->label->setFont(Font(14, Font::italic));
    }

    updateValues();

    // size to the content; the width comes from the viewport (OptionsView)
    int height = introHeight + 8;
    for (auto * item : mItems) {
        height += item->isHeader ? headerHeight : rowHeight;
    }
    setSize(jmax(getWidth(), 200), height + 10);
    resized();
}

float ChannelTrimView::getGainFor(const Item & item) const
{
    if (item.peerIndex < 0) {
        return processor.getInputGroupGain(item.group);
    }
    return processor.getRemotePeerChannelGain(item.peerIndex, item.group);
}

void ChannelTrimView::updateValues()
{
    const auto & steps = getTrimSteps();

    for (auto * item : mItems) {
        if (item->isHeader || !item->trim || item->trim->isPopupActive()) continue;

        const float db = Decibels::gainToDecibels(getGainFor(*item));

        int match = 0;
        for (int s = 0; s < steps.size(); ++s) {
            if (std::abs(steps[s] - db) < 0.05f) {
                match = s + 1;
                break;
            }
        }

        if (match > 0) {
            if (item->trim->getSelectedId() != match) {
                item->trim->setSelectedId(match, dontSendNotification);
            }
        } else {
            // set some other way (old saved state, or the legacy slider): show
            // the real value rather than pretend it is one of the steps
            const auto text = Decibels::toString(db, 1);
            if (item->trim->getText() != text) {
                item->trim->setText(text, dontSendNotification);
            }
        }
    }
}

void ChannelTrimView::trimChanged(Item * item)
{
    const int id = item->trim->getSelectedId();
    const auto & steps = getTrimSteps();
    if (!isPositiveAndBelow(id - 1, steps.size())) return;

    const float gain = Decibels::decibelsToGain(steps[id - 1]);

    if (item->peerIndex < 0) {
        if (item->group < processor.getInputGroupCount()) {
            processor.setInputGroupGain(item->group, gain);
        }
    }
    else {
        // the peer list may have shifted since the rows were built
        if (item->peerIndex < processor.getNumberRemotePeers()
            && processor.getRemotePeerDisplayName(item->peerIndex) == item->peerName
            && item->group < processor.getRemotePeerChannelGroupCount(item->peerIndex)) {
            processor.setRemotePeerChannelGain(item->peerIndex, item->group, gain);
        }
        else {
            // rebuild after this callback returns, since it deletes the combo
            Component::SafePointer<ChannelTrimView> safeThis(this);
            MessageManager::callAsync([safeThis]() { if (safeThis) safeThis->refresh(true); });
        }
    }
}

void ChannelTrimView::paint(Graphics & g)
{
    // faint separators between rows
    g.setColour(Colour(0x22ffffff));
    for (auto * item : mItems) {
        if (!item->isHeader && item->label) {
            auto b = item->label->getBounds();
            g.fillRect(b.getX(), b.getBottom() + 1, getWidth() - b.getX() - 8, 1);
        }
    }
}

void ChannelTrimView::resized()
{
    auto bounds = getLocalBounds().reduced(8, 0);
    bounds.removeFromTop(6);
    mIntroLabel->setBounds(bounds.removeFromTop(introHeight));
    bounds.removeFromTop(2);

    const int trimwidth = 110;

    for (auto * item : mItems) {
        if (item->isHeader) {
            item->label->setBounds(bounds.removeFromTop(headerHeight).withTrimmedBottom(4));
        }
        else {
            auto row = bounds.removeFromTop(rowHeight).reduced(0, 3);
            row.removeFromLeft(14); // indent under the header
            item->trim->setBounds(row.removeFromRight(trimwidth));
            row.removeFromRight(8);
            item->label->setBounds(row);
        }
    }
}
