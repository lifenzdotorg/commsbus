// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "BusesView.h"


namespace {

struct BusDestItemData : public GenericItemChooserItem::UserData
{
public:
    BusDestItemData(const BusDestItemData & other) : startIndex(other.startIndex), count(other.count) {}
    BusDestItemData(int start, int cnt) : startIndex(start), count(cnt) {}

    int startIndex;
    int count;
};

}


BusRowView::BusRowView() : sonoSliderLNF(12)
{
    borderColor = Colour::fromFloatRGBA(0.25f, 0.25f, 0.25f, 1.0f);
}

BusRowView::~BusRowView()
{
}

void BusRowView::paint(Graphics & g)
{
    g.setColour(borderColor);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f, 0.5f), 6.0f, 0.5f);
}

void BusRowView::resized()
{
    mainbox.performLayout(getLocalBounds().reduced(2, 1));

    if (nameLabel) {
        nameLabel->setFont(Font(nameLabel->getHeight() * 0.6f));
    }
}


BusesView::BusesView(CommsbusAudioProcessor & proc)
 : Component("busesview"), addLnf(20), processor(proc)
{
    bgColor = Colour::fromFloatRGBA(0.045f, 0.06f, 0.08f, 1.0f);
    outlineColor = Colour::fromFloatRGBA(0.25f, 0.25f, 0.25f, 1.0f);
    regularTextColor = Colour(0xa0eeeeee);
    dimTextColor = Colour(0xa0aaaaaa);

    mAddButton = std::make_unique<TextButton>("+");
    mAddButton->setTitle(TRANS("Add Bus"));
    mAddButton->setLookAndFeel(&addLnf);
    mAddButton->setTooltip(TRANS("Add a new output bus, which several received streams can be combined into"));
    mAddButton->onClick = [this] { addBusPressed(); };
    addAndMakeVisible(mAddButton.get());

    rebuildBusViews();
}

BusesView::~BusesView()
{
    mAddButton->setLookAndFeel(nullptr);
}

void BusesView::configLevelSlider(Slider * slider)
{
    slider->setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxTextColourId, Colour(0x90eeeeee));
    slider->setColour(TooltipWindow::textColourId, Colour(0xf0eeeeee));

    slider->setTextBoxStyle(Slider::TextBoxAbove, true, 100, 12);
    slider->setRange(0.0, 2.0, 0.0);
    slider->setSkewFactor(0.5);
    slider->setDoubleClickReturnValue(true, 1.0);
    slider->setTextBoxIsEditable(true);
    slider->setSliderSnapsToMousePosition(processor.getSlidersSnapToMousePosition());
    slider->setScrollWheelEnabled(false);
    slider->setWantsKeyboardFocus(true);
    slider->valueFromTextFunction = [](const String & s) -> float { return Decibels::decibelsToGain(s.getFloatValue()); };
    slider->textFromValueFunction = [](float v) -> String { return String(TRANS("Level: ")) + Decibels::toString(Decibels::gainToDecibels(v), 1); };
}

void BusesView::setNarrowMode(bool flag, bool update)
{
    if (isNarrow != flag) {
        isNarrow = flag;
        if (update) {
            // no notify: the editor is mid-resize when it flips narrow mode
            updateLayout(false);
            resized();
        }
    }
}

int BusesView::countSourcesForBus(int busIndex) const
{
    int count = 0;
    const int numpeers = processor.getNumberRemotePeers();
    for (int p = 0; p < numpeers; ++p) {
        const int groups = processor.getRemotePeerChannelGroupCount(p);
        for (int g = 0; g < groups; ++g) {
            if (processor.getRemotePeerChannelGroupBus(p, g) == busIndex) {
                ++count;
            }
        }
    }
    return count;
}

void BusesView::rebuildBusViews()
{
    const int numbuses = processor.getNumOutputBuses();

    while (mBusViews.size() < numbuses) {
        auto * bvf = new BusRowView();
        const int index = mBusViews.size();
        bvf->busIndex = index;

        bvf->nameLabel = std::make_unique<Label>("name", "");
        bvf->nameLabel->setJustificationType(Justification::centredLeft);
        bvf->nameLabel->setColour(Label::textColourId, regularTextColor);
        bvf->nameLabel->setEditable(false, true, false);
        bvf->nameLabel->setTooltip(TRANS("Double-click to rename this bus"));
        bvf->nameLabel->setTitle(TRANS("Bus Name"));
        bvf->nameLabel->addListener(this);

        bvf->sourcesLabel = std::make_unique<Label>("sources", "");
        bvf->sourcesLabel->setFont(12);
        bvf->sourcesLabel->setJustificationType(Justification::centredLeft);
        bvf->sourcesLabel->setColour(Label::textColourId, dimTextColor);
        bvf->sourcesLabel->setMinimumHorizontalScale(0.3f);
        bvf->sourcesLabel->setInterceptsMouseClicks(false, false);

        bvf->levelSlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxAbove);
        bvf->levelSlider->setName("buslevel");
        configLevelSlider(bvf->levelSlider.get());
        bvf->levelSlider->setTooltip(TRANS("Master level for everything summed into this bus"));
        bvf->levelSlider->setTitle(TRANS("Bus Level"));
        bvf->levelSlider->addListener(this);

        bvf->destButton = std::make_unique<TextButton>("dest");
        bvf->destButton->setTooltip(TRANS("Device (Dante) output channel this bus is sent to"));
        bvf->destButton->setTitle(TRANS("Bus Output Channel"));
        bvf->destButton->onClick = [this, bvf]() { showBusDestSelectionMenu(bvf->destButton.get(), bvf->busIndex); };

        bvf->removeButton = std::make_unique<SonoDrawableButton>("rm", DrawableButton::ButtonStyle::ImageFitted);
        std::unique_ptr<Drawable> ximg(Drawable::createFromImageData(BinaryData::x_icon_svg, BinaryData::x_icon_svgSize));
        bvf->removeButton->setImages(ximg.get());
        bvf->removeButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
        bvf->removeButton->setTooltip(TRANS("Remove this bus. Anything feeding it goes straight out again."));
        bvf->removeButton->setTitle(TRANS("Remove Bus"));
        bvf->removeButton->onClick = [this, bvf]() { removeBusPressed(bvf->busIndex); };

        bvf->addAndMakeVisible(bvf->nameLabel.get());
        bvf->addAndMakeVisible(bvf->sourcesLabel.get());
        bvf->addAndMakeVisible(bvf->levelSlider.get());
        bvf->addAndMakeVisible(bvf->destButton.get());
        bvf->addAndMakeVisible(bvf->removeButton.get());

        mBusViews.add(bvf);
        addAndMakeVisible(bvf);
    }

    while (mBusViews.size() > numbuses) {
        mBusViews.removeLast();
    }

    // indices shift when a bus in the middle is removed
    for (int i = 0; i < mBusViews.size(); ++i) {
        mBusViews.getUnchecked(i)->busIndex = i;
    }

    mLastBusCount = numbuses;

    updateBusViews();
    updateLayout(false);
}

void BusesView::refreshIfBusesChanged()
{
    if (processor.getNumOutputBuses() != mLastBusCount) {
        rebuildBusViews();
        resized();
        listeners.call(&BusesView::Listener::busLayoutChanged, this);
    }
    else {
        updateBusViews();
    }
}

void BusesView::updateBusViews()
{
    for (int i = 0; i < mBusViews.size(); ++i) {
        auto * bvf = mBusViews.getUnchecked(i);

        OutputBus bus;
        if (!processor.getOutputBus(i, bus)) continue;

        if (!bvf->nameLabel->isBeingEdited()) {
            bvf->nameLabel->setText(bus.name, dontSendNotification);
        }

        const int sources = countSourcesForBus(i);
        String srctext;
        if (sources == 0) {
            srctext << TRANS("no streams");
        } else if (sources == 1) {
            srctext << "1 " << TRANS("stream");
        } else {
            srctext << sources << " " << TRANS("streams");
        }
        bvf->sourcesLabel->setText(srctext, dontSendNotification);
        bvf->sourcesLabel->setAlpha(sources > 0 ? 1.0f : 0.6f);

        if (!bvf->levelSlider->isMouseOverOrDragging()) {
            bvf->levelSlider->setValue(bus.gain, dontSendNotification);
        }

        String desttext;
        const int dstcnt = jmax(1, bus.destChannels);
        if (dstcnt == 1) {
            desttext << bus.destStartIndex + 1;
        } else {
            desttext << bus.destStartIndex + 1 << "-" << bus.destStartIndex + dstcnt;
        }
        bvf->destButton->setButtonText(desttext);

        bvf->nameLabel->setTitle(TRANS("Bus Name") + ": " + bus.name);
        bvf->levelSlider->setTitle(bus.name + " " + TRANS("Level"));
        bvf->destButton->setTitle(bus.name + " " + TRANS("Output Channel"));
        bvf->removeButton->setTitle(TRANS("Remove") + " " + bus.name);
    }
}

void BusesView::addBusPressed()
{
    // Land it on the first output channel no other bus has claimed, so a new bus
    // is immediately usable without having to pick a destination first.
    const int numbuses = processor.getNumOutputBuses();
    const int totalouts = jmax(1, processor.getTotalNumOutputChannels());

    Array<int> used;
    for (int i = 0; i < numbuses; ++i) {
        OutputBus b;
        if (processor.getOutputBus(i, b)) used.add(b.destStartIndex);
    }

    int dest = 0;
    for (int ch = 0; ch < totalouts; ++ch) {
        if (!used.contains(ch)) { dest = ch; break; }
    }

    OutputBus nb(TRANS("Bus") + " " + String(numbuses + 1), dest, 1);
    if (processor.addOutputBus(nb) < 0) {
        return; // MAX_OUTPUT_BUSES reached
    }

    rebuildBusViews();
    resized();
    listeners.call(&BusesView::Listener::busLayoutChanged, this);
}

void BusesView::removeBusPressed(int index)
{
    if (!isPositiveAndBelow(index, processor.getNumOutputBuses())) return;

    const String name = processor.getOutputBusName(index);
    const int sources = countSourcesForBus(index);

    String message;
    message << TRANS("Remove") << " \"" << name << "\"?";
    if (sources > 0) {
        message << "\n\n";
        if (sources == 1) {
            message << TRANS("The stream feeding it will go straight out to device channels again.");
        } else {
            message << TRANS("The streams feeding it will go straight out to device channels again.");
        }
    }

    SafePointer<BusesView> safeThis(this);

    AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon,
                                 TRANS("Remove Bus"),
                                 message,
                                 TRANS("Remove"),
                                 TRANS("Cancel"),
                                 nullptr,
                                 ModalCallbackFunction::create([safeThis, index](int result) {
        if (result == 0 || !safeThis) return;

        safeThis->processor.removeOutputBus(index);
        safeThis->rebuildBusViews();
        safeThis->resized();
        safeThis->listeners.call(&BusesView::Listener::busLayoutChanged, safeThis.getComponent());
    }));
}

void BusesView::showBusDestSelectionMenu(Component * source, int index)
{
    OutputBus bus;
    if (!processor.getOutputBus(index, bus)) return;

    const int totalouts = processor.getTotalNumOutputChannels();

    StringArray outputnames;
    if (JUCEApplicationBase::isStandaloneApp() && getAudioDeviceManager && getAudioDeviceManager()) {
        if (auto cad = getAudioDeviceManager()->getCurrentAudioDevice()) {
            auto actives = cad->getActiveOutputChannels();
            auto allnames = cad->getOutputChannelNames();
            for (int ni = 0; ni < allnames.size(); ++ni) {
                if (actives[ni]) {
                    outputnames.add(allnames[ni]);
                }
            }
        }
    }

    Array<GenericItemChooserItem> items;
    items.add(GenericItemChooserItem(TRANS("SEND BUS TO OUTPUT:"), {}, nullptr, false, true));

    int selindex = -1;
    int ind = 1;

    for (int i = 0; i < totalouts; ++i) {
        String name;
        name << "[" << i + 1 << "] ";
        if (i < outputnames.size()) {
            name << outputnames[i];
        }
        items.add(GenericItemChooserItem(name, Image(), std::make_shared<BusDestItemData>(i, 1), i == 0));

        if (i == bus.destStartIndex && jmax(1, bus.destChannels) == 1) {
            selindex = ind;
        }
        ++ind;
    }

    SafePointer<BusesView> safeThis(this);
    SafePointer<Component> safeSource(source);

    auto callback = [safeThis, safeSource, index](GenericItemChooser * chooser, int chosen) mutable {
        if (!safeThis) return;

        auto & citems = chooser->getItems();
        auto & selitem = citems.getReference(chosen);
        auto bditem = std::dynamic_pointer_cast<BusDestItemData>(selitem.userdata);
        if (!bditem) {
            DBG("Error getting user data");
            return;
        }

        OutputBus bus;
        if (safeThis->processor.getOutputBus(index, bus)) {
            bus.destStartIndex = bditem->startIndex;
            bus.destChannels = bditem->count;
            safeThis->processor.setOutputBus(index, bus);
        }

        safeThis->updateBusViews();
        safeThis->updateLayout();
        safeThis->resized();

        Timer::callAfterDelay(100, [safeSource]() {
            if (safeSource) {
                safeSource->grabKeyboardFocus();
            }
        });
    };

    Component * dw = source->findParentComponentOfClass<AudioProcessorEditor>();
    if (!dw) dw = source->findParentComponentOfClass<Component>();

    juce::Rectangle<int> bounds = dw->getLocalArea(nullptr, source->getScreenBounds());

    GenericItemChooser::launchPopupChooser(items, bounds, dw, callback, selindex, dw ? dw->getHeight() - 30 : 0);
}

void BusesView::sliderValueChanged(Slider * slider)
{
    for (int i = 0; i < mBusViews.size(); ++i) {
        auto * bvf = mBusViews.getUnchecked(i);
        if (bvf->levelSlider.get() == slider) {
            OutputBus bus;
            if (processor.getOutputBus(i, bus)) {
                bus.gain = (float) slider->getValue();
                processor.setOutputBus(i, bus);
            }
            break;
        }
    }
}

void BusesView::labelTextChanged(Label * label)
{
    for (int i = 0; i < mBusViews.size(); ++i) {
        auto * bvf = mBusViews.getUnchecked(i);
        if (bvf->nameLabel.get() == label) {
            OutputBus bus;
            if (processor.getOutputBus(i, bus)) {
                auto newname = label->getText().trim();
                if (newname.isEmpty()) {
                    // never leave a bus nameless -- the receive rows show this name
                    newname = TRANS("Bus") + " " + String(i + 1);
                    label->setText(newname, dontSendNotification);
                }
                bus.name = newname;
                processor.setOutputBus(i, bus);
            }
            break;
        }
    }

    listeners.call(&BusesView::Listener::busLayoutChanged, this);
}

void BusesView::updateLayout(bool notify)
{
    const int minitemheight = 32;
    const int minSliderWidth = isNarrow ? 90 : 120;
    const int namewidth = isNarrow ? 88 : 120;
    const int sourceswidth = isNarrow ? 60 : 78;
    const int destbuttwidth = isNarrow ? 36 : 44;
    const int removebuttwidth = 22;
    const int addrowheight = minitemheight - 6;

    busesBox.items.clear();
    busesBox.flexDirection = FlexBox::Direction::column;
    busesBox.justifyContent = FlexBox::JustifyContent::flexStart;

    int totalheight = 0;
    int rowwidth = 0;

    for (int i = 0; i < mBusViews.size(); ++i) {
        auto * bvf = mBusViews.getUnchecked(i);

        bvf->mainbox.items.clear();
        bvf->mainbox.flexDirection = FlexBox::Direction::row;

        bvf->mainbox.items.add(FlexItem(4, minitemheight));
        bvf->mainbox.items.add(FlexItem(namewidth, minitemheight, *bvf->nameLabel).withMargin(0).withFlex(0));
        bvf->mainbox.items.add(FlexItem(3, minitemheight));
        bvf->mainbox.items.add(FlexItem(sourceswidth, minitemheight, *bvf->sourcesLabel).withMargin(0).withFlex(0));
        bvf->mainbox.items.add(FlexItem(3, minitemheight));
        bvf->mainbox.items.add(FlexItem(minSliderWidth, minitemheight, *bvf->levelSlider).withMargin(0).withFlex(1));
        bvf->mainbox.items.add(FlexItem(4, minitemheight));
        bvf->mainbox.items.add(FlexItem(destbuttwidth, minitemheight, *bvf->destButton).withMargin(0).withFlex(0));
        bvf->mainbox.items.add(FlexItem(3, minitemheight));
        bvf->mainbox.items.add(FlexItem(removebuttwidth, minitemheight, *bvf->removeButton).withMargin(0).withFlex(0));
        bvf->mainbox.items.add(FlexItem(3, minitemheight));

        rowwidth = 0;
        for (auto & item : bvf->mainbox.items) {
            rowwidth += item.minWidth;
        }

        busesBox.items.add(FlexItem(rowwidth, minitemheight + 2, *bvf).withMargin(1).withFlex(0));
        totalheight += minitemheight + 4;
    }

    busesBox.items.add(FlexItem(60, addrowheight, *mAddButton).withMargin(2).withFlex(0).withMaxWidth(90));
    totalheight += addrowheight + 4;

    mMinHeight = totalheight;
    mMinWidth = jmax(rowwidth, 200) + 8;

    if (notify) {
        listeners.call(&BusesView::Listener::busLayoutChanged, this);
    }
}

juce::Rectangle<int> BusesView::getMinimumContentBounds() const
{
    return juce::Rectangle<int>(0, 0, mMinWidth, mMinHeight);
}

void BusesView::paint(Graphics & g)
{
    g.fillAll(bgColor);
}

void BusesView::resized()
{
    busesBox.performLayout(getLocalBounds().reduced(2, 1));
}
