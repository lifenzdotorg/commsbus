// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell



#include "CommsbusAudioProcessor.h"
#include "CommsbusAudioProcessorEditor.h"

#include "BeatToggleGrid.h"

#include "PeersContainerView.h"
#include "RandomSentenceGenerator.h"
#include "SonoUtility.h"
#include "CommsbusTypes.h"
#include "ChannelGroupsView.h"
#include "MonitorDelayView.h"
#include "ChatView.h"
#include "AutoUpdater.h"
#include "LatencyMatchView.h"
#include "SuggestNewGroupView.h"
#include "SonoCallOutBox.h"
#include <sstream>

#if JUCE_ANDROID
#include "juce_core/native/juce_BasicNativeHeaders.h"
#include "juce_core/juce_core.h"
#include "juce_core/native/juce_JNIHelpers_android.h"
#endif


enum {
    PeriodicUpdateTimerId = 0,
    CheckForNewVersionTimerId
};


enum {
    nameTextColourId = 0x1002830,
    selectedColourId = 0x1002840,
    separatorColourId = 0x1002850,
};

enum {
    PeerLayoutRadioGroupId = 1
};

#define COMMSBUS_SCHEME "commsbus"

using namespace SonoAudio;


class CommsbusAudioProcessorEditor::PatchMatrixView : public Component, public BeatToggleGridDelegate
{
public:
    PatchMatrixView(CommsbusAudioProcessor& p) : Component(), processor(p) {
        
        grid = std::make_unique<BeatToggleGrid>();
        grid->setDelegate(this);
        
        addAndMakeVisible(grid.get());
        
        updateGrid();
    }
    virtual ~PatchMatrixView() {
    }
    
    bool beatToggleGridPressed(BeatToggleGrid* grid, int index, const MouseEvent & event) override { 
        int count = processor.getNumberRemotePeers();
        if (count == 0) return false;
        
        int src = index/count;
        int dest = index % count;        
        bool currval = processor.getPatchMatrixValue(src, dest);
        valonpress = currval;
        
        processor.setPatchMatrixValue(src, dest, !currval);

        updateGrid();

        return true;
    }
    
    bool beatToggleGridMoved(BeatToggleGrid* grid, int fromIndex, int toIndex, const MouseEvent & event) override {
        int count = processor.getNumberRemotePeers();
        if (count == 0) return false;
        
        int tosrc = toIndex/count;
        int todest = toIndex % count;        
        bool currval = processor.getPatchMatrixValue(tosrc, todest);
        
        processor.setPatchMatrixValue(tosrc, todest, !valonpress);
        
        updateGrid();
        return true;
    }
    
    bool beatToggleGridReleased(BeatToggleGrid* grid, int index, const MouseEvent & event) override {
        return true;
    }
    
    void updateGrid()
    {
        int count = processor.getNumberRemotePeers();
        if ( count*count != grid->getItems()) {
            updateGridLayout();
        }
        
        for (int i=0; i < count; ++i) {
            for (int j=0; j < count; ++j) {
                int item = i*count + j;
                
                grid->setSelected(processor.getPatchMatrixValue(i, j), item);
                if (i == j) {
                    grid->setAccented(processor.getPatchMatrixValue(i, j), item);                    
                }
            }            
        }
        
        grid->refreshGrid(false);
        repaint();
    }
    
    Rectangle<int> getPreferredBounds() const
    {
        int count = processor.getNumberRemotePeers();
        int labwidth = 30;
        int labheight= 18;
        int buttwidth = 60;
        int buttheight = 44;

        Rectangle<int> prefsize;
        prefsize.setX(0);
        prefsize.setY(0);
        prefsize.setWidth(count * buttwidth + labwidth);
        prefsize.setHeight(count * buttheight + labheight);

        return prefsize;
    }

    
    void updateGridLayout() {

        int count = processor.getNumberRemotePeers();
        
        
        topbox.items.clear();
        topbox.flexDirection = FlexBox::Direction::row;    
        leftbox.items.clear();
        leftbox.flexDirection = FlexBox::Direction::column;    

        while (leftLabels.size() < count) {
            Label * leftlab = new Label("lab", String::formatted("%d", leftLabels.size() + 1));
            leftlab->setJustificationType(Justification::centred);
            leftLabels.add(leftlab);

            Label * toplab = new Label("lab", String::formatted("%d", topLabels.size() + 1));
            toplab->setJustificationType(Justification::centred);
            topLabels.add(toplab);

            addAndMakeVisible( leftLabels.getUnchecked(leftLabels.size()-1));
            addAndMakeVisible( topLabels.getUnchecked(topLabels.size()-1));
        }
        
        for (int i=0; i < leftLabels.size(); ++i) {
            leftLabels.getUnchecked(i)->setVisible(i < count);
            topLabels.getUnchecked(i)->setVisible(i < count);
        }
        
        int labwidth = 30;
        int labheight= 18;
        int buttheight = 36;
        
        topbox.items.add(FlexItem(labwidth, labheight));

        grid->setItems(count * count);
        grid->setSegments(count);
        
        for (int i=0; i < count; ++i) {
            grid->setSegmentSize(count, i);
        }
        
        grid->refreshGrid(true);
        
        for (int i=0; i < count; ++i) {

            for (int j=0; j < count; ++j) {
                int item = i*count + j;
                grid->setLabel(String::formatted("%d>%d", i+1, j+1), item);                
            }
            
            topbox.items.add(FlexItem(20, labheight, *topLabels.getUnchecked(i)).withMargin(2).withFlex(1));
            leftbox.items.add(FlexItem(20, labheight, *leftLabels.getUnchecked(i)).withMargin(2).withFlex(1));            
        }
        

        middlebox.items.clear();
        middlebox.flexDirection = FlexBox::Direction::row;    
        middlebox.items.add(FlexItem(labwidth, labheight, leftbox).withMargin(2).withFlex(0));
        middlebox.items.add(FlexItem(labwidth, buttheight, *grid).withMargin(2).withFlex(1));
        

        
        mainbox.items.clear();
        mainbox.flexDirection = FlexBox::Direction::column;    
        mainbox.items.add(FlexItem(labwidth*2, labheight, topbox).withMargin(2).withFlex(0).withMaxHeight(30));
        mainbox.items.add(FlexItem(labwidth*2, buttheight, middlebox).withMargin(2).withFlex(1));
        
        resized();
    }
    
    void paint (Graphics& g) override
    {

    }

    void resized() override
    {
        Component::resized();
        
        mainbox.performLayout(getLocalBounds());    
    }
    
    std::unique_ptr<BeatToggleGrid> grid;
    OwnedArray<Label> leftLabels;
    OwnedArray<Label> topLabels;

    FlexBox mainbox;
    FlexBox middlebox;
    FlexBox leftbox;
    FlexBox topbox;

    
    bool valonpress = false;
    
    CommsbusAudioProcessor & processor;
};



static void configLabel(Label *label, bool val)
{
    if (val) {
        label->setFont(12);
        label->setColour(Label::textColourId, Colour(0x90eeeeee));
        label->setJustificationType(Justification::centred);        
    }
    else {
        label->setFont(14);
        //label->setColour(Label::textColourId, Colour(0xaaeeeeee));
        label->setJustificationType(Justification::centredLeft);
    }
}

static void configServerLabel(Label *label)
{
    label->setFont(14);
    label->setColour(Label::textColourId, Colour(0x90eeeeee));
    label->setJustificationType(Justification::centredRight);        
}

void CommsbusAudioProcessorEditor::configEditor(TextEditor *editor, bool passwd)
{
    editor->addListener(this);
    if (passwd)  {
        editor->setIndents(8, 6);        
    } else {
        editor->setIndents(8, 8);
    }
}


//==============================================================================
CommsbusAudioProcessorEditor::CommsbusAudioProcessorEditor (CommsbusAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),  sonoLookAndFeel(p.getUseUniversalFont()), sonoSliderLNF(13), smallLNF(14), teensyLNF(11), panSliderLNF(12)
{
    if (p.getUseUniversalFont()) {
#if JUCE_ANDROID
        SonoLookAndFeel::setFontScale(1.0f);
#elif JUCE_WINDOWS
        SonoLookAndFeel::setFontScale(1.35f);
#else
        SonoLookAndFeel::setFontScale(1.25f);
#endif
    }
    else {
        SonoLookAndFeel::setFontScale(1.0f);
    }

    LookAndFeel::setDefaultLookAndFeel(&sonoLookAndFeel);
    
    sonoLookAndFeel.setUsingNativeAlertWindows(true);

    mSettingsFolder = processor.getSupportDir();

    setupLocalisation(processor.getLanguageOverrideCode());

    sonoLookAndFeel.setLanguageCode(mActiveLanguageCode, processor.getUseUniversalFont());
    sonoSliderLNF.setLanguageCode(mActiveLanguageCode, processor.getUseUniversalFont());
    smallLNF.setLanguageCode(mActiveLanguageCode, processor.getUseUniversalFont());
    teensyLNF.setLanguageCode(mActiveLanguageCode, processor.getUseUniversalFont());
    panSliderLNF.setLanguageCode(mActiveLanguageCode, processor.getUseUniversalFont());

    setColour (nameTextColourId, Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.9f));
    setColour (selectedColourId, Colour::fromFloatRGBA(0.0f, 0.4f, 0.8f, 0.5f));
    setColour (separatorColourId, Colour::fromFloatRGBA(0.3f, 0.3f, 0.3f, 0.3f));

    mWorkPool = std::make_unique<ThreadPool>(1);
    
    sonoSliderLNF.textJustification = Justification::centredRight;
    sonoSliderLNF.sliderTextJustification = Justification::centredRight;
    
    
    Array<AooServerConnectionInfo> recents;
    processor.getRecentServerConnectionInfos(recents);
    if (recents.size() > 0) {
        currConnectionInfo = recents.getReference(0);
    }
    else {
        // defaults
        currConnectionInfo.groupName = "";
        String username = processor.getCurrentUsername();

        if (username.isEmpty()) {
#if JUCE_IOS
            //String username = SystemStats::getFullUserName(); //SystemStats::getLogonName();
            username = SystemStats::getComputerName(); //SystemStats::getLogonName();
#else
            username = SystemStats::getFullUserName(); //SystemStats::getLogonName();
            //if (username.length() > 0) username = username.replaceSection(0, 1, username.substring(0, 1).toUpperCase());
#endif
        }
        if (username.isEmpty()) { // fallback
            username = SystemStats::getComputerName();
        }
        
        currConnectionInfo.userName = username.trim();

        currConnectionInfo.serverHost = DEFAULT_SERVER_HOST;
        currConnectionInfo.serverPort = DEFAULT_SERVER_PORT;
    }

    String lastusername = processor.getCurrentUsername().trim();
    if (lastusername.isNotEmpty()) {
        currConnectionInfo.userName = lastusername;
    }

    mTitleLabel = std::make_unique<Label>("title", TRANS("Commsbus"));
    mTitleLabel->setFont(20);
    mTitleLabel->setAccessible(false);
    mTitleLabel->setColour(Label::textColourId, Colour(0xff47b0f8));
    mTitleLabel->setInterceptsMouseClicks(true, false);
    mTitleLabel->addMouseListener(this, false);

    mTitleImage = std::make_unique<ImageComponent>("title");
    mTitleImage->setImage(ImageCache::getFromMemory(BinaryData::commsbus_logo_96_png, BinaryData::commsbus_logo_96_pngSize));
    mTitleImage->setInterceptsMouseClicks(true, false);
    mTitleImage->addMouseListener(this, false);

    
    mMainGroupLabel = std::make_unique<Label>("group", "");
    mMainGroupLabel->setJustificationType(Justification::centredLeft);
    mMainGroupLabel->setFont(16);
    mMainGroupLabel->setEnabled(false);
    mMainGroupLabel->setTitle(TRANS("Group Name"));


    mMainUserLabel = std::make_unique<Label>("user", "");
    mMainUserLabel->setJustificationType(Justification::centredLeft);
    mMainUserLabel->setTitle(TRANS("Your Name"));
    mMainUserLabel->setFont(14);
    mMainUserLabel->setEnabled(false);

    mMainPeerLabel = std::make_unique<Label>("peers", "");
    mMainPeerLabel->setTitle(TRANS("User Count"));
    mMainPeerLabel->setJustificationType(Justification::centred);
    mMainPeerLabel->setFont(18);
    mMainPeerLabel->setEnabled(false);

    mMainMessageLabel = std::make_unique<Label>("mesg", "");
    mMainMessageLabel->setJustificationType(Justification::centred);
    mMainMessageLabel->setFont(20);

    mMainPersonImage = std::make_unique<ImageComponent>();
    mMainPersonImage->setImage(ImageCache::getFromMemory(BinaryData::person_png, BinaryData::person_pngSize));
    mMainGroupImage = std::make_unique<ImageComponent>();
    mMainGroupImage->setImage(ImageCache::getFromMemory(BinaryData::people_png, BinaryData::people_pngSize));
    mMainPersonImage->setInterceptsMouseClicks(false, false);
    mMainGroupImage->setInterceptsMouseClicks(false, false);
    mMainPersonImage->setAccessible(false);
    mMainGroupImage->setAccessible(false);


    mPeerLayoutFullButton = std::make_unique<SonoDrawableButton>("peerfull", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> fullimg(Drawable::createFromImageData(BinaryData::dispfull_svg, BinaryData::dispfull_svgSize));
    mPeerLayoutFullButton->setImages(fullimg.get());
    mPeerLayoutFullButton->addListener(this);
    mPeerLayoutFullButton->setClickingTogglesState(false);
    mPeerLayoutFullButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mPeerLayoutFullButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mPeerLayoutFullButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mPeerLayoutFullButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mPeerLayoutFullButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mPeerLayoutFullButton->setTitle(TRANS("Detailed View"));
    mPeerLayoutFullButton->setTooltip(TRANS("Shows full information for connected users"));
    mPeerLayoutFullButton->setConnectedEdges(Button::ConnectedOnLeft);
    mPeerLayoutFullButton->setRadioGroupId(PeerLayoutRadioGroupId);

    mPeerLayoutMinimalButton = std::make_unique<SonoDrawableButton>("peermin", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> minimg(Drawable::createFromImageData(BinaryData::dispminimal_svg, BinaryData::dispminimal_svgSize));
    mPeerLayoutMinimalButton->setImages(minimg.get());
    mPeerLayoutMinimalButton->addListener(this);
    mPeerLayoutMinimalButton->setClickingTogglesState(false);
    mPeerLayoutMinimalButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mPeerLayoutMinimalButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mPeerLayoutMinimalButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mPeerLayoutMinimalButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mPeerLayoutMinimalButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mPeerLayoutMinimalButton->setTooltip(TRANS("Shows minimal information for connected users"));
    mPeerLayoutMinimalButton->setTitle(TRANS("Minimal View"));
    mPeerLayoutMinimalButton->setConnectedEdges(Button::ConnectedOnRight);
    mPeerLayoutMinimalButton->setRadioGroupId(PeerLayoutRadioGroupId);


    mInGainSlider     = std::make_unique<Slider>(Slider::LinearHorizontal,  Slider::TextBoxAbove);
    mInGainSlider->setName("ingain");
    mInGainSlider->setTitle(TRANS("In Level"));
    mInGainSlider->setSliderSnapsToMousePosition(processor.getSlidersSnapToMousePosition());
    mInGainSlider->setTextBoxIsEditable(true);
    mInGainSlider->setScrollWheelEnabled(false);


    mInMixerButton = std::make_unique<TextButton>("mix");
    mInMixerButton->setButtonText(TRANS("INPUT MIXER"));
    mInMixerButton->setLookAndFeel(&smallLNF);
    mInMixerButton->addListener(this);
    mInMixerButton->setClickingTogglesState(true);
    mInMixerButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.1, 0.3, 0.4, 1.0));
    mInMixerButton->setTooltip(TRANS("This button shows/hides the Input Mixer where you can organize your audio input levels, panning, and effects before your signal is sent out. You can also control how you monitor yourself."));

    mInMuteButton = std::make_unique<TextButton>("mute");
    mInMuteButton->setButtonText(TRANS("MUTE"));
    mInMuteButton->setLookAndFeel(&smallLNF);
    mInMuteButton->addListener(this);
    mInMuteButton->setClickingTogglesState(true);
    mInMuteButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.6, 0.3, 0.1, 1.0));
    mInMuteButton->setTooltip(TRANS("Mutes your input preventing everyone from hearing you, without any indicator"));
    mInMonMuteAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainInMute, *mInMuteButton);

    mInSoloButton = std::make_unique<TextButton>("solo");
    mInSoloButton->setButtonText(TRANS("SOLO"));
    mInSoloButton->setLookAndFeel(&smallLNF);
    mInSoloButton->addListener(this);
    mInSoloButton->setClickingTogglesState(true);
    mInSoloButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(1.0, 1.0, 0.6, 0.7f));
    mInSoloButton->setColour(TextButton::textColourOnId, Colours::darkblue);

    mInSoloButton->setTooltip(TRANS("Listen to only yourself, and other soloed users. Alt-click to exclusively solo yourself."));
    mInMonSoloAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainMonitorSolo, *mInSoloButton);


    mMonDelayButton = std::make_unique<TextButton>("mondel");
    mMonDelayButton->setButtonText(TRANS("MON DELAY"));
    mMonDelayButton->setLookAndFeel(&smallLNF);
    mMonDelayButton->addListener(this);
    mMonDelayButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.6, 1.0, 0.6, 0.7f));
    mMonDelayButton->setColour(TextButton::textColourOnId, Colours::darkblue);
    mMonDelayButton->setTooltip(TRANS("Control additional self-monitoring delay, which can help mitigate synchronization with others"));

    
    mMainMuteButton = std::make_unique<SonoDrawableButton>("sendmute", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> sendallowimg(Drawable::createFromImageData(BinaryData::mic_svg, BinaryData::mic_svgSize));
    std::unique_ptr<Drawable> senddisallowimg(Drawable::createFromImageData(BinaryData::mic_disabled_svg, BinaryData::mic_disabled_svgSize));
    mMainMuteButton->setTitle(TRANS("Send Mute"));
    mMainMuteButton->setImages(sendallowimg.get(), nullptr, nullptr, nullptr, senddisallowimg.get());
    mMainMuteButton->addListener(this);
    mMainMuteButton->setClickingTogglesState(true);
    mMainMuteButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainMuteButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mMainMuteButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mMainMuteButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainMuteButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mMainMuteButton->setTooltip(TRANS("Silences your Input, none of your audio (including file playback) will be sent to users and they will see a muted indicator"));

    mMainRecvMuteButton = std::make_unique<SonoDrawableButton>("recvmute", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> recvallowimg(Drawable::createFromImageData(BinaryData::hearothers_svg, BinaryData::hearothers_svgSize));
    std::unique_ptr<Drawable> recvdisallowimg(Drawable::createFromImageData(BinaryData::muteothers_svg, BinaryData::muteothers_svgSize));
    mMainRecvMuteButton->setImages(recvallowimg.get(), nullptr, nullptr, nullptr, recvdisallowimg.get());
    mMainRecvMuteButton->addListener(this);
    mMainRecvMuteButton->setClickingTogglesState(true);
    mMainRecvMuteButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainRecvMuteButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mMainRecvMuteButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mMainRecvMuteButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainRecvMuteButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mMainRecvMuteButton->setTooltip(TRANS("Mutes/Unmutes all users, no audio data will be received when users are muted"));
    mMainRecvMuteButton->setTitle(TRANS("Mute All Others"));

    mMainPushToTalkButton = std::make_unique<SonoDrawableButton>("ptt", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> pttimg(Drawable::createFromImageData(BinaryData::mic_pointing_svg, BinaryData::mic_pointing_svgSize));
    std::unique_ptr<Drawable> pttonimg(Drawable::createFromImageData(BinaryData::speaker_disabled_svg, BinaryData::speaker_disabled_svgSize));
    mMainPushToTalkButton->setImages(pttimg.get()/*, nullptr, pttonimg.get()*/);
    mMainPushToTalkButton->addMouseListener(this, false);
    mMainPushToTalkButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainPushToTalkButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mMainPushToTalkButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mMainPushToTalkButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    mMainPushToTalkButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mMainPushToTalkButton->setTitle(TRANS("Push To Talk"));
    String pttmessage = TRANS("When pressed, mutes others and unmutes you momentarily (push to talk).");
#if !(JUCE_IOS || JUCE_ANDROID)
    pttmessage += TRANS(" Use the 'T' key as a shortcut.");
#endif
    mMainPushToTalkButton->setTooltip(pttmessage);
    
    mEffectsContainer = std::make_unique<Component>();
    mDrySlider     = std::make_unique<Slider>(Slider::LinearHorizontal,  Slider::TextBoxAbove);
    mDrySlider->setName("dry");
    mDrySlider->setTitle(TRANS("Monitor"));
    mDrySlider->setSliderSnapsToMousePosition(processor.getSlidersSnapToMousePosition());
    mDrySlider->setScrollWheelEnabled(false);

    mOutGainSlider     = std::make_unique<Slider>(Slider::LinearHorizontal,  Slider::TextBoxRight);
    mOutGainSlider->setName("wet");
    mOutGainSlider->setTitle(TRANS("Out Level"));
    mOutGainSlider->setSliderSnapsToMousePosition(processor.getSlidersSnapToMousePosition());
    mOutGainSlider->setScrollWheelEnabled(false);

    configLevelSlider(mInGainSlider.get());
    configLevelSlider(mDrySlider.get());
    configLevelSlider(mOutGainSlider.get());

    mOutGainSlider->setTextBoxIsEditable(true);
    mDrySlider->setTextBoxIsEditable(true);
    //mInGainSlider->setTextBoxIsEditable(true);

    mDrySlider->setWantsKeyboardFocus(true);
    mOutGainSlider->setWantsKeyboardFocus(true);



    mInGainLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramDry, TRANS("In Level"));
    configLabel(mInGainLabel.get(), false);
    mInGainLabel->setJustificationType(Justification::topLeft);
    mInGainLabel->setTooltip(TRANS("This reduces or boosts the level of your own audio input, and it will affect the level of your audio being sent to others and your own monitoring"));
    mInGainLabel->setInterceptsMouseClicks(true, false);
    mInGainLabel->setAccessible(false);

    mDryLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramDry, TRANS("Monitor"));
    configLabel(mDryLabel.get(), false);
    mDryLabel->setJustificationType(Justification::topLeft);
    mDryLabel->setTooltip(TRANS("This adjusts the level of the monitoring of your input, that only you hear"));
    mDryLabel->setInterceptsMouseClicks(true, false);
    mDryLabel->setAccessible(false);

    mOutGainLabel = std::make_unique<Label>("outgain", TRANS("Out Level"));
    configLabel(mOutGainLabel.get(), false);
    mOutGainLabel->setJustificationType(Justification::topLeft);
    mOutGainLabel->setTooltip(TRANS("This is the main volume control which affects everything you hear"));
    mOutGainLabel->setInterceptsMouseClicks(true, false);
    mOutGainLabel->setAccessible(false);

    
    int largeEditorFontsize = 16;
    int smallerEditorFontsize = 14;

#if JUCE_IOS || JUCE_ANDROID
    largeEditorFontsize = 18;
    smallerEditorFontsize = 16;
#endif
    
    mInGainAttachment     = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramInGain, *mInGainSlider);
    mDryAttachment     = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramDry, *mDrySlider);
    mWetAttachment     = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramWet, *mOutGainSlider);
    mMainSendMuteAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainSendMute, *mMainMuteButton);
    mMainRecvMuteAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainRecvMute, *mMainRecvMuteButton);


    
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramMainSendMute, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramMainRecvMute, this);
    //processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramHearLatencyTest, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramMainReverbModel, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramMainReverbEnabled, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramSendChannels, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramInMonitorMonoPan, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramInMonitorPan1, this);
    processor.getValueTreeState().addParameterListener (CommsbusAudioProcessor::paramInMonitorPan2, this);


    mConnectButton = std::make_unique<SonoTextButton>("directconnect");
    mConnectButton->setButtonText(TRANS("Connect..."));
    mConnectButton->addListener(this);
    mConnectButton->setColour(TextButton::buttonColourId, Colour::fromFloatRGBA(0.1, 0.4, 0.6, 0.6));
    mConnectButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.6, 0.4, 0.6, 0.6));
    mConnectButton->setColour(SonoTextButton::outlineColourId, Colour::fromFloatRGBA(0.5, 0.5, 0.5, 0.4));
    mConnectButton->setTextJustification(Justification::centred);


    mAltConnectButton = std::make_unique<SonoDrawableButton>("altconn", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> dotsimg(Drawable::createFromImageData(BinaryData::network_svg, BinaryData::network_svgSize));
    mAltConnectButton->setImages(dotsimg.get(), nullptr, nullptr, nullptr, nullptr);
    mAltConnectButton->addListener(this);
    //mAltConnectButton->setColour(TextButton::buttonOnColourId, Colours::transparentBlack);
    //mAltConnectButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mAltConnectButton->setTooltip(TRANS("Show the connections page, while staying connected to current group"));
    mAltConnectButton->setTitle(TRANS("Connect to Other"));


    mVideoButton = std::make_unique<SonoDrawableButton>("vid", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> vidimg(Drawable::createFromImageData(BinaryData::videocamoutline_svg, BinaryData::videocamoutline_svgSize));
    mVideoButton->setImages(vidimg.get(), nullptr, nullptr, nullptr, nullptr);
    mVideoButton->addListener(this);
    mVideoButton->setTooltip(TRANS("Show the VDO.Ninja video chat link options"));
    mVideoButton->setTitle(TRANS("VDO.Ninja Link"));
    mVideoButton->onClick = [this] {
        showVDONinjaView(true);
    };

    mMainStatusLabel = std::make_unique<Label>("servstat", "");
    mMainStatusLabel->setJustificationType(Justification::centredRight);
    mMainStatusLabel->setFont(13);
    mMainStatusLabel->setColour(Label::textColourId, Colour(0x66ffffff));

    mConnectionTimeLabel = std::make_unique<Label>("conntime", "");
    mConnectionTimeLabel->setJustificationType(Justification::centredBottom);
    mConnectionTimeLabel->setFont(13);
    mConnectionTimeLabel->setColour(Label::textColourId, Colour(0x66ffffff));
    mConnectionTimeLabel->setEnabled(false);

    




    mPatchbayButton = std::make_unique<TextButton>("patch");
    mPatchbayButton->setButtonText(TRANS("Patchbay"));
    mPatchbayButton->addListener(this);

    mSetupAudioButton = std::make_unique<SonoTextButton>("directconnect");
    mSetupAudioButton->setButtonText(TRANS("Setup Audio"));
    mSetupAudioButton->addListener(this);
    mSetupAudioButton->setTextJustification(Justification::centred);

    
    auto flags = foleys::LevelMeter::Minimal; //|foleys::LevelMeter::MaxNumber;
    
    inputMeter = std::make_unique<foleys::LevelMeter>(flags);
    inputMeter->setLookAndFeel(&inMeterLnf);
    inputMeter->setRefreshRateHz(8);
    inputMeter->setMeterSource (&processor.getSendMeterSource());
    inputMeter->addMouseListener(this, false);
    
    outputMeter = std::make_unique<foleys::LevelMeter>(flags);
    outputMeter->setLookAndFeel(&outMeterLnf);
    outputMeter->setRefreshRateHz(8);
    outputMeter->setMeterSource (&processor.getOutputMeterSource());            
    outputMeter->addMouseListener(this, false);

    
    mSettingsButton = std::make_unique<SonoDrawableButton>("settings",  DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> setimg(Drawable::createFromImageData(BinaryData::settings_icon_svg, BinaryData::settings_icon_svgSize));
    mSettingsButton->setImages(setimg.get());
    mSettingsButton->setTitle(TRANS("Settings"));
    mSettingsButton->addListener(this);
    mSettingsButton->setAlpha(0.7);
    mSettingsButton->addMouseListener(this, false);

    mMainLinkButton = std::make_unique<SonoDrawableButton>("link",  DrawableButton::ButtonStyle::ImageOnButtonBackground);
    mMainLinkButton->setTitle(TRANS("Group Action Menu"));
    mMainLinkButton->addListener(this);
    mMainLinkButton->setTooltip(TRANS("Press for group action menu"));

    mMainLinkArrow = Drawable::createFromImageData(BinaryData::triangle_disclosure_svg, BinaryData::triangle_disclosure_svgSize);
    mMainLinkArrow->setInterceptsMouseClicks(false, false);
    mMainLinkArrow->setAlpha(0.7f);


    mPeerContainer = std::make_unique<PeersContainerView>(processor);
    mPeerContainer->addListener(this);
    mPeerContainer->setFocusContainerType(FocusContainerType::focusContainer);


    mTopLevelContainer = std::make_unique<Component>();


    mMainContainer = std::make_unique<Component>();

    mMainViewport = std::make_unique<Viewport>();
    mMainViewport->setViewedComponent(mMainContainer.get(), false);

    mMainContainer->addAndMakeVisible(mPeerContainer.get());

    mInputChannelsContainer = std::make_unique<ChannelGroupsView>(processor, false);
    mMainContainer->addChildComponent(mInputChannelsContainer.get());
    mInputChannelsContainer->addListener(this);

    //mInputChannelsViewport = std::make_unique<Viewport>();
    //mInputChannelsViewport->setViewedComponent(mInputChannelsContainer.get(), false);
    //mMainContainer->addChildComponent(mInputChannelsViewport.get());

    mChatView = std::make_unique<ChatView>(processor, currConnectionInfo);
    mChatView->setVisible(false);
    mChatView->addComponentListener(this);

    mChatButton = std::make_unique<SonoDrawableButton>("chat", DrawableButton::ButtonStyle::ImageOnButtonBackground);
    std::unique_ptr<Drawable> chatdotsimg(Drawable::createFromImageData(BinaryData::chat_dots_svg, BinaryData::chat_dots_svgSize));
    std::unique_ptr<Drawable> chatimg(Drawable::createFromImageData(BinaryData::chat_svg, BinaryData::chat_svgSize));
    mChatButton->setImages(chatimg.get(), nullptr, nullptr, nullptr, chatdotsimg.get());
    mChatButton->onClick = [this]() {
        bool newshown = !mChatView->isVisible();
        
        this->showChatPanel(newshown);
        resized();
    };
    //mChatButton->setClickingTogglesState(true);
    //mChatButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    //mChatButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    //mChatButton->setColour(SonoTextButton::outlineColourId, Colours::transparentBlack);
    mChatButton->setColour(DrawableButton::backgroundOnColourId, Colour::fromFloatRGBA(0.2, 0.2, 0.2, 0.7));
    //mChatButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mChatButton->setTooltip(TRANS("Show/Hide Chat"));



    mChatSizeConstrainer = std::make_unique<ComponentBoundsConstrainer>();
    mChatSizeConstrainer->setSizeLimits(180, 100, 1200, 10000);
    mChatEdgeResizer = std::make_unique<ResizableEdgeComponent>(mChatView.get(), mChatSizeConstrainer.get(), ResizableEdgeComponent::leftEdge);



    mConnectView = std::make_unique<ConnectView>(processor, currConnectionInfo);
    mConnectView->setWantsKeyboardFocus(true);
    mConnectView->updateServerFieldsFromConnectionInfo();
    mConnectView->addComponentListener(this);

    // effects
    
    mEffectsButton = std::make_unique<TextButton>("mainfx");
    mEffectsButton->setButtonText(TRANS("FX"));
    mEffectsButton->setTitle(TRANS("Main Effects"));
    mEffectsButton->setLookAndFeel(&smallLNF);
    mEffectsButton->addListener(this);
    mEffectsButton->setColour(TextButton::buttonOnColourId, Colour::fromFloatRGBA(0.2, 0.5, 0.7, 0.5));

    mBufferMinButton = std::make_unique<SonoDrawableButton>("", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> backimg(Drawable::createFromImageData(BinaryData::reset_buffer_icon_svg, BinaryData::reset_buffer_icon_svgSize));
    mBufferMinButton->setImages(backimg.get());
    mBufferMinButton->addListener(this);
    mBufferMinButton->setTooltip(TRANS("Resets jitter buffer to the minimum for all."));
    mBufferMinButton->setTitle(TRANS("Reset All Jitter Buffers"));
    mBufferMinButton->setAlpha(0.8f);

    mSendChannelsChoice = std::make_unique<SonoChoiceButton>();
    mSendChannelsChoice->setTitle(TRANS("Send Channels"));
    mSendChannelsChoice->addChoiceListener(this);
    mSendChannelsChoice->addItem(TRANS("Send Mono"), 1);
    mSendChannelsChoice->addItem(TRANS("Send Stereo"), 2);
    mSendChannelsChoice->addItem(TRANS("Send Multichannel"), 0);
    mSendChannelsChoice->setLookAndFeel(&smallLNF);

    mSendChannelsChoice->setTooltip(TRANS("This controls how many channels of audio you send out to other connected users. Mono and Stereo send 1 or 2 channels respectively mixed from all of your inputs that you set up in the Input Mixer. If you choose Send Multichannel, it will send all of the inputs you specify in the Input Mixer separately to everyone connected, where they can mix all the channels themselves."));


    mSendChannelsLabel = std::make_unique<Label>("sendch", TRANS("# Send Channels"));
    configLabel(mSendChannelsLabel.get(), true);
    mSendChannelsLabel->setJustificationType(Justification::centred);


    
    mReverbHeaderBg = std::make_unique<DrawableRectangle>();
    mReverbHeaderBg->setCornerSize(Point<float>(6,6));
    mReverbHeaderBg->setFill (Colour::fromFloatRGBA(0.07, 0.07, 0.07, 1.0));
    mReverbHeaderBg->setStrokeFill (Colour::fromFloatRGBA(0.5, 0.5, 0.5, 0.25));
    mReverbHeaderBg->setStrokeThickness(0.5);

    
    mReverbLevelLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramMainReverbLevel, TRANS("Level"));
    configLabel(mReverbLevelLabel.get(), false);
    mReverbLevelLabel->setJustificationType(Justification::centred);
    mReverbLevelLabel->setAccessible(false);

    mReverbSizeLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramMainReverbSize, TRANS("Size"));
    configLabel(mReverbSizeLabel.get(), false);
    mReverbSizeLabel->setJustificationType(Justification::centred);
    mReverbSizeLabel->setAccessible(false);

    mReverbDampingLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramMainReverbDamping, TRANS("Damping"));
    configLabel(mReverbDampingLabel.get(), false);
    mReverbDampingLabel->setJustificationType(Justification::centred);
    mReverbDampingLabel->setAccessible(false);

    mReverbPreDelayLabel = std::make_unique<Label>(CommsbusAudioProcessor::paramMainReverbDamping, TRANS("Pre-Delay"));
    configLabel(mReverbPreDelayLabel.get(), false);
    mReverbPreDelayLabel->setJustificationType(Justification::centred);
    mReverbPreDelayLabel->setAccessible(false);


    mReverbTitleLabel = std::make_unique<Label>("revtitle", TRANS("Reverb"));
    mReverbTitleLabel->setJustificationType(Justification::centredLeft);
    mReverbTitleLabel->setInterceptsMouseClicks(true, false);
    mReverbTitleLabel->addMouseListener(this, false);
    mReverbTitleLabel->setAccessible(false);

    mReverbEnabledButton = std::make_unique<SonoDrawableButton>("reven", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> powerimg(Drawable::createFromImageData(BinaryData::power_svg, BinaryData::power_svgSize));
    std::unique_ptr<Drawable> powerselimg(Drawable::createFromImageData(BinaryData::power_sel_svg, BinaryData::power_sel_svgSize));
    mReverbEnabledButton->setImages(powerimg.get(), nullptr, nullptr, nullptr, powerselimg.get());
    mReverbEnabledButton->setClickingTogglesState(true);
    mReverbEnabledButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
    mReverbEnabledButton->setColour(TextButton::buttonOnColourId, Colours::transparentBlack);
    mReverbEnabledButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mReverbEnabledButton->setColour(DrawableButton::backgroundOnColourId, Colours::transparentBlack);    
    mReverbEnabledButton->addListener(this);
    mReverbEnabledButton->setTitle(TRANS("Reverb Enabled"));
    mReverbEnableAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainReverbEnabled, *mReverbEnabledButton);


    mReverbModelChoice = std::make_unique<SonoChoiceButton>();
    mReverbModelChoice->setTitle(TRANS("Reverb Style"));
    mReverbModelChoice->setColour(SonoTextButton::outlineColourId, Colour::fromFloatRGBA(0.6, 0.6, 0.6, 0.4));
    mReverbModelChoice->addChoiceListener(this);
    mReverbModelChoice->addItem(TRANS("Freeverb"), CommsbusAudioProcessor::ReverbModelFreeverb);
    mReverbModelChoice->addItem(TRANS("MVerb"), CommsbusAudioProcessor::ReverbModelMVerb);
    mReverbModelChoice->addItem(TRANS("Zita"), CommsbusAudioProcessor::ReverbModelZita);

    
    mReverbSizeSlider     = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag,  Slider::NoTextBox);
    mReverbSizeSlider->setName("revsize");
    mReverbSizeSlider->setTitle(TRANS("Size"));
    mReverbSizeSlider->setSliderSnapsToMousePosition(false);
    mReverbSizeSlider->setScrollWheelEnabled(false);
    configKnobSlider(mReverbSizeSlider.get());
    mReverbSizeSlider->setTextBoxStyle(Slider::TextBoxAbove, true, 80, 18);
    mReverbSizeSlider->setTextBoxIsEditable(true);
    mReverbSizeSlider->setWantsKeyboardFocus(true);

    mReverbSizeAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainReverbSize, *mReverbSizeSlider);

    mReverbLevelSlider     = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag,  Slider::NoTextBox);
    mReverbLevelSlider->setName("revlevel");
    mReverbLevelSlider->setTitle(TRANS("Level"));
    mReverbLevelSlider->setSliderSnapsToMousePosition(false);
    mReverbLevelSlider->setScrollWheelEnabled(false);
    configKnobSlider(mReverbLevelSlider.get());
    mReverbLevelSlider->setTextBoxStyle(Slider::TextBoxAbove, true, 80, 18);
    mReverbLevelSlider->setTextBoxIsEditable(true);
    mReverbLevelSlider->setWantsKeyboardFocus(true);

    mReverbLevelAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainReverbLevel, *mReverbLevelSlider);

    mReverbDampingSlider     = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag,  Slider::NoTextBox);
    mReverbDampingSlider->setName("revdamp");
    mReverbDampingSlider->setTitle(TRANS("Damping"));
    mReverbDampingSlider->setSliderSnapsToMousePosition(false);
    mReverbDampingSlider->setScrollWheelEnabled(false);
    configKnobSlider(mReverbDampingSlider.get());
    mReverbDampingSlider->setTextBoxStyle(Slider::TextBoxAbove, true, 80, 18);
    mReverbDampingSlider->setTextBoxIsEditable(true);
    mReverbDampingSlider->setWantsKeyboardFocus(true);

    mReverbDampingAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainReverbDamping, *mReverbDampingSlider);

    mReverbPreDelaySlider     = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag,  Slider::NoTextBox);
    mReverbPreDelaySlider->setName("revpredel");
    mReverbPreDelaySlider->setTitle(TRANS("Pre-Delay"));
    mReverbPreDelaySlider->setSliderSnapsToMousePosition(false);
    mReverbPreDelaySlider->setScrollWheelEnabled(false);
    configKnobSlider(mReverbPreDelaySlider.get());
    mReverbPreDelaySlider->setTextBoxStyle(Slider::TextBoxAbove, true, 80, 18);
    mReverbPreDelaySlider->setTextBoxIsEditable(true);
    mReverbPreDelaySlider->setWantsKeyboardFocus(true);

    mReverbPreDelayAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.getValueTreeState(), CommsbusAudioProcessor::paramMainReverbPreDelay, *mReverbPreDelaySlider);

    
    mIAAHostButton = std::make_unique<SonoDrawableButton>("iaa", DrawableButton::ButtonStyle::ImageFitted);
    mIAAHostButton->addListener(this);

    
    mPeerRecImage = Drawable::createFromImageData(BinaryData::rectape_svg, BinaryData::rectape_svgSize);
    mPeerRecImage->setInterceptsMouseClicks(false, false);

    {
        mRecordingButton = std::make_unique<SonoDrawableButton>("record", DrawableButton::ButtonStyle::ImageFitted);
        std::unique_ptr<Drawable> recimg(Drawable::createFromImageData(BinaryData::record_svg, BinaryData::record_svgSize));
        std::unique_ptr<Drawable> recselimg(Drawable::createFromImageData(BinaryData::record_active_alt_svg, BinaryData::record_active_alt_svgSize));
        mRecordingButton->setImages(recimg.get(), nullptr, nullptr, nullptr, recselimg.get());
        mRecordingButton->addListener(this);
        mRecordingButton->setColour(DrawableButton::backgroundOnColourId, Colours::transparentBlack);
        mRecordingButton->setTooltip(TRANS("Start/Stop recording audio to file"));
        mRecordingButton->setTitle(TRANS("Record"));
        mRecordingButton->setClickingTogglesState(true);
    }

#if JUCE_IOS || JUCE_ANDROID
    mMainViewport->setScrollOnDragEnabled(true);
    //mInputChannelsViewport->setScrollOnDragEnabled(true);
#endif
    


    
    mTopLevelContainer->addAndMakeVisible(mMainViewport.get());
    mTopLevelContainer->addAndMakeVisible(mTitleLabel.get());
    //addAndMakeVisible(mTitleImage.get());

    mTopLevelContainer->addAndMakeVisible(mMainLinkButton.get());
    mTopLevelContainer->addChildComponent(mMainLinkArrow.get());
    mTopLevelContainer->addAndMakeVisible(mMainGroupLabel.get());
    mTopLevelContainer->addAndMakeVisible(mMainPeerLabel.get());
    mTopLevelContainer->addAndMakeVisible(mMainMessageLabel.get());
    mTopLevelContainer->addAndMakeVisible(mMainUserLabel.get());
    mTopLevelContainer->addAndMakeVisible(mMainPersonImage.get());
    mTopLevelContainer->addAndMakeVisible(mMainGroupImage.get());
    mTopLevelContainer->addChildComponent(mPeerRecImage.get());
    mTopLevelContainer->addAndMakeVisible(mPeerLayoutFullButton.get());
    mTopLevelContainer->addAndMakeVisible(mPeerLayoutMinimalButton.get());


    mTopLevelContainer->addAndMakeVisible(mDrySlider.get());
    mTopLevelContainer->addAndMakeVisible(mOutGainSlider.get());
    //mTopLevelContainer->addAndMakeVisible(mInGainSlider.get());
    mTopLevelContainer->addAndMakeVisible(mMainMuteButton.get());
    mTopLevelContainer->addAndMakeVisible(mMainRecvMuteButton.get());
    mTopLevelContainer->addAndMakeVisible(mMainPushToTalkButton.get());
    mTopLevelContainer->addAndMakeVisible(mInMuteButton.get());
    mTopLevelContainer->addAndMakeVisible(mInSoloButton.get());
    //addAndMakeVisible(mMonDelayButton.get());

    mTopLevelContainer->addAndMakeVisible(mPatchbayButton.get());
    mTopLevelContainer->addAndMakeVisible (mSettingsButton.get());
    mTopLevelContainer->addAndMakeVisible(mConnectButton.get());
    mTopLevelContainer->addChildComponent(mAltConnectButton.get());
    mTopLevelContainer->addChildComponent(mVideoButton.get());
    mTopLevelContainer->addAndMakeVisible(mMainStatusLabel.get());
    mTopLevelContainer->addAndMakeVisible(mConnectionTimeLabel.get());

    mTopLevelContainer->addChildComponent(mSetupAudioButton.get());

    
    mTopLevelContainer->addAndMakeVisible(mEffectsButton.get());
    mTopLevelContainer->addAndMakeVisible(mBufferMinButton.get());


    // (plugin-only "sync metronome to host tempo" control removed with the plugin builds)

    mEffectsContainer->addAndMakeVisible(mReverbHeaderBg.get());
    mEffectsContainer->addAndMakeVisible(mReverbTitleLabel.get());
    mEffectsContainer->addAndMakeVisible(mReverbEnabledButton.get());
    mEffectsContainer->addAndMakeVisible(mReverbModelChoice.get());
    mEffectsContainer->addAndMakeVisible(mReverbLevelSlider.get());
    mEffectsContainer->addAndMakeVisible(mReverbSizeSlider.get());
    mEffectsContainer->addAndMakeVisible(mReverbLevelLabel.get());
    mEffectsContainer->addAndMakeVisible(mReverbSizeLabel.get());
    mEffectsContainer->addAndMakeVisible(mReverbDampingLabel.get());
    mEffectsContainer->addAndMakeVisible(mReverbDampingSlider.get());
    mEffectsContainer->addAndMakeVisible(mReverbPreDelayLabel.get());
    mEffectsContainer->addAndMakeVisible(mReverbPreDelaySlider.get());
    
    

    mTopLevelContainer->addAndMakeVisible(mChatButton.get());

    

    mTopLevelContainer->addChildComponent(mIAAHostButton.get());


    if (mRecordingButton) {
        mTopLevelContainer->addAndMakeVisible(mRecordingButton.get());
    }



    //mTopLevelContainer->addAndMakeVisible(mInGainLabel.get());
    mTopLevelContainer->addAndMakeVisible(mDryLabel.get());
    mTopLevelContainer->addAndMakeVisible(mOutGainLabel.get());
    mTopLevelContainer->addAndMakeVisible(inputMeter.get());
    mTopLevelContainer->addAndMakeVisible(outputMeter.get());
    mTopLevelContainer->addAndMakeVisible (mInMixerButton.get());


    mTopLevelContainer->addAndMakeVisible (mSendChannelsChoice.get());
    //addAndMakeVisible (mSendChannelsLabel.get());


    mTopLevelContainer->addChildComponent(mChatView.get());
    mChatView->addAndMakeVisible(mChatEdgeResizer.get());



    addAndMakeVisible(mTopLevelContainer.get());

    addChildComponent(mConnectView.get());

    // over everything


    //mSettingsButton->setExplicitFocusOrder(1);
    //mConnectButton->setExplicitFocusOrder(2);

    //mChatView->setFocusContainerType(FocusContainerType::keyboardFocusContainer);

    //setFocusContainerType(FocusContainerType::keyboardFocusContainer);

    //mPublicServerConnectViewport->setViewedComponent(mPublicServerConnectContainer.get());


    inChannels = processor.getTotalNumInputChannels(); // processor.getMainBusNumInputChannels();
    outChannels = processor.getMainBusNumOutputChannels();
    
    updateLayout();
    
    updateState();

    //updateServerFieldsFromConnectionInfo();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    auto defbounds = processor.getLastPluginBounds();

    setSize (defbounds.getWidth(), defbounds.getHeight());


    setResizeLimits(320, 340, 10000, 10000);

    commandManager.registerAllCommandsForTarget (this);

    if (JUCEApplicationBase::isStandaloneApp()) {
#if !(JUCE_IOS || JUCE_ANDROID)
        processor.startAooServer();
#endif
        setResizable(true, false);


        commandManager.registerAllCommandsForTarget (JUCEApplication::getInstance());

        menuBarModel = std::make_unique<CommsbusMenuBarModel>(*this);

        menuBarModel->setApplicationCommandManagerToWatch(&commandManager);

        
#if JUCE_MAC
        auto extraAppleMenuItems = PopupMenu();
        extraAppleMenuItems.addCommandItem(&commandManager, CommsbusCommands::ShowOptions);
        
        MenuBarModel::setMacMainMenu(menuBarModel.get(), &extraAppleMenuItems);
#endif

#if (JUCE_WINDOWS || JUCE_LINUX)
        mMenuBar = std::make_unique<MenuBarComponent>(menuBarModel.get());
        //mMenuBar->setFocusContainerType(FocusContainerType::keyboardFocusContainer);
        mMenuBar->setWantsKeyboardFocus(true);
        addAndMakeVisible(mMenuBar.get());
#endif

        // look for link in clipboard        
        if (mConnectView->attemptToPasteConnectionFromClipboard()) {
            // show connect immediately
            showConnectPopup(true);
            // switch to group page
            mConnectView->showPrivateGroupTab();

            updateServerStatusLabel(TRANS("Filled in Group information from clipboard! Press 'Connect to Group' to join..."), false);
        }
        
        
        // check for update!
        // LatestVersionCheckerAndUpdater::getInstance()->checkForNewVersion (false);
        
    } else  {
        setResizable(true, true);    
    }
    
    processor.addClientListener(this);

    // handles registering commands
    updateUseKeybindings();
        
    commandManager.commandStatusChanged();
    
    setWantsKeyboardFocus(true);
    
    startTimer(PeriodicUpdateTimerId, 1000);

#if (JUCE_WINDOWS || JUCE_MAC)
    if (JUCEApplicationBase::isStandaloneApp()) {
        startTimer(CheckForNewVersionTimerId, 5000);
    }
#endif

   // Make sure that before the constructor has finished, you've set the
   // editor's size to whatever you need it to be.

    //auto defbounds = processor.getLastPluginBounds();

    //setSize (defbounds.getWidth(), defbounds.getHeight());


    // to make sure transport area is initialized with the current state
    

}

CommsbusAudioProcessorEditor::~CommsbusAudioProcessorEditor()
{
    if (menuBarModel) {
        menuBarModel->setApplicationCommandManagerToWatch(nullptr);
#if JUCE_MAC
        MenuBarModel::setMacMainMenu(nullptr);
#endif
    }
    
    if (tooltipWindow) {
        tooltipWindow->parent = nullptr;
    }
    
    popTip.reset();
    
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramMainSendMute, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramMainRecvMute, this);
    //processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramHearLatencyTest, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramMainReverbModel, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramMainReverbEnabled, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramSendChannels, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramInMonitorMonoPan, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramInMonitorPan1, this);
    processor.getValueTreeState().removeParameterListener (CommsbusAudioProcessor::paramInMonitorPan2, this);


    
    processor.removeClientListener(this);
    
    

}

static void doActuallyQuit (int result, CommsbusAudioProcessorEditor* editor)
{
    if (result != 0) {
        JUCEApplicationBase::quit();
    }
}

bool CommsbusAudioProcessorEditor::requestedQuit()
{
    // allow quit if we are not connected
    if (currConnected && currGroup.isNotEmpty()) {
        // pop up confirmation
        AlertWindow::showOkCancelBox (AlertWindow::WarningIcon,
        TRANS("Quit Confirmation"),
        TRANS("You are connected, are you sure you want to quit?"),
        TRANS ("Quit"),
        String(),
        nullptr,
        ModalCallbackFunction::create (doActuallyQuit, this));
        
        return false;
    }
    return true;
}

void CommsbusAudioProcessorEditor::updateUseKeybindings()
{
    commandManager.clearCommands();
    commandManager.registerAllCommandsForTarget (this);

    if (JUCEApplicationBase::isStandaloneApp()) {

        commandManager.registerAllCommandsForTarget (JUCEApplication::getInstance());
    }

    // this will use the command manager to initialise the KeyPressMappingSet with
    // the default keypresses that were specified when the targets added their commands
    // to the manager.
    commandManager.getKeyMappings()->resetToDefaultMappings();

    // having set up the default key-mappings, you might now want to load the last set
    // of mappings that the user configured.
    //myCommandManager->getKeyMappings()->restoreFromXml (lastSavedKeyMappingsXML);


    // is this even necessary? the registration thing above handles most of it with the menu commands
    if (processor.getDisableKeyboardShortcuts()) {
        removeKeyListener(commandManager.getKeyMappings());
    }
    else {
        // Now tell our top-level window to send any keypresses that arrive to the
        // KeyPressMappingSet, which will use them to invoke the appropriate commands.
        addKeyListener (commandManager.getKeyMappings());
    }

}

void CommsbusAudioProcessorEditor::connectionsChanged(ConnectView *comp)
{
    updateState(false);
}

void CommsbusAudioProcessorEditor::channelLayoutChanged(ChannelGroupsView *comp)
{
    //updateState();

    int sendchval = (int) processor.getSendChannels();
    mSendChannelsChoice->setSelectedId(sendchval, dontSendNotification);
    if (sendchval > 0) {
        inputMeter->setFixedNumChannels(sendchval);
    } else {
        inputMeter->setFixedNumChannels(processor.getActiveSendChannelCount());
    }

    updateLayout();

    resized();
}

void CommsbusAudioProcessorEditor::internalSizesChanged(PeersContainerView *comp)
{
    resized();
}


void CommsbusAudioProcessorEditor::configKnobSlider(Slider * slider)
{
    //slider->setVelocityBasedMode(true);
    //slider->setVelocityModeParameters(2.5, 1, 0.05);
    //slider->setTextBoxStyle(Slider::NoTextBox, true, 40, 18);
    slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle(Slider::TextBoxAbove, true, 60, 14);
    slider->setMouseDragSensitivity(128);
    slider->setScrollWheelEnabled(false);
    //slider->setPopupDisplayEnabled(true, false, this);
    slider->setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxTextColourId, Colour(0x90eeeeee));
    slider->setColour(TooltipWindow::textColourId, Colour(0xf0eeeeee));

    slider->setLookAndFeel(&sonoSliderLNF);
}

void CommsbusAudioProcessorEditor::configLevelSlider(Slider * slider)
{
    //slider->setVelocityBasedMode(true);
    //slider->setVelocityModeParameters(2.5, 1, 0.05);
    //slider->setTextBoxStyle(Slider::NoTextBox, true, 40, 18);
    slider->setSliderStyle(Slider::SliderStyle::LinearHorizontal);
    slider->setTextBoxStyle(Slider::TextBoxAbove, true, 50, 14);
    slider->setMouseDragSensitivity(128);
    slider->setScrollWheelEnabled(false);
    //slider->setPopupDisplayEnabled(true, false, this);
    slider->setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider->setColour(Slider::textBoxTextColourId, Colour(0x90eeeeee));
    slider->setColour(TooltipWindow::textColourId, Colour(0xf0eeeeee));

    slider->setLookAndFeel(&sonoSliderLNF);
}




//////////////////
// these client listener callbacks will be from a different thread

void CommsbusAudioProcessorEditor::aooClientConnected(CommsbusAudioProcessor *comp, bool success, const String & errmesg) 
{
    DBG("Client connect success: " <<  (int) success << "  mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::ConnectEvent, success, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientDisconnected(CommsbusAudioProcessor *comp, bool success, const String & errmesg) 
{
    DBG("Client disconnect success: " << (int) success <<  "  mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::DisconnectEvent, success, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientLoginResult(CommsbusAudioProcessor *comp, bool success, const String & errmesg)  
{
    DBG("Client login success: " << (int)success << "  mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::LoginEvent, success, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientGroupJoined(CommsbusAudioProcessor *comp, bool success, const String & group,  const String & errmesg) 
{
    DBG("Client join group " << group << " success: " << (int)success  << "  mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::GroupJoinEvent, group, success, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientGroupLeft(CommsbusAudioProcessor *comp, bool success, const String & group, const String & errmesg)  
{
    DBG("Client leave group " << group << " success: " << (int)success << "   mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::GroupLeaveEvent, group, success, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientPublicGroupModified(CommsbusAudioProcessor *comp, const String & group, int count, const String & errmesg)
{
    DBG("Public group add/modified " << group << " count: " << (int)count << "   mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add(ClientEvent(ClientEvent::PublicGroupModifiedEvent, group, true, errmesg));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientPublicGroupDeleted(CommsbusAudioProcessor *comp, const String & group,  const String & errmesg)
{
    DBG("Public group delete " << group << "   mesg: " << errmesg);
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add(ClientEvent(ClientEvent::PublicGroupDeletedEvent, group, true, errmesg));
    }
    triggerAsyncUpdate();
}


void CommsbusAudioProcessorEditor::aooClientPeerJoined(CommsbusAudioProcessor *comp, const String & group, const String & user)  
{
    DBG("Client peer '" << user  << "' joined group '" <<  group << "'");
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::PeerJoinEvent, group, true, "", user));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientPeerPendingJoin(CommsbusAudioProcessor *comp, const String & group, const String & user) 
{
    DBG("Client peer '" << user  << "' pending join group '" <<  group << "'");
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::PeerPendingJoinEvent, group, true, "", user));
    }
    triggerAsyncUpdate();    
}

void CommsbusAudioProcessorEditor::aooClientPeerJoinFailed(CommsbusAudioProcessor *comp, const String & group, const String & user)
{
    DBG("Client peer '" << user  << "' FAILed to join group '" <<  group << "'");
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::PeerFailedJoinEvent, group, true, "", user));
    }
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientPeerJoinBlocked(CommsbusAudioProcessor *comp, const String & group, const String & user, const String & address, int port)
{
    DBG("Client peer '" << user  << "' with address: " << address << " : " << port <<  " BLOCKED from joining group '" <<  group << "'");
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add(ClientEvent(ClientEvent::PeerBlockedJoinEvent, group, true, address, user, port));
    }
    triggerAsyncUpdate();
}


void CommsbusAudioProcessorEditor::aooClientPeerLeft(CommsbusAudioProcessor *comp, const String & group, const String & user)  
{
    DBG("Client peer '" << user  << "' left group '" <<  group << "'");
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::PeerLeaveEvent, group, true, "", user));
    }
    triggerAsyncUpdate();

}

void CommsbusAudioProcessorEditor::aooClientError(CommsbusAudioProcessor *comp, const String & errmesg)  
{
    DBG("Client error: " <<  errmesg);
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::Error, errmesg));
    }
    
    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::aooClientPeerChangedState(CommsbusAudioProcessor *comp, const String & mesg)
{
    {
        const ScopedLock sl (clientStateLock);        
        clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, mesg));
    }
    
    triggerAsyncUpdate();    
}

void CommsbusAudioProcessorEditor::sbChatEventReceived(CommsbusAudioProcessor *comp, const SBChatEvent & mesg)
{
    haveNewChatEvents = true;

    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::peerRequestedLatencyMatch(CommsbusAudioProcessor *comp, const String & username, float latency)
{
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add(ClientEvent(ClientEvent::PeerRequestedLatencyMatchEvent, username, latency));
    }

    triggerAsyncUpdate();
}

void CommsbusAudioProcessorEditor::peerSuggestedNewGroup(CommsbusAudioProcessor *comp, const String & username, const String & newgroup, const String & passwd, bool isPublic, const StringArray & others)
{
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add( ClientEvent::makeSuggestedNewGroupEvent(username, newgroup,  passwd, isPublic, others));
    }

    triggerAsyncUpdate();
}


void CommsbusAudioProcessorEditor::peerBlockedInfoChanged(CommsbusAudioProcessor *comp, const String & username, bool blocked)
{
    {
        const ScopedLock sl (clientStateLock);
        clientEvents.add(ClientEvent(ClientEvent::PeerBlockedInfoChangedEvent, username, blocked));
    }

    triggerAsyncUpdate();

}


//////////////////////////


void CommsbusAudioProcessorEditor::choiceButtonSelected(SonoChoiceButton *comp, int index, int ident)
{
    if (comp == mReverbModelChoice.get()) {
        processor.setMainReverbModel((CommsbusAudioProcessor::ReverbModel) ident);
    }
    else if (comp == mSendChannelsChoice.get()) {
        float fval = processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramSendChannels)->convertTo0to1(ident);
        processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramSendChannels)->setValueNotifyingHost(fval);
        updateLayout();
    }
}


bool CommsbusAudioProcessorEditor::updatePeerState(bool force)
{
    if (!mPeerContainer) return false;
    
    if (force || mPeerContainer->getPeerViewCount() != processor.getNumberRemotePeers()) {
        mPeerContainer->rebuildPeerViews();
        updateLayout();
        resized();
        
        if (patchbayCalloutBox) {
            mPatchMatrixView->updateGridLayout();
            mPatchMatrixView->updateGrid();
        }

        updateState(false);
        return true;
    }
    else {
        mPeerContainer->updatePeerViews();

        if (patchbayCalloutBox) {
            mPatchMatrixView->updateGrid();
        }
        return false;
    }        
}


void CommsbusAudioProcessorEditor::updateChannelState(bool force)
{
    //if (force || inChannels != processor.getMainBusNumInputChannels() || outChannels != processor.getMainBusNumOutputChannels()) {
    if (force || inChannels != processor.getTotalNumInputChannels() || outChannels != processor.getMainBusNumOutputChannels()) {
        inChannels = processor.getTotalNumInputChannels(); // processor.getMainBusNumInputChannels();
        outChannels = processor.getMainBusNumOutputChannels();
        updateLayout();
        updateState();
        resized();
    }    
}

void CommsbusAudioProcessorEditor::updateOptionsState(bool ignorecheck)
{
    if (mOptionsView != nullptr)  {
        mOptionsView->updateState();
    }
}


void CommsbusAudioProcessorEditor::timerCallback(int timerid)
{
    if (timerid == PeriodicUpdateTimerId) {
        
        bool stateUpdated = updatePeerState();
        
        updateChannelState();
        
        if (!stateUpdated && (currGroup != processor.getCurrentJoinedGroup()
                              || currConnected != processor.isConnectedToServer()
                              )) {
            updateState();
        }
        
        double nowstamp = Time::getMillisecondCounterHiRes() * 1e-3;
        if (serverStatusFadeTimestamp > 0.0 && nowstamp >= serverStatusFadeTimestamp) {
            Desktop::getInstance().getAnimator().fadeOut(mMainStatusLabel.get(), 500);
            serverStatusFadeTimestamp = 0.0;
        }
        
        

        if (processor.isConnectedToServer() && processor.getCurrentJoinedGroup().isNotEmpty()) {
            mConnectionTimeLabel->setText(SonoUtility::durationToString(processor.getElapsedConnectedTime(), true), dontSendNotification);
            mConnectionTimeLabel->setEnabled(true);
        }

        if (!tooltipWindow && getParentComponent()) {
            Component* dw = this; //this->findParentComponentOfClass<DocumentWindow>();
            if (!dw)
                dw = this->findParentComponentOfClass<AudioProcessorEditor>();
            if (!dw)
                dw = this->findParentComponentOfClass<Component>();
            //if (!dw)
            //    dw = this;

            if (dw) {
                tooltipWindow = std::make_unique<CustomTooltipWindow>(this, dw);
            }

            // first time thing
            mConnectButton->grabKeyboardFocus();
        }

        if (processor.getLastChatShown() != mChatView->isVisible()) {
            showChatPanel(processor.getLastChatShown(), false);
            resized();
        }
        

        mChatButton->setToggleState(mChatView->haveNewSinceLastView(), dontSendNotification);

        auto anyrec = processor.isAnyRemotePeerRecording() || processor.isRecordingToFile();
        if (mPeerRecImage->isVisible() != anyrec) {
            mPeerRecImage->setVisible(anyrec);
            mPeerRecImage->repaint();
            resized();
        }

#if 0
        if (JUCEApplicationBase::isStandaloneApp() && getAudioDeviceManager())
        {
            if (auto * ad = getAudioDeviceManager()->getCurrentAudioDevice()) {
                int inlat = ad->getInputLatencyInSamples();
                int outlat = ad->getOutputLatencyInSamples();
                int bufsize = ad->getCurrentBufferSizeSamples();
                
                DBG("InLat: " << inlat << "  OutLat: " << outlat << "  bufsize: " << bufsize);
            }
        }
#endif
        
#if JUCE_IOS
        if (JUCEApplicationBase::isStandaloneApp()) {
            bool iaaconn = isInterAppAudioConnected();
            if (iaaconn != iaaConnected) {
                iaaConnected = iaaconn;
                DBG("Interapp audio state changed: " << (int) iaaConnected);
                if (iaaConnected) {
                    iaaHostIcon = getIAAHostIcon(78);
                    DrawableImage randimg;
                    randimg.setImage(iaaHostIcon);
                    mIAAHostButton->setImages(&randimg);
                }
                updateLayout();
                updateState();
            }
            
        }
#endif
    }
    else if (timerid == CheckForNewVersionTimerId) {
        if (getShouldCheckForNewVersionValue) {
            Value * val = getShouldCheckForNewVersionValue();
            if (val && (bool)val->getValue()) {
                DBG("Checking for new version");
                LatestVersionCheckerAndUpdater::getInstance()->checkForNewVersion (false);
            }
        }
        stopTimer(CheckForNewVersionTimerId);
    }
}


void CommsbusAudioProcessorEditor::textEditorReturnKeyPressed (TextEditor& ed)
{
    DBG("Return pressed");

}

void CommsbusAudioProcessorEditor::textEditorEscapeKeyPressed (TextEditor& ed)
{
}

void CommsbusAudioProcessorEditor::textEditorTextChanged (TextEditor&)
{
}


void CommsbusAudioProcessorEditor::textEditorFocusLost (TextEditor& ed)
{

}




void CommsbusAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == mConnectButton.get() || buttonThatWasClicked == mAltConnectButton.get()) {
        
        if (processor.isConnectedToServer() && processor.getCurrentJoinedGroup().isNotEmpty() && buttonThatWasClicked != mAltConnectButton.get()) {
            mConnectionTimeLabel->setText(TRANS("Last Session: ") + SonoUtility::durationToString(processor.getElapsedConnectedTime(), true), dontSendNotification);
            mConnectButton->setTextJustification(Justification::centredTop);
            mConnectionTimeLabel->setEnabled(true);

            if (processor.getWatchPublicGroups()) {
                processor.leaveServerGroup(processor.getCurrentJoinedGroup());
            }
            else {
                processor.disconnectFromServer();
            }
            updateState();
        }
        else {

            if (buttonThatWasClicked != mAltConnectButton.get()) {
                mConnectionTimeLabel->setText("", dontSendNotification);
                mConnectionTimeLabel->setEnabled(false);
                mConnectButton->setTextJustification(Justification::centred);
            }

            if (!mConnectView->isVisible()) {
                showConnectPopup(true);
            } else {
                showConnectPopup(false);
            }
        }
        
    }
    else if (buttonThatWasClicked == mSetupAudioButton.get()) {
        if (!settingsCalloutBox) {
            showSettings(true);
            if (mOptionsView) {
                mOptionsView->showAudioTab();
            }
        }
    }
    else if (buttonThatWasClicked == mPatchbayButton.get()) {
        if (!patchbayCalloutBox) {
            showPatchbay(true);
        } else {
            showPatchbay(false);
        }        
    }
    else if (buttonThatWasClicked == mInMixerButton.get()) {

        mInputChannelsContainer->setVisible(mInMixerButton->getToggleState());
        mInputChannelsContainer->rebuildChannelViews();
        resized();

    }
    
    else if (buttonThatWasClicked == mEffectsButton.get()) {
        if (!effectsCalloutBox) {
            showEffectsConfig(true);
        } else {
            showEffectsConfig(false);
        }        
    }
    else if (buttonThatWasClicked == mBufferMinButton.get()) {
        resetJitterBufferForAll();
    }
    else if (buttonThatWasClicked == mMainMuteButton.get()) {
        // allow or disallow sending to all peers, handled by button attachment

        if (mMainMuteButton->getToggleState()) {
            showPopTip(TRANS("Not sending your audio anywhere"), 3000, mMainMuteButton.get());
        } else {
            showPopTip(TRANS("Sending your audio to others"), 3000, mMainMuteButton.get());
        }
    }
    else if (buttonThatWasClicked == mMonDelayButton.get()) {
        if (!monDelayCalloutBox) {
            showMonitorDelayView(true);
        } else {
            showMonitorDelayView(false);
        }
    }
    else if (buttonThatWasClicked == mInSoloButton.get()) {
        if (ModifierKeys::currentModifiers.isAltDown()) {
            // exclusive solo this one
            for (int j=0; j < processor.getNumberRemotePeers(); ++j) {
                processor.setRemotePeerSoloed(j, false);
            }
        }
        updatePeerState();
    }
    else if (buttonThatWasClicked == mMainRecvMuteButton.get()) {
        // allow or disallow sending to all peers, handled by button attachment

        if (processor.getNumberRemotePeers() > 0 && settingsCalloutBox == nullptr) {
            if (mMainRecvMuteButton->getToggleState()) {
                showPopTip(TRANS("Muted everyone"), 3000, mMainRecvMuteButton.get());
            } else {
                showPopTip(TRANS("Unmuted all who were not muted previously"), 3000, mMainRecvMuteButton.get());
            }
        }
    }
    
    
    else if (buttonThatWasClicked == mPeerLayoutMinimalButton.get()) {
        processor.setPeerDisplayMode( CommsbusAudioProcessor::PeerDisplayModeMinimal);
        mPeerContainer->setPeerDisplayMode(CommsbusAudioProcessor::PeerDisplayModeMinimal);
        updateState();
    }
    else if (buttonThatWasClicked == mPeerLayoutFullButton.get()) {
        processor.setPeerDisplayMode( CommsbusAudioProcessor::PeerDisplayModeFull);
        mPeerContainer->setPeerDisplayMode(CommsbusAudioProcessor::PeerDisplayModeFull);
        updateState();
    }

    else if (buttonThatWasClicked == mMainLinkButton.get()) {

        showGroupMenu(true);

    }

    else if (buttonThatWasClicked == mSettingsButton.get()) {
        if (!settingsWasShownOnDown) {
            showSettings(true);
        }
    }
    else if (buttonThatWasClicked == mIAAHostButton.get()) {
        if (iaaConnected) {
            switchToHostApplication();
        }
    }
    else if (buttonThatWasClicked == mRecordingButton.get()) {
        if (processor.isRecordingToFile()) {
            processor.stopRecordingToFile();

            mRecordingButton->setToggleState(false, dontSendNotification);
            //updateServerStatusLabel("Stopped Recording");

            String filepath;
#if (JUCE_IOS || JUCE_ANDROID)
            if (lastRecordedFile.isLocalFile()) {
                filepath = lastRecordedFile.getLocalFile().getRelativePathFrom(File::getSpecialLocation (File::userDocumentsDirectory));
                //showPopTip(TRANS("Finished recording to ") + filepath, 4000, mRecordingButton.get(), 130);
            }
            else {
                filepath = lastRecordedFile.getFileName();
            }
#elif (JUCE_ANDROID)
            filepath = lastRecordedFile.getFileName();
#else
            if (lastRecordedFile.isLocalFile()) {
                filepath = lastRecordedFile.getLocalFile().getRelativePathFrom(File::getSpecialLocation (File::userHomeDirectory));
            }
            else {
                filepath = lastRecordedFile.getFileName();
            }
#endif

            mRecordingButton->setTooltip(TRANS("Last recorded file: ") + filepath);


            //Timer::callAfterDelay(200, []() {
            //    AccessibilityHandler::postAnnouncement(TRANS("Recording finished"), AccessibilityHandler::AnnouncementPriority::high);
            //});


            if (processor.getRecordFinishOpens()) {
                // load up recording
                if (lastRecordedFile.isLocalFile()) {
                }
                updateLayout();
                resized();
            }
            
        } else {

            SafePointer<CommsbusAudioProcessorEditor> safeThis (this);

#if JUCE_ANDROID
            if (getAndroidSDKVersion() < 29) {
                if (! RuntimePermissions::isGranted (RuntimePermissions::writeExternalStorage))
                {
                    RuntimePermissions::request (RuntimePermissions::writeExternalStorage,
                                                 [safeThis] (bool granted) mutable
                                                 {
                        if (granted)
                            safeThis->buttonClicked (safeThis->mRecordingButton.get());
                    });
                    return;
                }
            }
#endif
            
            // create new timestamped filename
            String filename = (currGroup.isEmpty() ? "CommsbusSession" : currGroup) + String("_") + Time::getCurrentTime().formatted("%Y-%m-%d_%H.%M.%S");

            filename = File::createLegalFileName(filename);

            auto parentDirUrl = processor.getDefaultRecordingDirectory();
            
            if (parentDirUrl.isEmpty()) {
                // only happens on android, ask for external location to store files
                
                auto alertcb = [safeThis] (int button) {
                    if (safeThis && button == 0) {
                        safeThis->requestRecordDir([safeThis] (URL recurl) mutable
                                                   {
                            //if (!recurl.isEmpty()) {
                            //    safeThis->buttonClicked (safeThis->mRecordingButton.get());
                            //}
                        });
                    }
                };
                
#if JUCE_ANDROID
                auto mbopts = MessageBoxOptions().withTitle(TRANS("Select Folder")).withMessage(TRANS("You need to first choose a folder on your device to save recordings to.")).withButton(TRANS("Choose Folder")).withButton(TRANS("Cancel")).withAssociatedComponent(this);

                AlertWindow::showAsync(mbopts, alertcb);
#else
                // just do it
                alertcb(0);
#endif
                return;
            }
            
            File parentDir;
            if (parentDirUrl.isLocalFile()) {
                parentDir = parentDirUrl.getLocalFile();
                parentDir.createDirectory();
            }

            filename += ".flac";
            
            //File file (parentDir.getNonexistentChildFile (filename, ".flac"));
            URL returl;
            
            if (processor.startRecordingToFile(parentDirUrl, filename, returl)) {
                //updateServerStatusLabel("Started recording...");
                lastRecordedFile = returl;
                String filepath;

#if (JUCE_IOS || JUCE_ANDROID)
                showPopTip(TRANS("Started recording output"), 2000, mRecordingButton.get());
#else
                //Timer::callAfterDelay(200, []() {
                //    AccessibilityHandler::postAnnouncement(TRANS("Started recording output"), AccessibilityHandler::AnnouncementPriority::high);
                //});
#endif



                if (processor.getDefaultRecordingOptions() == CommsbusAudioProcessor::RecordMix) {

#if (JUCE_IOS)
                    if (lastRecordedFile.isLocalFile()) {
                        filepath = lastRecordedFile.getLocalFile().getRelativePathFrom(File::getSpecialLocation (File::userDocumentsDirectory));
                    } else {
                        filepath = lastRecordedFile.getFileName();
                    }
#elif JUCE_ANDROID
                    filepath = lastRecordedFile.getFileName();
#else
                    if (lastRecordedFile.isLocalFile()) {
                        filepath = lastRecordedFile.getLocalFile().getRelativePathFrom(File::getSpecialLocation (File::userHomeDirectory));
                    } else {
                        filepath = lastRecordedFile.getFileName();
                    }
#endif

                    mRecordingButton->setTooltip(TRANS("Recording audio to: ") + filepath);
                } else 
                {
#if (JUCE_IOS)
                    if (lastRecordedFile.isLocalFile()) {
                        filepath = lastRecordedFile.getLocalFile().getParentDirectory().getRelativePathFrom(File::getSpecialLocation (File::userDocumentsDirectory));
                    } else {
                        filepath = lastRecordedFile.getFileName();
                    }
#elif (JUCE_ANDROID)
                    filepath = lastRecordedFile.getFileName();
#else
                    if (lastRecordedFile.isLocalFile()) {
                        filepath = lastRecordedFile.getLocalFile().getParentDirectory().getRelativePathFrom(File::getSpecialLocation (File::userHomeDirectory));
                    } else {
                        filepath = lastRecordedFile.getFileName();
                    }
#endif
                    mRecordingButton->setTooltip(TRANS("Recording multi-track audio to: ") + filepath);
                }
            }
            else {
                // show error starting record
                String lasterr = processor.getLastErrorMessage();
                showPopTip(lasterr, 0, mRecordingButton.get());
            }
            
            mRecordingButton->setToggleState(true, dontSendNotification);

        }
    }
    
    
    
    
    
    

    
    else {
        
       
    }
}

void CommsbusAudioProcessorEditor::resetJitterBufferForAll()
{
    // do it for everyone who's on auto
    bool initCompleted = false;
    for (int j=0; j < processor.getNumberRemotePeers(); ++j)
    {
        if (processor.getRemotePeerAutoresizeBufferMode(j, initCompleted) != CommsbusAudioProcessor::AutoNetBufferModeOff) {
            float buftime = 0.0;
            processor.setRemotePeerBufferTime(j, buftime);
        }
    }
}


void CommsbusAudioProcessorEditor::requestRecordDir(std::function<void (URL)> callback)
{
    SafePointer<CommsbusAudioProcessorEditor> safeThis (this);

    DBG("Requesting recdir");
    
    File initopendir;
#if JUCE_ANDROID
    initopendir = File::getSpecialLocation(File::SpecialLocationType::userMusicDirectory);
    // doesn't work
#endif
    
    mFileChooser.reset(new FileChooser(TRANS("Choose a location to store recorded files."),
                                       initopendir,
                                       "",
                                       true, false, getTopLevelComponent()));
    
    
    
    mFileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                               [safeThis,callback] (const FileChooser& chooser) mutable
                               {
        auto results = chooser.getURLResults();
        if (safeThis != nullptr && results.size() > 0)
        {
            auto url = results.getReference (0);
            
            DBG("Chosen recdir to save in: " <<  url.toString(false));
            
#if JUCE_ANDROID
            auto docdir = AndroidDocument::fromTree(url);
            if (!docdir.hasValue()) {
                docdir = AndroidDocument::fromFile(url.getLocalFile());
            }
            
            if (docdir.hasValue()) {
                AndroidDocumentPermission::takePersistentReadWriteAccess(url);
                if (docdir.getInfo().isDirectory()) {
                    safeThis->processor.setDefaultRecordingDirectory(url);
                }
            }
#else
            if (url.isLocalFile()) {
                File lfile = url.getLocalFile();
                if (lfile.isDirectory()) {
                    safeThis->processor.setDefaultRecordingDirectory(url);
                } else {
                    auto parurl = URL(lfile.getParentDirectory());
                    safeThis->processor.setDefaultRecordingDirectory(parurl);
                }

            }
#endif
            
            if (url.isLocalFile()) {
            }

            callback(url);
        }
        
        if (safeThis) {
            safeThis->mFileChooser.reset();
        }
                    
    }, nullptr);
}

void CommsbusAudioProcessorEditor::showSaveSettingsPreset()
{
    SafePointer<CommsbusAudioProcessorEditor> safeThis (this);

    File recdir; // = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("Commsbus Setups");
    String * recentsfolder = nullptr;
    if (getLastRecentsFolder) {
        if ((recentsfolder = getLastRecentsFolder()) != nullptr) {
            recdir = File(*recentsfolder);
        }
    }
    // TODO - on iOS we need to give it a name first

    mFileChooser.reset(new FileChooser(TRANS("Choose a location and name to store the setup"),
                                       recdir,
                                       "*.commsbus",
                                       true, false, getTopLevelComponent()));



    mFileChooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::doNotClearFileNameOnRootChange,
                               [safeThis] (const FileChooser& chooser) mutable
                               {
        auto results = chooser.getURLResults();
        if (safeThis != nullptr && results.size() > 0)
        {
            auto url = results.getReference (0);

            DBG("Chose directory: " <<  url.toString(false));

            if (url.isLocalFile()) {
                File lfile = url.getLocalFile();

                // save settings
                safeThis->saveSettingsToFile(lfile);
            }
        }

        if (safeThis) {
            safeThis->mFileChooser.reset();
        }

    }, nullptr);
}

void CommsbusAudioProcessorEditor::showLoadSettingsPreset()
{
    SafePointer<CommsbusAudioProcessorEditor> safeThis (this);

    File recdir; // = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("Commsbus Setups");
    String * recentsfolder = nullptr;
    if (getLastRecentsFolder) {
        if ((recentsfolder = getLastRecentsFolder()) != nullptr) {
            recdir = File(*recentsfolder);
        }
    }

    mFileChooser.reset(new FileChooser(TRANS("Choose a setup file to load"),
                                       recdir,
                                       "*.commsbus",
                                       true, false, getTopLevelComponent()));



    mFileChooser->launchAsync (FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,
                               [safeThis] (const FileChooser& chooser) mutable
                               {
        auto results = chooser.getURLResults();
        if (safeThis != nullptr && results.size() > 0)
        {
            auto url = results.getReference (0);

            DBG("Chose file: " <<  url.toString(false));

            if (url.isLocalFile()) {
                File lfile = url.getLocalFile();

                // load settings
                safeThis->loadSettingsFromFile(lfile);
            }
        }

        if (safeThis) {
            safeThis->mFileChooser.reset();
        }

    }, nullptr);
}

bool CommsbusAudioProcessorEditor::loadSettingsFromFile(const File & file)
{
    if (!getAudioDeviceManager || !getAudioDeviceManager()) return false;
    bool retval = true;

    PropertiesFile::Options opts;
    PropertiesFile propfile = PropertiesFile(file, opts);

    if (!propfile.isValidFile()) {
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                          TRANS("Error while loading"),
                                          TRANS("Couldn't read from the specified file!"));

        return false;
    }

    MemoryBlock data;

    if (propfile.containsKey("filterStateXML")) {
        String filtxml = propfile.getValue ("filterStateXML");
        data.replaceWith(filtxml.toUTF8(), filtxml.getNumBytesAsUTF8());
        if (data.getSize() > 0) {
            processor.setStateInformationWithOptions (data.getData(), (int) data.getSize(), false, true, true);
        }
        else {
            DBG("Empty XML filterstate");
            retval = false;
        }
    }
    else {
        if (data.fromBase64Encoding (propfile.getValue ("filterState")) && data.getSize() > 0) {
            processor.setStateInformationWithOptions (data.getData(), (int) data.getSize(), false, true);
        } else {
            retval = false;
        }
    }

    auto deviceManager = getAudioDeviceManager();

    auto savedAudioState = propfile.getXmlValue ("audioSetup");

    if (savedAudioState.get()) {
        std::unique_ptr<AudioDeviceManager::AudioDeviceSetup> prefSetupOptions;
        String preferredDefaultDeviceName;

        // now remove samplerate from saved state if necessary
        // as well as preferredSetupOptions

        if (!((bool)getShouldOverrideSampleRateValue()->getValue())) {
            DBG("NOT OVERRIDING SAMPLERATE");
            if (savedAudioState && savedAudioState->hasAttribute("audioDeviceRate")) {
                savedAudioState->removeAttribute("audioDeviceRate");
            }
            if (prefSetupOptions) {
                prefSetupOptions->sampleRate = 0;
            }
        }



        auto totalInChannels  = processor.getMainBusNumInputChannels();
        auto totalOutChannels = processor.getMainBusNumOutputChannels();

        deviceManager->initialise (totalInChannels,
                                   totalOutChannels,
                                   savedAudioState.get(),
                                   true,
                                   preferredDefaultDeviceName,
                                   prefSetupOptions.get());
    }

    if (!retval) {
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                          TRANS("Error while loading"),
                                          TRANS("Invalid setup!"));
    }

    updateState();
    updateLayout();
    resized();

    addToRecentsSetups(file);

    return retval;
}

void CommsbusAudioProcessorEditor::addToRecentsSetups(const File & file)
{
    if (getRecentSetupFiles && getRecentSetupFiles()) {
        auto recents = getRecentSetupFiles();
        auto fname = file.getFullPathName();
        if (auto index = recents->indexOf(fname) >= 0) {
            // move it to the end (most recent)
            recents->move(index, recents->size()-1);
        }
        else {
            recents->add(fname);

            // limit it to 12, remove from front (oldest)
            if (recents->size() > 12) {
                recents->remove(0);
            }
        }

        // force regen of file menu
        if (menuBarModel) {
            menuBarModel->menuItemsChanged();
        }
    }

    if (getLastRecentsFolder) {
        if (auto * recentsfolder = getLastRecentsFolder()) {
            *recentsfolder = file.getParentDirectory().getFullPathName();
        }
    }

}


bool CommsbusAudioProcessorEditor::saveSettingsToFile(const File & file)
{
    if (!getAudioDeviceManager || !getAudioDeviceManager()) return false;

    bool usexmlstate = true;

    MemoryBlock data;
    processor.getStateInformationWithOptions(data, false, true, usexmlstate);

    PropertiesFile::Options opts;
    PropertiesFile propfile = PropertiesFile(file, opts);

    std::unique_ptr<XmlElement> xml (getAudioDeviceManager()->createStateXml());
    propfile.setValue ("audioSetup", xml.get());

    if (usexmlstate) {
        std::unique_ptr<XmlElement> filtxml = juce::parseXML(String::createStringFromData(data.getData(), (int)data.getSize()));
        if (filtxml) {
            propfile.setValue ("filterStateXML", filtxml.get());
        }
    } else {
        propfile.setValue ("filterState", data.toBase64Encoding());
    }

    if (!propfile.save()) {

        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                          TRANS("Error while saving"),
                                          TRANS("Couldn't write to the specified file!"));

        return false;
    }

    addToRecentsSetups(file);

    return true;
}



void CommsbusAudioProcessorEditor::updateSliderSnap()
{
    // set level slider snap to mouse property based on processor state and size of slider
    auto snap = processor.getSlidersSnapToMousePosition();

    int minsize = 60;

    std::function<void(Slider *)> snapset = [&](Slider * slider){
        slider->setSliderSnapsToMousePosition(slider->getWidth() > minsize && snap);
    };

    //snapset(mInGainSlider.get());
    snapset(mOutGainSlider.get());
    snapset(mDrySlider.get());
    //snapset(mOptionsDefaultLevelSlider.get());

    mPeerContainer->applyToAllSliders(snapset);
    mInputChannelsContainer->applyToAllSliders(snapset);
}


void CommsbusAudioProcessorEditor::handleURL(const String & urlstr)
{
    URL url(urlstr);
    if (url.isWellFormed()) {
        if (!currConnected || currGroup.isEmpty()) {
            if (mConnectView->handleCommsbusURL(url)) {

                // connect immediately
                connectWithInfo(currConnectionInfo);

                //showConnectPopup(true);
                // switch to group page
                //mConnectTab->setCurrentTabIndex(mConnectTab->getNumTabs() > 2 ? 1 : 0);
                //updateServerStatusLabel(TRANS("Filled in Group from link! Press 'Connect to Group' to join..."), false);
            }
        }
    }
}

// XXX
void CommsbusAudioProcessorEditor::connectWithInfo(const AooServerConnectionInfo & info, bool allowEmptyGroup, bool copyInfoOnly)
{
    currConnectionInfo = info;

    if (!copyInfoOnly) {
        mConnectView->connectWithInfo(currConnectionInfo, allowEmptyGroup);
    }
}





void CommsbusAudioProcessorEditor::showEffectsConfig(bool flag)
{
    
    if (flag && effectsCalloutBox == nullptr) {
        
        auto wrap = std::make_unique<Viewport>();

        
        Component* dw = this; 
        
#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 260; 
        const int defHeight = 154;
#else
        const int defWidth = 260; 
        const int defHeight = 135;
#endif
        
        
        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));
        
        
        mEffectsContainer->setBounds(Rectangle<int>(0,0,defWidth,defHeight));
        
        wrap->setViewedComponent(mEffectsContainer.get(), false);
        mEffectsContainer->setVisible(true);
        
        effectsBox.performLayout(mEffectsContainer->getLocalBounds());

        auto headbgbounds = mReverbEnabledButton->getBounds().withRight(mReverbModelChoice->getRight()).expanded(2);
        mReverbHeaderBg->setRectangle (headbgbounds.toFloat());

        
        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mEffectsButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        effectsCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(effectsCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }

        mReverbEnabledButton->grabKeyboardFocus();
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(effectsCalloutBox.get())) {
            box->dismiss();
            effectsCalloutBox = nullptr;
        }
    }
}


void CommsbusAudioProcessorEditor::showPatchbay(bool flag)
{
    if (!mPatchMatrixView) {
        mPatchMatrixView = std::make_unique<PatchMatrixView>(processor);
    }
    
    if (flag && patchbayCalloutBox == nullptr) {
        
        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;
        
        // calculate based on how many peers we have
        auto prefbounds = mPatchMatrixView->getPreferredBounds();
        const int defWidth = prefbounds.getWidth(); 
        const int defHeight = prefbounds.getHeight();
        
        wrap->setSize(jmin(defWidth + 8, dw->getWidth() - 20), jmin(defHeight + 8, dw->getHeight() - 24));
                
        mPatchMatrixView->setBounds(Rectangle<int>(0,0,defWidth,defHeight));
        
        wrap->setViewedComponent(mPatchMatrixView.get(), false);
        
        mPatchMatrixView->updateGridLayout();
        mPatchMatrixView->updateGrid();
        
        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mPatchbayButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        patchbayCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(patchbayCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(patchbayCalloutBox.get())) {
            box->dismiss();
            patchbayCalloutBox = nullptr;
        }
    }
}

void CommsbusAudioProcessorEditor::showLatencyMatchView(bool show)
{
    if (show && latmatchCalloutBox == nullptr) {

        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 260;
        const int defHeight = 250;
#else
        const int defWidth = 260;
        const int defHeight = 300;
#endif

        if (!mLatMatchView) {
            mLatMatchView = std::make_unique<LatencyMatchView>(processor);
        }


        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));


        mLatMatchView->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setViewedComponent(mLatMatchView.get(), false);
        mLatMatchView->setVisible(true);

        mLatMatchView->startLatMatchProcess();


        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMainLinkButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        latmatchCalloutBox = & SonoCallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false, [this](const Component * comp) {
            if (comp == mMainLinkButton.get()) return false;
            return true;
        });
        if (auto * box = dynamic_cast<SonoCallOutBox*>(latmatchCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (auto * box = dynamic_cast<SonoCallOutBox*>(latmatchCalloutBox.get())) {
            box->dismiss();
            latmatchCalloutBox = nullptr;
        }
    }
}

void CommsbusAudioProcessorEditor::showVDONinjaView(bool show, bool fromVideoButton)
{
    if (show && vdoninjaViewCalloutBox == nullptr) {

        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 320;
        const int defHeight = 350;
#else
        const int defWidth = 500;
        const int defHeight = 315;
#endif

        if (!mVDONinjaView) {
            mVDONinjaView = std::make_unique<VDONinjaView>(processor);
        }


        int prefwidth = jmin(defWidth, dw->getWidth() - 10);
        
        // size it once, then get min height out of it
        mVDONinjaView->setBounds(Rectangle<int>(0,0,prefwidth,defHeight));

        auto useheight = mVDONinjaView->getMinimumContentBounds().getHeight() + mVDONinjaView->getMinimumHeaderBounds().getHeight();
        auto usewidth = std::max(prefwidth, mVDONinjaView->getMinimumContentBounds().getWidth());

        mVDONinjaView->setBounds(Rectangle<int>(0,0, usewidth,useheight));

        wrap->setViewedComponent(mVDONinjaView.get(), false);
        mVDONinjaView->updateState();
        mVDONinjaView->setVisible(true);

        wrap->setSize(usewidth, jmin(useheight, dw->getHeight() - 24));

        Rectangle<int> bounds =  dw->getLocalArea(nullptr, fromVideoButton ? mVideoButton->getScreenBounds() : mMainLinkButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        vdoninjaViewCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(vdoninjaViewCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(vdoninjaViewCalloutBox.get())) {
            box->dismiss();
            vdoninjaViewCalloutBox = nullptr;
        }
    }
}

void CommsbusAudioProcessorEditor::showSuggestGroupView(bool show)
{
    if (show && suggestNewGroupViewCalloutBox == nullptr) {

        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 280;
        const int defHeight = 360;
#else
        const int defWidth = 260;
        const int defHeight = 360;
#endif

        if (!mSuggestNewGroupView) {
            mSuggestNewGroupView = std::make_unique<SuggestNewGroupView>(processor);

            mSuggestNewGroupView->connectToGroup = [this] (const String & group, const String & groupPass, bool isPublic) {
                currConnectionInfo.groupName = group;
                currConnectionInfo.groupPassword = groupPass;
                currConnectionInfo.groupIsPublic = isPublic;
                connectWithInfo(currConnectionInfo);
            };
        }


        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));


        mSuggestNewGroupView->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setViewedComponent(mSuggestNewGroupView.get(), false);
        mSuggestNewGroupView->setVisible(true);

        mSuggestNewGroupView->updatePeerRows(true);

        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMainLinkButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        suggestNewGroupViewCalloutBox = & SonoCallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false, [this](const Component * comp) {
            if (comp == mMainLinkButton.get()) return false;
            return true;
        });
        if (SonoCallOutBox * box = dynamic_cast<SonoCallOutBox*>(suggestNewGroupViewCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(suggestNewGroupViewCalloutBox.get())) {
            box->dismiss();
            suggestNewGroupViewCalloutBox = nullptr;
        }
    }
}


void CommsbusAudioProcessorEditor::showConnectPopup(bool flag)
{
    if (flag) {
        mConnectView->toFront(true);

        mConnectView->updateState();

        mConnectView->setVisible(true);

        mConnectView->grabInitialFocus();
    }
    else {
        mConnectView->setVisible(false);
    }
}


void CommsbusAudioProcessorEditor::sliderValueChanged (Slider* slider)
{
}

void CommsbusAudioProcessorEditor::mouseDown (const MouseEvent& event) 
{
    
    if (event.eventComponent == mSettingsButton.get()) {
        settingsWasShownOnDown = settingsCalloutBox != nullptr || (Time::getMillisecondCounter() < settingsClosedTimestamp + 500);

        if (!settingsWasShownOnDown) {
          //  showOrHideSettings();
        }
    }
    else if (event.eventComponent == mTitleLabel.get() || event.eventComponent == mTitleImage.get()) {
        settingsWasShownOnDown = settingsCalloutBox != nullptr;
        settingsClosedTimestamp = 0; // reset on a down
        if (settingsWasShownOnDown) {
            showSettings(false);
        }
    }
    else if (event.eventComponent == inputMeter.get()) {
        inputMeter->clearClipIndicator(-1);
        outputMeter->clearClipIndicator(-1);
        inputMeter->clearMaxLevelDisplay(-1);
        outputMeter->clearMaxLevelDisplay(-1);
        mPeerContainer->clearClipIndicators();
        if (mInputChannelsContainer) {
            mInputChannelsContainer->clearClipIndicators();
        }
    }
    else if (event.eventComponent == outputMeter.get()) {
        outputMeter->clearClipIndicator(-1);
        inputMeter->clearClipIndicator(-1);
        inputMeter->clearMaxLevelDisplay(-1);
        outputMeter->clearMaxLevelDisplay(-1);
        mPeerContainer->clearClipIndicators();
        if (mInputChannelsContainer) {
            mInputChannelsContainer->clearClipIndicators();
        }
    }
    else if (event.eventComponent == mMainPushToTalkButton.get()) {
        // mute others/recv, send self
        if (mMainPushToTalkButton->isEnabled()) {
            mPushToTalkWasMuted = processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->getValue() > 0;
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainRecvMute)->setValueNotifyingHost(1.0);            
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->setValueNotifyingHost(0.0);
        }
    }
}

void CommsbusAudioProcessorEditor::mouseUp (const MouseEvent& event)
{
    if (event.eventComponent == mTitleLabel.get() || event.eventComponent == mTitleImage.get()) {
        if (Time::getMillisecondCounter() > settingsClosedTimestamp + 1000) {
            // only show if it wasn't just dismissed
            showSettings(true);
        }
    }
    else if (event.eventComponent == mMainPushToTalkButton.get()) {
        if (mMainPushToTalkButton->isEnabled()) {
            // back to mute self, hear others
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->setValueNotifyingHost(mPushToTalkWasMuted ? 1.0 : 0.0);            
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainRecvMute)->setValueNotifyingHost(0.0);
        }
    }
    else if (event.eventComponent == mReverbTitleLabel.get()) {
        mReverbEnabledButton->setToggleState(!mReverbEnabledButton->getToggleState(), sendNotification);
    }
}

void CommsbusAudioProcessorEditor::componentVisibilityChanged (Component& component)
{
    //if (&component == mSettingsTab.get()) {
    //    DebugLogC("setting vis changed: %d", component.isVisible());
    //}

    if (&component == mChatView.get()) {
        if (!mChatView->isVisible() && mChatWasVisible) {
            if (!mChatOverlay && mChatShowDidResize) {
                // reduce size
                int newwidth = getWidth() - mChatView->getWidth();
                setSize(newwidth, getHeight());
                //mChatShowDidResize = false;
            } else {
                resized();
            }
        }

        mChatWasVisible = mChatView->isVisible();
        //mChatButton->setToggleState(mChatWasVisible, dontSendNotification);
        processor.setLastChatShown(mChatWasVisible);

        mAboutToShowChat = false;
    }
    
    else if (&component == mConnectView.get()) {
        mTopLevelContainer->setEnabled(!mConnectView->isVisible());
        if (!mConnectView->isVisible()) {
            // focus on main connect button
            mConnectButton->grabKeyboardFocus();
        }
    }

}

void CommsbusAudioProcessorEditor::componentMovedOrResized (Component& component, bool wasmoved, bool wasresized)
{
    if (&component == mChatView.get()) {
        if (mChatView->isVisible()) {
            processor.setLastChatWidth(mChatView->getWidth());
            if (!mIgnoreResize) {
                resized();
            }
        }
    }
    
}


void CommsbusAudioProcessorEditor::componentParentHierarchyChanged (Component& component)
{
    if (&component == mOptionsView.get()) {
        if (component.getParentComponent() == nullptr) {
            DBG("setting parent changed: " << (uint64) component.getParentComponent());
            settingsClosedTimestamp = Time::getMillisecondCounter();
        }
    }
}

bool CommsbusAudioProcessorEditor::keyPressed (const KeyPress & key)
{
    DBG("Got key: " << key.getTextCharacter() << "  isdown: " << (key.isCurrentlyDown() ? 1 : 0) << " keycode: " << key.getKeyCode() << " pcode: " << (int)'p');

    mAltReleaseShouldAct = false; // reset alt check
    bool gotone = false;

    if (key.isKeyCurrentlyDown('T') && !processor.getDisableKeyboardShortcuts()) {
        if (!mPushToTalkKeyDown) {
            DBG("T press");
            // mute others, send self
            mPushToTalkWasMuted = processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->getValue() > 0;
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainRecvMute)->setValueNotifyingHost(1.0);
            processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->setValueNotifyingHost(0.0);
            mPushToTalkKeyDown = true;
        }
        gotone = true;
    }
    else if (key.isKeyCode(KeyPress::escapeKey)) {
        DBG("ESCAPE pressed");
        if (mConnectView->isVisible()) {
            mConnectView->escapePressed();
            gotone = true;
        }
    }

    return gotone;
}

bool CommsbusAudioProcessorEditor::keyStateChanged (bool isKeyDown)
{
    bool pttdown = KeyPress::isKeyCurrentlyDown('T');
    
    if (!pttdown && mPushToTalkKeyDown) {
        // release
        DBG("T release");
        // mute self again, send to others
        processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->setValueNotifyingHost(mPushToTalkWasMuted ? 1.0 : 0.0);
        processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainRecvMute)->setValueNotifyingHost(0.0);            
        mPushToTalkKeyDown = false;
        return true;
    }

#if 0
    if (!mAltReleaseIsPending && ModifierKeys::currentModifiers.isAltDown()) {
        DBG("Alt down");
        mAltReleaseIsPending = true;
        mAltReleaseShouldAct = true;
    }
    else if (mAltReleaseIsPending && !ModifierKeys::currentModifiers.isAltDown()) {
        DBG("Alt release");
        mAltReleaseIsPending = false;
        // on windows focus menubar
#if (JUCE_WINDOWS || JUCE_LINUX)
        if (mAltReleaseShouldAct && mMenuBar) {
            if (mMenuBar->hasKeyboardFocus(true)) {
                PopupMenu::dismissAllActiveMenus();
            }
            else {
                mMenuBar->grabKeyboardFocus();
                mMenuBar->showMenu(0);
            }
        }
#endif
        mAltReleaseShouldAct = false;
    }
#endif

    return pttdown;
}


void CommsbusAudioProcessorEditor::showSettings(bool flag)
{
    DBG("Got settings click");

    if (flag && settingsCalloutBox == nullptr) {
        
        //Viewport * wrap = new Viewport();
        
        Component* dw = this; 
        
#if JUCE_IOS || JUCE_ANDROID
        int defWidth = 320;
        int defHeight = 420;
#else
        int defWidth = 340;
        int defHeight = 400;
#endif
        

        bool firsttime = false;
        if (!mOptionsView) {
            mOptionsView = std::make_unique<OptionsView>(processor, getAudioDeviceManager);
            mOptionsView->getShouldOverrideSampleRateValue = getShouldOverrideSampleRateValue;
            mOptionsView->getShouldCheckForNewVersionValue = getShouldCheckForNewVersionValue;
            mOptionsView->getAllowBluetoothInputValue = getAllowBluetoothInputValue;
            mOptionsView->updateSliderSnap = [this]() {  updateSliderSnap();  };
            mOptionsView->setupLocalisation = [this](const String & lang) {  return setupLocalisation(lang);  };
            mOptionsView->saveSettingsIfNeeded = [this]() {  if (saveSettingsIfNeeded) saveSettingsIfNeeded();  };
            mOptionsView->updateKeybindings = [this]() {  updateUseKeybindings();  };

            mOptionsView->addComponentListener(this);
            firsttime = true;
        }


        auto prefbounds = mOptionsView->getPreferredContentBounds();

        defHeight = prefbounds.getHeight();

        defWidth = jmin(defWidth + 8, dw->getWidth() - 30);
        defHeight = jmin(defHeight + 8, dw->getHeight() - 90); // 24


        auto wrap = std::make_unique<Component>();

        wrap->addAndMakeVisible(mOptionsView.get());

        mOptionsView->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setSize(defWidth,defHeight);

        updateOptionsState();
        
       
        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mTitleLabel->getScreenBounds().reduced(10));
        DBG("callout bounds: " << bounds.toString());
        settingsCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(settingsCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }

        settingsClosedTimestamp = 0;


#if JUCE_WINDOWS
        if (firsttime && JUCEApplicationBase::isStandaloneApp() && getAudioDeviceManager())
        {
            mOptionsView->showWarnings();
        }
#endif


        mOptionsView->grabInitialFocus();

    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(settingsCalloutBox.get())) {
            box->dismiss();
            settingsCalloutBox = nullptr;
        }
    }
}

void CommsbusAudioProcessorEditor::showMonitorDelayView(bool flag)
{
    if (flag && monDelayCalloutBox == nullptr) {

        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        int defWidth = 230;
        int defHeight = 96;
#else
        int defWidth = 210;
        int defHeight = 86;
#endif

        if (!mMonitorDelayView) {
            mMonitorDelayView = std::make_unique<MonitorDelayView>(processor);
        }

        auto minbounds = mMonitorDelayView->getMinimumContentBounds();
        defWidth = minbounds.getWidth();
        defHeight = minbounds.getHeight();


        int extrawidth = 0;
        if (defHeight > dw->getHeight() - 24) {
            extrawidth = wrap->getScrollBarThickness() + 1;
        }

        wrap->setSize(jmin(defWidth + extrawidth, dw->getWidth() - 10), jmin(defHeight, dw->getHeight() - 24));


        mMonitorDelayView->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        //mMonitorDelayView->updateParams();

        wrap->setViewedComponent(mMonitorDelayView.get(), false);
        mMonitorDelayView->setVisible(true);

        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMonDelayButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        monDelayCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(monDelayCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }
    else {
        // dismiss it
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(monDelayCalloutBox.get())) {
            box->dismiss();
            monDelayCalloutBox = nullptr;
        }
    }
}


void CommsbusAudioProcessorEditor::updateState(bool rebuildInputChannels)
{

    currConnected = processor.isConnectedToServer();
    currGroup = processor.getCurrentJoinedGroup();

    if (currConnected && currGroup.isNotEmpty()) {
        mConnectButton->setButtonText(TRANS("Disconnect"));
        mConnectButton->setTextJustification(Justification::centredTop);
        mConnectButton->setToggleState(true, dontSendNotification);
    }
    else {
        mConnectButton->setButtonText(TRANS("Connect..."));
        mConnectButton->setToggleState(false, dontSendNotification);
        
        mPeerContainer->resetPendingUsers();
    }

    if (mRecordingButton) {
        mRecordingButton->setToggleState(processor.isRecordingToFile(), dontSendNotification);
    }

    mReverbModelChoice->setSelectedId(processor.getMainReverbModel(), dontSendNotification);
    mEffectsButton->setToggleState(processor.getMainReverbEnabled(), dontSendNotification);


    if (mReverbEnabledButton->getToggleState()) {
        mReverbHeaderBg->setFill(Colour::fromFloatRGBA(0.2f, 0.5f, 0.7f, 0.5f));                
    } else {
        mReverbHeaderBg->setFill(Colour(0xff2a2a2a));
    }

    mReverbEnabledButton->setAlpha(mReverbEnabledButton->getToggleState() ? 1.0 : 0.5);

    int sendchval = (int) processor.getSendChannels();
    mSendChannelsChoice->setSelectedId(sendchval, dontSendNotification);


    if (sendchval > 0) {
        inputMeter->setFixedNumChannels(sendchval);
    } else {
        //inputMeter->setFixedNumChannels(processor.getMainBusNumInputChannels());
        inputMeter->setFixedNumChannels(processor.getActiveSendChannelCount());
    }

    if (rebuildInputChannels) {
        mInputChannelsContainer->rebuildChannelViews();
    }
    
    bool sendmute = processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainSendMute)->getValue();
    if (!mMainPushToTalkButton->isMouseButtonDown() && !mPushToTalkKeyDown) {
        mMainPushToTalkButton->setVisible(sendmute);
    }
    

    if (processor.getMainReverbModel() == CommsbusAudioProcessor::ReverbModelFreeverb) {
        mReverbPreDelaySlider->setVisible(false);
        mReverbPreDelayLabel->setVisible(false);
    } else {
        mReverbPreDelaySlider->setVisible(true);
        mReverbPreDelayLabel->setVisible(true);        
    }

    mPeerLayoutMinimalButton->setToggleState(processor.getPeerDisplayMode() == CommsbusAudioProcessor::PeerDisplayModeMinimal, dontSendNotification);
    mPeerLayoutFullButton->setToggleState(processor.getPeerDisplayMode() == CommsbusAudioProcessor::PeerDisplayModeFull, dontSendNotification);

    if (!currGroup.isEmpty() && currConnected)
    {
        String grouptext;
        grouptext << (currConnectionInfo.groupIsPublic ? TRANS("[P] ") : "") << currGroup;
        mMainGroupLabel->setText(grouptext, dontSendNotification);
        String userstr;

        if (processor.getNumberRemotePeers() > 0) {
            userstr = String::formatted("%d", processor.getNumberRemotePeers() + 1);
        } else {
            userstr = "1";
        }

        mMainPeerLabel->setText(userstr, dontSendNotification);
        mMainUserLabel->setText(currConnectionInfo.userName, dontSendNotification);

        mMainUserLabel->setEnabled(true);
        mMainPeerLabel->setEnabled(true);
        mMainGroupLabel->setEnabled(true);

        mMainGroupImage->setVisible(true);
        mMainPersonImage->setVisible(true);
        mMainPeerLabel->setVisible(true);
        mMainLinkButton->setVisible(true);
        mMainLinkArrow->setVisible(true);

        if (processor.getNumberRemotePeers() == 0 && mPeerContainer->getPendingPeerCount() == 0) {
            String labstr;
            labstr << TRANS("Waiting for other users to join group") << " \"" << currGroup << "\"...";
            mMainMessageLabel->setText(labstr, dontSendNotification);
            mMainMessageLabel->setVisible(true);
        } else {
            mMainMessageLabel->setText("", dontSendNotification);
            mMainMessageLabel->setVisible(false);
        }
        
        mSetupAudioButton->setVisible(false);
        mPatchbayButton->setVisible(false);
    }
    else {
        mMainGroupLabel->setText("", dontSendNotification);
        mMainUserLabel->setText("", dontSendNotification);

        mMainUserLabel->setEnabled(false);
        mMainPeerLabel->setEnabled(false);
        mMainGroupLabel->setEnabled(false);

        mMainGroupImage->setVisible(false);
        mMainPersonImage->setVisible(false);
        mMainPeerLabel->setVisible(false);
        mMainLinkButton->setVisible(false);
        mMainLinkArrow->setVisible(false);

        mPeerRecImage->setVisible(false);

        mMainMessageLabel->setVisible(true);

        if (processor.getNumberRemotePeers() == 0 /* || !currConnected */ ) {
            String message;
            message += TRANS("Press Connect button to start.") + "\n\n" + TRANS("Please use headphones if you are using a microphone!");
            mMainMessageLabel->setText(message, dontSendNotification);
        } else {
            mMainMessageLabel->setText("", dontSendNotification);
        }

        
        
#if JUCE_IOS || JUCE_ANDROID
        mPatchbayButton->setVisible(false);
        mSetupAudioButton->setVisible(false);
#else


        if (JUCEApplication::isStandaloneApp()) {

            mSetupAudioButton->setVisible(processor.getNumberRemotePeers() == 0);

        } else {
            mSetupAudioButton->setVisible(false);            
        }

        
        if (processor.getNumberRemotePeers() > 1 && !currConnected) {
            mPatchbayButton->setVisible(true);
        } else {
            mPatchbayButton->setVisible(false);            
        }
#endif   
    }
    
#if JUCE_IOS
    mIAAHostButton->setVisible(iaaConnected);
#endif
    
    commandManager.commandStatusChanged();

}


void CommsbusAudioProcessorEditor::parameterChanged (const String& pname, float newValue)
{
    
    if (pname == CommsbusAudioProcessor::paramMainSendMute) {
        {
            const ScopedLock sl (clientStateLock);
            clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, ""));
        }
        triggerAsyncUpdate();
    }
    else if (pname == CommsbusAudioProcessor::paramMainRecvMute) {
        {
            const ScopedLock sl (clientStateLock);
            clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, ""));
        }
        triggerAsyncUpdate();
    }
    
    
    
    
    else if (pname == CommsbusAudioProcessor::paramMainReverbModel) {
        {
            const ScopedLock sl (clientStateLock);
            clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, ""));
        }
        triggerAsyncUpdate();
    }
    else if (pname == CommsbusAudioProcessor::paramMainReverbEnabled) {
        {
            const ScopedLock sl (clientStateLock);
            clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, ""));
        }
        triggerAsyncUpdate();
    }
    else if (pname == CommsbusAudioProcessor::paramSendChannels) {
        {
            const ScopedLock sl (clientStateLock);
            clientEvents.add(ClientEvent(ClientEvent::PeerChangedState, ""));
        }
        triggerAsyncUpdate();
    }
    else if (pname == CommsbusAudioProcessor::paramInMonitorPan1 
             || pname == CommsbusAudioProcessor::paramInMonitorPan2
             || pname == CommsbusAudioProcessor::paramInMonitorMonoPan
             ) 
    {
        mPanChanged = true;
        triggerAsyncUpdate();
    }
}

void CommsbusAudioProcessorEditor::updateServerStatusLabel(const String & mesg, bool mainonly)
{
    const double fadeAfterSec = 5.0;
    //mMainStatusLabel->setText(mesg, dontSendNotification);
    //Desktop::getInstance().getAnimator().fadeIn(mMainStatusLabel.get(), 200);
    //serverStatusFadeTimestamp = Time::getMillisecondCounterHiRes() * 1e-3 + fadeAfterSec;
    
    if (!mainonly) {
        //mServerStatusLabel->setText(mesg, dontSendNotification);
        //mPublicServerStatusInfoLabel->setText(mesg, dontSendNotification);
        //mServerInfoLabel->setVisible(false);
        //mServerStatusLabel->setVisible(true);
        //mPublicServerStatusInfoLabel->setVisible(true);

        mConnectView->updateServerStatusLabel(mesg, mainonly);
    }
}


String CommsbusAudioProcessorEditor::generateNewUsername(const AooServerConnectionInfo & info)
{
    String newname = info.userName;
    
    // look for number as last word
    int spacepos = newname.lastIndexOf(" ");
    if (spacepos >= 0) {
        String lastword = newname.substring(spacepos+1);
        int ival = lastword.getIntValue();
        if (ival > 0) {
            // found a number, increment it
            newname = newname.substring(0, spacepos) + String::formatted(" %d", ival+1);
        }
        else {
            // no number, add one
            newname += " 2";
        }
    } else {
        // nothing, append a number 2
        newname += " 2";
    }
    
    return newname.trim();
}


void CommsbusAudioProcessorEditor::handleAsyncUpdate()
{

    Array<ClientEvent> newevents;
    {
        const ScopedLock sl (clientStateLock);
        newevents = clientEvents;
        clientEvents.clearQuick();
    }
    
    for (auto & ev : newevents) {
        if (ev.type == ClientEvent::PeerChangedState) {
            peerStateUpdated = true;
            //mPeerContainer->updateLayout();
            //mPeerContainer->resized();
            //Timer::callAfterDelay(100, [this](){
            updatePeerState(true);
            updateState(false);
            //});
        }
        else if (ev.type == ClientEvent::ConnectEvent) {
            String statstr;
            if (ev.success) {
                statstr = TRANS("Connected to server");                
            } else {
                if (String(ev.message).contains("access denied")) {
                    statstr = TRANS("Already connected with this user name");

                    // try again with different username (auto-incremented)
                    currConnectionInfo.userName = generateNewUsername(currConnectionInfo);

                    DBG("Trying again with name: " << currConnectionInfo.userName);
                    connectWithInfo(currConnectionInfo, true);
                    
                    return;
                    
                } else {
                    statstr = TRANS("Connect failed: ") + ev.message;
                }

            }
            
            // we might already have autojoined a group, only attempt if we haven't
            if (processor.getCurrentJoinedGroup().isEmpty()) {
                
                // now attempt to join group
                if (ev.success) {
                    
                    if (currConnectionInfo.groupName.isNotEmpty()) {
                        
                        currConnectionInfo.timestamp = Time::getCurrentTime().toMilliseconds();
                        processor.addRecentServerConnectionInfo(currConnectionInfo);
                        
                        processor.setWatchPublicGroups(false);
                        
                        processor.joinServerGroup(currConnectionInfo.groupName, currConnectionInfo.groupPassword, currConnectionInfo.groupIsPublic);
                    }
                    else {
                        // we've connected but have not specified group, assume we want to see public groups
                        processor.setWatchPublicGroups(true);
                        mConnectView->updatePublicGroups();
                    }
                    
                    mConnectView->updateServerFieldsFromConnectionInfo();
                    
                    //mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, "", "", "", "", statstr));
                    
                }
                else {
                    // switch to group page
                    mConnectView->showActiveGroupTab();
                }
            }
            else {
                DBG("Already joined a group, update currConnectionInfo");
                currConnectionInfo.groupName = processor.getCurrentJoinedGroup();
                currConnectionInfo.userName = processor.getCurrentUsername();
            }
            
            updateServerStatusLabel(statstr, false);
            updateState();
        }
        else if (ev.type == ClientEvent::DisconnectEvent) {
            String statstr;
            if (ev.success) {
                statstr = TRANS("Disconnected from server");
            } else {
                //statstr = TRANS("Disconnect failed: ") + ev.message;
            }

            //mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, "", "", "", "", statstr));

            //AccessibilityHandler::postAnnouncement(TRANS("Disconnected"), AccessibilityHandler::AnnouncementPriority::high);


            mPeerContainer->resetPendingUsers();
            updateServerStatusLabel(statstr);
            updatePeerState(true);
            updateState();
        }
        else if (ev.type == ClientEvent::GroupJoinEvent) {
            String statstr;
            if (ev.success) {
                statstr = TRANS("Joined Group: ") + ev.group;

                //AccessibilityHandler::postAnnouncement(statstr, AccessibilityHandler::AnnouncementPriority::high);

                showConnectPopup(false);

                mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, ev.group, "", "", "", statstr));

                if (JUCEApplicationBase::isStandaloneApp() && saveSettingsIfNeeded) {
                    DBG("Saving settings");
                    saveSettingsIfNeeded();
                }


                // need to update layout too
                updateLayout();
                resized();

                mMainMessageLabel->setWantsKeyboardFocus(true);
                if (mMainMessageLabel->isShowing()) {
                    mMainMessageLabel->grabKeyboardFocus();
                }
            } else {

                if (processor.getCurrentJoinedGroup().isEmpty()) {
                    statstr = TRANS("Failed to join group: ") + ev.message;

                    mConnectView->groupJoinFailed();
                    
                    mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, ev.group, "", "", "", statstr));
                    
                    // disconnect
                    processor.disconnectFromServer();
                }
                
            }
            updateServerStatusLabel(statstr, false);
            updateState();
        }
        else if (ev.type == ClientEvent::GroupLeaveEvent) {
            String statstr;
            if (ev.success) {
                statstr = TRANS("Left Group: ") + ev.group;
            } else {
                statstr = TRANS("Failed to leave group: ") + ev.message;
            }

            //AccessibilityHandler::postAnnouncement(statstr, AccessibilityHandler::AnnouncementPriority::high);

            mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, ev.group, "", "", "", statstr));

            mPeerContainer->resetPendingUsers();
            updateServerStatusLabel(statstr);
            updatePeerState(true);
            updateState();
            // need to update layout too
            updateLayout();
            resized();
        }
        else if (ev.type == ClientEvent::PublicGroupModifiedEvent) {
            mConnectView->updatePublicGroups();
        }
        else if (ev.type == ClientEvent::PublicGroupDeletedEvent) {
            mConnectView->updatePublicGroups();
        }
        else if (ev.type == ClientEvent::PeerJoinEvent) {
            DBG("Peer " << ev.user << "joined doing full update");

            if (!currConnectionInfo.groupIsPublic) {
                String mesg;
                mesg << ev.user << TRANS(" - joined group");
                mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, ev.group, ev.user, "", "", mesg));
            }

            // delay update
            Timer::callAfterDelay(200, [this] {
                updatePeerState(true);
                updateState(false);
            });
        }
        else if (ev.type == ClientEvent::PeerLeaveEvent) {
            if (!currConnectionInfo.groupIsPublic) {
                String mesg;
                mesg << ev.user << TRANS(" - left group");
                mChatView->addNewChatMessage(SBChatEvent(SBChatEvent::SystemType, ev.group, ev.user, "", "", mesg));
            }

            mPeerContainer->peerLeftGroup(ev.group, ev.user);

            updatePeerState(true);
            updateState(false);
        }
        else if (ev.type == ClientEvent::PeerPendingJoinEvent) {
            mPeerContainer->peerPendingJoin(ev.group, ev.user);
        }
        else if (ev.type == ClientEvent::PeerFailedJoinEvent) {
            mPeerContainer->peerFailedJoin(ev.group, ev.user);
        }
        else if (ev.type == ClientEvent::PeerBlockedJoinEvent) {
            mPeerContainer->peerBlockedJoin(ev.group, ev.user, ev.message, (int) lrint(ev.floatVal));
        }
        else if (ev.type == ClientEvent::PeerRequestedLatencyMatchEvent) {
            showLatencyMatchPrompt(ev.message, ev.floatVal);
        }
        else if (ev.type == ClientEvent::PeerSuggestedNewGroupEvent) {
            showSuggestedGroupPrompt(ev.user, ev.group, ev.message, ev.success, ev.array);
        }
        else if (ev.type == ClientEvent::PeerBlockedInfoChangedEvent) {
            updatePeerState(true);
        }
    }

    if (haveNewChatEvents.compareAndSetBool(false, true))
    {
        mChatView->refreshMessages();
    }

    if (mReloadFile) {
        mReloadFile = false;
    }
}

void CommsbusAudioProcessorEditor::copyGroupLink()
{
#if JUCE_IOS || JUCE_ANDROID
    String message;
    const bool singleurl = true;

    if (mConnectView && mConnectView->copyInfoToClipboard(singleurl, &message)) {
        /*
         URL url(message);
         if (url.isWellFormed()) {
         Array<URL> urlarray;
         urlarray.add(url);
         safeThis->mScopedShareBox = ContentSharer::shareFilesScoped(urlarray, [safeThis](bool result, const String& msg){ DBG("url share returned " << (int)result << " : " <<  msg);
         safeThis->mScopedShareBox = {};
         });
         } else
         */
        // just share as text for now
        {
            SafePointer<CommsbusAudioProcessorEditor> safeThis(this);
            mScopedShareBox = ContentSharer::shareTextScoped(message, [safeThis](bool result, const String& msg){ DBG("share returned " << (int)result << " : " << msg);
                safeThis->mScopedShareBox = {};
            });
        }
    }
#else
    const bool singleurl = true;
    if (mConnectView && mConnectView->copyInfoToClipboard(singleurl)) {
        auto msg = TRANS("Copied group connection info to clipboard for you to share with others");
        showPopTip(msg, 3000, mMainLinkButton.get());
    }
#endif
}

void CommsbusAudioProcessorEditor::showGroupMenu(bool show)
{
    Array<GenericItemChooserItem> items;

#if JUCE_IOS || JUCE_ANDROID
    items.add(GenericItemChooserItem(TRANS("Share Group Link"), {}, nullptr, false));
#else
    items.add(GenericItemChooserItem(TRANS("Copy Group Link"), {}, nullptr, false));
#endif

    items.add(GenericItemChooserItem(TRANS("Group Latency Match..."), {}, nullptr, true));

    items.add(GenericItemChooserItem(TRANS("VDO.Ninja Video Link..."), {}, nullptr, true));

    items.add(GenericItemChooserItem(TRANS("Suggest New Group..."), {}, nullptr, true));


    Component* dw = mMainLinkButton->findParentComponentOfClass<AudioProcessorEditor>();
    if (!dw) dw = mMainLinkButton->findParentComponentOfClass<Component>();
    Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMainLinkButton->getScreenBounds());

    SafePointer<CommsbusAudioProcessorEditor> safeThis(this);

    auto callback = [safeThis,dw,bounds](GenericItemChooser* chooser,int index) mutable {
        if (!safeThis) return;
        if (index == 0) {
            // copy group link
            safeThis->copyGroupLink();
        } else if (index == 1) {
            // group latency
            safeThis->showLatencyMatchView(true);
        } else if (index == 2) {
            // vdo ninja
            safeThis->showVDONinjaView(true, false);
        }
        else if (index == 3) {
            // suggest new group
            safeThis->showSuggestGroupView(true);
        }
    };

    GenericItemChooser::launchPopupChooser(items, bounds, dw, callback, -1, dw ? dw->getHeight()-30 : 0);
}

class CommsbusAudioProcessorEditor::ApproveComponent : public Component
{
public:
    ApproveComponent(const String & buttonlabel, const String & button2Label="") {

        button.setButtonText(buttonlabel);

        button2.setButtonText(button2Label);

        addAndMakeVisible(label);
        addAndMakeVisible(button);

        if (button2Label.isNotEmpty()) {
            addAndMakeVisible(button2);
        }
    }

    void resized() override {
        FlexBox buttBox;
        buttBox.flexDirection = FlexBox::Direction::row;
        buttBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(1));
        buttBox.items.add(FlexItem(100, 38, button).withMargin(0).withFlex(4));
        if (button2.isVisible()) {
            buttBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(1));
            buttBox.items.add(FlexItem(70, 38, button2).withMargin(0).withFlex(2));
        }
        buttBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(1));

        FlexBox mainBox;
        mainBox.flexDirection = FlexBox::Direction::column;
        mainBox.items.add(FlexItem(4, 4).withMargin(0).withFlex(0));
        mainBox.items.add(FlexItem(200, 40, label).withMargin(0).withFlex(1));
        mainBox.items.add(FlexItem(100, 40, buttBox).withMargin(0).withFlex(0));
        mainBox.items.add(FlexItem(4, 4).withMargin(0).withFlex(0));


        mainBox.performLayout(getLocalBounds());
    }

    Label  label;
    TextButton button;
    TextButton button2;
};

void CommsbusAudioProcessorEditor::showLatencyMatchPrompt(const String & name, float latencyms)
{
    if (!mLatMatchApproveComponent) {
        mLatMatchApproveComponent = std::make_unique<ApproveComponent>(TRANS("Match Latency"), TRANS("Ignore"));
    }


    // jlcc
    if (latmatchCalloutBox == nullptr) {
        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 260;
        const int defHeight = 120;
#else
        const int defWidth = 260;
        const int defHeight = 115;
#endif


        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));

        mLatMatchApproveComponent->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setViewedComponent(mLatMatchApproveComponent.get(), false);
        mLatMatchApproveComponent->setVisible(true);



        String mesg;
        mesg << name << " " << TRANS("requests to use a matched group latency of:");
        mesg << " " << lrintf(latencyms) << " ms";
        
        mLatMatchApproveComponent->label.setText(mesg, dontSendNotification);

        mLatMatchApproveComponent->button.onClick = [this,latencyms]() {
            processor.commitLatencyMatch(latencyms);

            // dismiss it
            if (CallOutBox * box = dynamic_cast<CallOutBox*>(latmatchCalloutBox.get())) {
                box->dismiss();
                latmatchCalloutBox = nullptr;
            }
        };

        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMainLinkButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        latmatchCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(latmatchCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }

}

void CommsbusAudioProcessorEditor::showSuggestedGroupPrompt(const String &name, const String &group, const String & grouppass, bool ispublic, const StringArray & others)
{
    if (!mSuggestedGroupComponent) {
        mSuggestedGroupComponent = std::make_unique<ApproveComponent>(TRANS("Connect To Group"), TRANS("Ignore"));
    }


    if (suggestedGroupCalloutBox == nullptr) {
        auto wrap = std::make_unique<Viewport>();

        Component* dw = this;

#if JUCE_IOS || JUCE_ANDROID
        const int defWidth = 260;
        const int defHeight = 190;
#else
        const int defWidth = 260;
        const int defHeight = 170;
#endif


        wrap->setSize(jmin(defWidth, dw->getWidth() - 20), jmin(defHeight, dw->getHeight() - 24));

        mSuggestedGroupComponent->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setViewedComponent(mSuggestedGroupComponent.get(), false);
        mSuggestedGroupComponent->setVisible(true);

        String mesg;
        if (ispublic) {
            mesg << TRANS("Requested to join a new public group:");
        } else {
            mesg << TRANS("Requested to join a new private group:");
        }

        mesg << "\n   " <<  TRANS("From: ") << name;
        mesg << "\n   " <<  TRANS("New Group: ") << group;
        mesg << "\n   " <<  TRANS("With: ") << others.joinIntoString(", ");

        mSuggestedGroupComponent->label.setText(mesg, dontSendNotification);

        mSuggestedGroupComponent->button.onClick = [this,group, grouppass, ispublic]() {
            // join new group
            currConnectionInfo.groupName = group;
            currConnectionInfo.groupPassword = grouppass;
            currConnectionInfo.groupIsPublic = ispublic;
            connectWithInfo(currConnectionInfo);

            // dismiss it
            if (CallOutBox * box = dynamic_cast<CallOutBox*>(suggestedGroupCalloutBox.get())) {
                box->dismiss();
                suggestedGroupCalloutBox = nullptr;
            }
        };

        mSuggestedGroupComponent->button2.onClick = [this]() {
            // dismiss it
            if (CallOutBox * box = dynamic_cast<CallOutBox*>(suggestedGroupCalloutBox.get())) {
                box->dismiss();
                suggestedGroupCalloutBox = nullptr;
            }
        };

        Rectangle<int> bounds =  dw->getLocalArea(nullptr, mMainLinkButton->getScreenBounds());
        DBG("callout bounds: " << bounds.toString());
        suggestedGroupCalloutBox = & SonoCallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false, [this](const Component * comp) {
            if (comp == mMainLinkButton.get()) return false;
            return true;
        });

        if (SonoCallOutBox * box = dynamic_cast<SonoCallOutBox*>(suggestedGroupCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }
    }

}


void CommsbusAudioProcessorEditor::showChatPanel(bool show, bool allowresize)
{

#if !(JUCE_IOS || JUCE_ANDROID)
    // attempt resize
    if (allowresize && show && !isNarrow) {
        auto * display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
        int maxwidth = display ? display->userArea.getWidth() : 1600;
        int newwidth = jmin(maxwidth, getWidth() + mChatView->getWidth());
        mAboutToShowChat = true;
        if (abs(newwidth - getWidth()) > 10 ) {
            if (abs(newwidth - getWidth()) < mChatView->getWidth()) {
                mChatShowDidResize = false;
            } else {
                mChatShowDidResize = true;
            }
            setSize(newwidth, getHeight());
        }
        else {
            mChatShowDidResize = false;
        }
    }
    else if (show) {
        mChatShowDidResize = false;
    }
#else
    mChatShowDidResize = false;
#endif

    mChatView->setVisible(show);

#if !(JUCE_IOS || JUCE_ANDROID)
    if (show && allowresize) {
        mChatView->setFocusToChat();
    }
#endif
}

void CommsbusAudioProcessorEditor::parentHierarchyChanged()
{    
    AudioProcessorEditor::parentHierarchyChanged();
}

//==============================================================================
void CommsbusAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

}

void CommsbusAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    Component::resized();
 
#if JUCE_IOS || JUCE_ANDROID
    int narrowthresh = 480; //520;
    int rnarrowthresh = 360; //520;
#else
    int narrowthresh = 690 ; // 644; // 588; //520;
    int rnarrowthresh = 380; // 588; //520;
#endif


    if (mChatView->isVisible()) {
        narrowthresh += mChatView->getWidth();
    }

    

    bool nownarrow = getWidth() < narrowthresh;
    if (nownarrow != isNarrow) {
        isNarrow = nownarrow;
        mPeerContainer->setNarrowMode(isNarrow);
        mInputChannelsContainer->setNarrowMode(isNarrow);
        updateLayout();
    }

    bool nowrnarrow = getWidth() < rnarrowthresh;
    if (nowrnarrow != isReallyNarrow) {
        isReallyNarrow = nowrnarrow;
        updateLayout();
    }

   
    
    DBG("RESIZED to " << getWidth() << " " << getHeight());
    
    auto mainBounds = getLocalBounds();
    const auto menuHeight = getLookAndFeel().getDefaultMenuBarHeight();

    processor.setLastPluginBounds(mainBounds); // just relative
    

    if (mMenuBar) {
        auto menuBounds = mainBounds.removeFromTop(menuHeight);
        mMenuBar->setBounds(menuBounds);
    }

    int chatwidth = processor.getLastChatWidth();

    mChatOverlay = mainBounds.getWidth() - chatwidth < 340;

    mIgnoreResize = true; // important!
 
    mChatView->setBounds(getLocalBounds().removeFromRight(chatwidth));

    if (mChatView->isVisible() || mAboutToShowChat) {
        if (!isNarrow || !mChatOverlay) {
            // take it off
            mChatView->setBounds(mainBounds.removeFromRight(chatwidth));
        }
    }


    

    mIgnoreResize = false;


    mTopLevelContainer->setBounds(getLocalBounds());

    mainBox.performLayout(mainBounds);


    mChatEdgeResizer->setBounds(mChatView->getLocalBounds().withWidth(5));



    int inchantargwidth = mMainViewport->getWidth() - 10;

    if (mInputChannelsContainer->getEstimatedWidth() != inchantargwidth && mInputChannelsContainer->isVisible()) {
        mInputChannelsContainer->setEstimatedWidth(inchantargwidth);
        mInputChannelsContainer->updateLayout(false);
    }

    Rectangle<int> peersminbounds = mPeerContainer->getMinimumContentBounds();
    Rectangle<int> inmixminbounds = mInputChannelsContainer->getMinimumContentBounds();

    Rectangle<int> inmixactualbounds = Rectangle<int>(0,0,0,0);

    if (mInputChannelsContainer->isVisible()) {
        inmixactualbounds = Rectangle<int>(0, 0,
                                           std::max(inmixminbounds.getWidth(), inchantargwidth),
                                           inmixminbounds.getHeight() + 5);

        mInputChannelsContainer->setBounds(inmixactualbounds);
    }

    int vgap = inmixactualbounds.getHeight() > 0 ?  6 : 0;

    mPeerContainer->setBounds(Rectangle<int>(0, inmixactualbounds.getBottom() + vgap, std::max(peersminbounds.getWidth(), mMainViewport->getWidth() - 10), std::max(peersminbounds.getHeight() + 5, mMainViewport->getHeight() - inmixactualbounds.getHeight() - vgap)));

    Rectangle<int> totbounds = mPeerContainer->getBounds().getUnion(inmixactualbounds);
    //totbounds.setHeight(totbounds.getHeight());


    auto viewpos = mMainViewport->getViewPosition();

    mMainContainer->setBounds(totbounds);

    mMainViewport->setViewPosition(viewpos);


#if JUCE_IOS || JUCE_ANDROID
    mSetupAudioButton->setSize(150, 1);
#else
    mSetupAudioButton->setSize(150, 50);	
#endif
	
    mSetupAudioButton->setCentrePosition(mMainViewport->getX() + 0.5*mMainViewport->getWidth(), mMainViewport->getY() + inmixactualbounds.getHeight() + 45);
    
    mMainMessageLabel->setBounds(mMainViewport->getX() + 10, mSetupAudioButton->getBottom() + 10, mMainViewport->getRight() - mMainViewport->getX() - 20, jmin(120, mMainViewport->getBottom() - (mSetupAudioButton->getBottom() + 10)));
    


    //auto grouptextbounds = Rectangle<int>(mMainPeerLabel->getX(), mMainGroupImage->getY(), mMainUserLabel->getRight() - mMainPeerLabel->getX(),  mMainGroupImage->getHeight()).expanded(2, 2);
    //auto grouptextbounds = Rectangle<int>(mMainPeerLabel->getX(), mMainGroupImage->getY(), mMainUserLabel->getRight() - mMainPeerLabel->getX(),  mMainUserLabel->getBottom() - mMainGroupImage->getY());
    auto grouptextbounds = Rectangle<int>(mMainPeerLabel->getX(), mPeerLayoutFullButton->getY(), mMainUserLabel->getRight() - mMainPeerLabel->getX(),  mPeerLayoutFullButton->getHeight());
    mMainLinkButton->setBounds(grouptextbounds);

    auto triwidth = 12;
    auto triheight = 8;
    //auto linkarrowrect = Rectangle<int>(mMainLinkButton->getRight() - triwidth - 4, mMainLinkButton->getBottom() - mMainLinkButton->getHeight()/2 - triheight + 2, triwidth, triheight);
    auto linkarrowrect = Rectangle<int>(mMainLinkButton->getRight() - triwidth - 4, mMainLinkButton->getBottom() - triheight - 4, triwidth, triheight);
    mMainLinkArrow->setTransformToFit(linkarrowrect.toFloat(), RectanglePlacement::stretchToFit);


    const auto precwidth = 20;
    auto peerrecbounds = Rectangle<int>(mMainLinkButton->getRight() - precwidth - 4, mMainLinkButton->getY() + mMainLinkButton->getHeight()/2 - precwidth/2, precwidth,  precwidth);
    mPeerRecImage->setTransformToFit(peerrecbounds.toFloat(), RectanglePlacement::fillDestination);




    // connect component stuff
    if (mConnectView) {
        mConnectView->setBounds(getLocalBounds());
    }

    mConnectionTimeLabel->setBounds(mConnectButton->getBounds().removeFromBottom(16));
    
    if (mRecordingButton) {
    }

    mDrySlider->setMouseDragSensitivity(jmax(128, mDrySlider->getWidth()));
    mOutGainSlider->setMouseDragSensitivity(jmax(128, mOutGainSlider->getWidth()));
    //mInGainSlider->setMouseDragSensitivity(jmax(128, mInGainSlider->getWidth()));

    mDryLabel->setBounds(mDrySlider->getBounds().removeFromTop(17).removeFromLeft(mDrySlider->getWidth() - mDrySlider->getTextBoxWidth() + 3).translated(4, -2));
    //mInGainLabel->setBounds(mInGainSlider->getBounds().removeFromTop(14).removeFromLeft(mInGainSlider->getWidth() - mInGainSlider->getTextBoxWidth() + 3).translated(4, 0));
    mOutGainLabel->setBounds(mOutGainSlider->getBounds().removeFromTop(17).removeFromLeft(mOutGainSlider->getWidth() - mOutGainSlider->getTextBoxWidth() + 3).translated(4, -2));


    
    Component* dw = this; 
    
    if (auto * callout = dynamic_cast<CallOutBox*>(effectsCalloutBox.get())) {
        callout->updatePosition(dw->getLocalArea(nullptr, mEffectsButton->getScreenBounds()), dw->getLocalBounds());
    }


    updateSliderSnap();

}


void CommsbusAudioProcessorEditor::updateLayout()
{
    int minKnobWidth = 50;
    int minSliderWidth = 50;
    int minPannerWidth = 40;
    int minitemheight = 36;
    int knobitemheight = 80;
    int minpassheight = 30;
    int setitemheight = 36;
    int minButtonWidth = 90;
    int inmeterwidth = 22 ;
    int outmeterwidth = 22 ;
    int iconheight = 18; // 24;
    int iconwidth = iconheight;
    int knoblabelheight = 18;
    int toolwidth = 44;
    int mintoolwidth = 38;
#if JUCE_IOS || JUCE_ANDROID
    // make the button heights a bit more for touchscreen purposes
    iconheight = 22; // 24;
    minitemheight = 44;
    knobitemheight = 90;
    minpassheight = 38;
#endif

    // adjust max meterwidth if channel count > 2
    if (processor.getActiveSendChannelCount() > 2 && processor.getSendChannels() == 0) {
        inmeterwidth = 5 * processor.getActiveSendChannelCount();
    }
    if (processor.getMainBusNumOutputChannels() > 2) {
        outmeterwidth = 5 * processor.getMainBusNumOutputChannels();
    }

    int mutew = 52;
    int inmixw = 74;
    int choicew = inmixw + mutew + 3;

    inGainBox.items.clear();
    inGainBox.flexDirection = FlexBox::Direction::row;
    //inGainBox.items.add(FlexItem(minKnobWidth, minitemheight, *mInGainSlider).withMargin(0).withFlex(1));
    //inGainBox.items.add(FlexItem(choicew, minitemheight, *mSendChannelsLabel).withMargin(0).withFlex(0.5));
    inGainBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1));
    inGainBox.items.add(FlexItem(choicew, minitemheight, *mSendChannelsChoice).withMargin(0).withFlex(1).withMaxWidth(160));
    inGainBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1));

    dryBox.items.clear();
    dryBox.flexDirection = FlexBox::Direction::row;
    dryBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    dryBox.items.add(FlexItem(minKnobWidth, minitemheight, *mDrySlider).withMargin(0).withFlex(1));

    
    outBox.items.clear();
    outBox.flexDirection = FlexBox::Direction::column;
    outBox.items.add(FlexItem(minKnobWidth, minitemheight, *mOutGainSlider).withMargin(0).withFlex(1)); //.withAlignSelf(FlexItem::AlignSelf::center));

    inMeterBox.items.clear();
    inMeterBox.flexDirection = FlexBox::Direction::column;
    inMeterBox.items.add(FlexItem(inmeterwidth, minitemheight, *inputMeter).withMargin(0).withFlex(1).withMaxWidth(inmeterwidth).withAlignSelf(FlexItem::AlignSelf::center));

    mainMeterBox.items.clear();
    mainMeterBox.flexDirection = FlexBox::Direction::column;
    mainMeterBox.items.add(FlexItem(outmeterwidth, minitemheight, *outputMeter).withMargin(0).withFlex(1).withMaxWidth(outmeterwidth).withAlignSelf(FlexItem::AlignSelf::center));
    
    inputPannerBox.items.clear();
    inputPannerBox.flexDirection = FlexBox::Direction::row;
    inputPannerBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.2));
    inputPannerBox.items.add(FlexItem(inmixw, minitemheight, *mInMixerButton).withMargin(0).withFlex(1).withMaxWidth(160 - mutew - 3));
    inputPannerBox.items.add(FlexItem(3, 6).withMargin(0).withFlex(0));
    inputPannerBox.items.add(FlexItem(mutew, minitemheight, *mInMuteButton).withMargin(0).withFlex(0.0));
    //inputPannerBox.items.add(FlexItem(mutew, minitemheight, *mInEffectsButton).withMargin(0).withFlex(0));
    inputPannerBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.2));

    inputLeftBox.items.clear();
    inputLeftBox.flexDirection = FlexBox::Direction::column;
    inputLeftBox.items.add(FlexItem(choicew + 4, minitemheight, inGainBox).withMargin(0).withFlex(1)); //.withMaxWidth(isNarrow ? 160 : 120));
    inputLeftBox.items.add(FlexItem(4, 4).withMargin(0).withFlex(0));
    inputLeftBox.items.add(FlexItem(mutew+inmixw + 10, minitemheight, inputPannerBox).withMargin(0).withFlex(1)); //.withMaxWidth(maxPannerWidth));

    inputButtonBox.items.clear();
    inputButtonBox.flexDirection = FlexBox::Direction::row;
    inputButtonBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0.1));
    inputButtonBox.items.add(FlexItem(mutew, minitemheight, *mInSoloButton).withMargin(0).withFlex(0) ); //.withMaxWidth(maxPannerWidth));
    inputButtonBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.2));
    //inputButtonBox.items.add(FlexItem(mutew, minitemheight, *mMonDelayButton).withMargin(0).withFlex(0) ); //.withMaxWidth(maxPannerWidth));
    inputButtonBox.items.add(FlexItem(3, 4));
    inputButtonBox.items.add(FlexItem(toolwidth, minitemheight, *mChatButton).withMargin(0).withFlex(0) ); //.withMaxWidth(maxPannerWidth));
    inputButtonBox.items.add(FlexItem(7, 6).withMargin(0).withFlex(0));

    inputRightBox.items.clear();
    inputRightBox.flexDirection = FlexBox::Direction::column;
    inputRightBox.items.add(FlexItem(minSliderWidth, minitemheight, dryBox).withMargin(0).withFlex(1)); //.withMaxWidth(isNarrow ? 160 : 120));
    inputRightBox.items.add(FlexItem(4, 4).withMargin(0).withFlex(0));
    inputRightBox.items.add(FlexItem(2*mutew + 10, minitemheight, inputButtonBox).withMargin(0).withFlex(1)); //.withMaxWidth(maxPannerWidth));

    inputMainBox.items.clear();
    inputMainBox.flexDirection = FlexBox::Direction::row;
    inputMainBox.items.add(FlexItem(jmax(choicew + 4, 2*mutew + 16), minitemheight + minitemheight + 4, inputLeftBox).withMargin(0).withFlex(1)); //.withMaxWidth(isNarrow ? 160 : 120));
    inputMainBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    inputMainBox.items.add(FlexItem(inmeterwidth, minitemheight, inMeterBox).withMargin(0).withFlex(0));
    inputMainBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    inputMainBox.items.add(FlexItem(jmax(minSliderWidth, 2*mutew + 16), minitemheight + minitemheight +4, inputRightBox).withMargin(0).withFlex(1)) ; //.withMaxWidth(isNarrow ? 160 : 120));

    int minmainw = jmax(minPannerWidth, 2*mutew + 8) + inmeterwidth + jmax(minPannerWidth, 2*mutew + 8);
    
    outputMainBox.items.clear();
    outputMainBox.flexDirection = FlexBox::Direction::row;
    outputMainBox.items.add(FlexItem(7, 6).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(toolwidth, minitemheight, *mBufferMinButton).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(toolwidth, minitemheight, *mEffectsButton).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(minSliderWidth, minitemheight, outBox).withMargin(0).withFlex(1)); //.withMaxWidth(isNarrow ? 160 : 120));
    outputMainBox.items.add(FlexItem(4, 6).withMargin(0).withFlex(0));
    outputMainBox.items.add(FlexItem(outmeterwidth, minitemheight, mainMeterBox).withMargin(0).withFlex(0));
    if (isNarrow) {
        outputMainBox.items.add(FlexItem(21, 6).withMargin(0).withFlex(0));
    } else {
        outputMainBox.items.add(FlexItem(16, 6).withMargin(0).withFlex(0));        
    }

    
    paramsBox.items.clear();
    paramsBox.flexDirection = FlexBox::Direction::column;
    paramsBox.items.add(FlexItem(minmainw, minitemheight + minitemheight + 4, inputMainBox).withMargin(2).withFlex(0));



    mainGroupBox.items.clear();
    mainGroupBox.flexDirection = FlexBox::Direction::row;
    //mainGroupBox.items.add(FlexItem(24, minitemheight/2, *mMainPeerLabel).withMargin(0).withFlex(0));
    mainGroupBox.items.add(FlexItem(iconwidth, iconheight, *mMainGroupImage).withMargin(0).withFlex(0));
    mainGroupBox.items.add(FlexItem(2, 4));
    mainGroupBox.items.add(FlexItem(minButtonWidth, minitemheight/2 - 0, *mMainGroupLabel).withMargin(0).withFlex(1));


    mainUserBox.items.clear();
    mainUserBox.flexDirection = FlexBox::Direction::row;
    //mainUserBox.items.add(FlexItem(24, 4));
    mainUserBox.items.add(FlexItem(iconwidth, iconheight, *mMainPersonImage).withMargin(0).withFlex(0));
    mainUserBox.items.add(FlexItem(2, 4));
    mainUserBox.items.add(FlexItem(minButtonWidth, minitemheight/2 - 0, *mMainUserLabel).withMargin(0).withFlex(1));

    mainGroupUserBox.items.clear();
    mainGroupUserBox.flexDirection = FlexBox::Direction::column;
    //mainGroupUserBox.items.add(FlexItem(2, 2));
    mainGroupUserBox.items.add(FlexItem(minButtonWidth, minitemheight/2 - 4, mainGroupBox).withMargin(0).withFlex(0));
    mainGroupUserBox.items.add(FlexItem(2, 2));
    mainGroupUserBox.items.add(FlexItem(minButtonWidth, minitemheight/2 - 4, mainUserBox).withMargin(0).withFlex(0));
    mainGroupUserBox.items.add(FlexItem(2, 2));

    mainGroupLayoutBox.items.clear();
    mainGroupLayoutBox.flexDirection = FlexBox::Direction::row;
    mainGroupLayoutBox.items.add(FlexItem(1, 4));
    mainGroupLayoutBox.items.add(FlexItem(toolwidth, minitemheight, *mPeerLayoutMinimalButton).withMargin(0).withFlex(0));
    mainGroupLayoutBox.items.add(FlexItem(toolwidth, minitemheight, *mPeerLayoutFullButton).withMargin(0).withFlex(0));
    //mainGroupLayoutBox.items.add(FlexItem(3, 4));
    //mainGroupLayoutBox.items.add(FlexItem(toolwidth, minitemheight, *mChatButton).withMargin(0).withFlex(0) ); //.withMaxWidth(maxPannerWidth));
    mainGroupLayoutBox.items.add(FlexItem(4, 4));
    mainGroupLayoutBox.items.add(FlexItem(24, minitemheight, *mMainPeerLabel).withMargin(0).withFlex(0));
    mainGroupLayoutBox.items.add(FlexItem(minButtonWidth, minitemheight - 5, mainGroupUserBox).withMargin(0).withFlex(1));
    if (processor.isConnectedToServer() && processor.getCurrentJoinedGroup().isNotEmpty() && !isReallyNarrow) {
        mainGroupLayoutBox.items.add(FlexItem(3, 4).withMargin(1).withFlex(0.0));
        mainGroupLayoutBox.items.add(FlexItem(toolwidth, minitemheight, *mVideoButton).withMargin(0).withFlex(0));
        mVideoButton->setVisible(true);
    }
    else {
        mVideoButton->setVisible(false);
    }



    connectBox.items.clear();
    connectBox.flexDirection = FlexBox::Direction::column;
    connectBox.items.add(FlexItem(4, 2));
    connectBox.items.add(FlexItem(minButtonWidth, minitemheight, titleBox).withMargin(0).withFlex(1));
    connectBox.items.add(FlexItem(4, 4));
    connectBox.items.add(FlexItem(minButtonWidth, minitemheight, mainGroupLayoutBox).withMargin(0).withFlex(1));
    connectBox.items.add(FlexItem(4, 2));

    titleBox.items.clear();
    titleBox.flexDirection = FlexBox::Direction::row;
    titleBox.items.add(FlexItem(2, 20).withMargin(1).withFlex(0));
    titleBox.items.add(FlexItem(minitemheight - 8, minitemheight - 12, *mSettingsButton).withMargin(1).withMaxWidth(44).withFlex(0));
    titleBox.items.add(FlexItem(86, 20, *mTitleLabel).withMargin(1).withFlex(0));
    if (iaaConnected) {
        titleBox.items.add(FlexItem(4, 4).withMargin(1).withFlex(0.0));
        titleBox.items.add(FlexItem(minitemheight, minitemheight, *mIAAHostButton).withMargin(0).withFlex(0));
    }
    titleBox.items.add(FlexItem(4, 4).withMargin(1).withFlex(0.0));
    titleBox.items.add(FlexItem(minButtonWidth, minitemheight, *mConnectButton).withMargin(0).withFlex(1));
    if (processor.isConnectedToServer() && processor.getCurrentJoinedGroup().isNotEmpty()) {
        titleBox.items.add(FlexItem(3, 4).withMargin(1).withFlex(0.0));
        titleBox.items.add(FlexItem(toolwidth, minitemheight, *mAltConnectButton).withMargin(0).withFlex(0));
        mAltConnectButton->setVisible(true);
    }
    else {
        mAltConnectButton->setVisible(false);
    }

    
    middleBox.items.clear();
    int midboxminheight =  minitemheight * 2 + 8 ; //(knobitemheight + 4);
    int midboxminwidth = 296;
    
    if (isNarrow) {
        middleBox.flexDirection = FlexBox::Direction::column;
        middleBox.items.add(FlexItem(150, minitemheight*2 + 8, connectBox).withMargin(2).withFlex(0));
        middleBox.items.add(FlexItem(5, 2));
        middleBox.items.add(FlexItem(100, minitemheight + minitemheight + 6, paramsBox).withMargin(2).withFlex(0));
        middleBox.items.add(FlexItem(6, 2));

        //midboxminheight = (minitemheight*1.5 + minitemheight * 2 + 20);
        midboxminwidth = 190;

        midboxminheight = 0;
        for (auto & item : middleBox.items) {
            midboxminheight += item.minHeight;
        }

    } else {
        middleBox.flexDirection = FlexBox::Direction::row;        
        middleBox.items.add(FlexItem(280, minitemheight*2+8 , connectBox).withMargin(2).withFlex(1.5).withMaxWidth(470));
        middleBox.items.add(FlexItem(1, 1).withMaxWidth(3).withFlex(0.5));
        middleBox.items.add(FlexItem(minKnobWidth*4 + inmeterwidth*2, minitemheight + minitemheight, paramsBox).withMargin(2).withFlex(4));

        midboxminwidth = 0;
        for (auto & item : middleBox.items) {
            midboxminwidth += item.minWidth;
        }

    }



    toolbarTextBox.items.clear();
    toolbarTextBox.flexDirection = FlexBox::Direction::column;
    toolbarTextBox.items.add(FlexItem(40, minitemheight/2, *mMainStatusLabel).withMargin(0).withFlex(1));








    
    // effects

    reverbSizeBox.items.clear();
    reverbSizeBox.flexDirection = FlexBox::Direction::column;
    reverbSizeBox.items.add(FlexItem(minKnobWidth, knoblabelheight, *mReverbSizeLabel).withMargin(0).withFlex(0));
    reverbSizeBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbSizeSlider).withMargin(0).withFlex(1));

    reverbLevelBox.items.clear();
    reverbLevelBox.flexDirection = FlexBox::Direction::column;
    reverbLevelBox.items.add(FlexItem(minKnobWidth, knoblabelheight, *mReverbLevelLabel).withMargin(0).withFlex(0));
    reverbLevelBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbLevelSlider).withMargin(0).withFlex(1));

    reverbDampBox.items.clear();
    reverbDampBox.flexDirection = FlexBox::Direction::column;
    reverbDampBox.items.add(FlexItem(minKnobWidth, knoblabelheight, *mReverbDampingLabel).withMargin(0).withFlex(0));
    reverbDampBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbDampingSlider).withMargin(0).withFlex(1));

    reverbPreDelayBox.items.clear();
    reverbPreDelayBox.flexDirection = FlexBox::Direction::column;
    reverbPreDelayBox.items.add(FlexItem(minKnobWidth, knoblabelheight, *mReverbPreDelayLabel).withMargin(0).withFlex(0));
    reverbPreDelayBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbPreDelaySlider).withMargin(0).withFlex(1));

    
    reverbCheckBox.items.clear();
    reverbCheckBox.flexDirection = FlexBox::Direction::row;
    reverbCheckBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(0));
    reverbCheckBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbEnabledButton).withMargin(0).withFlex(0));
    reverbCheckBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbTitleLabel).withMargin(0).withFlex(2));
    reverbCheckBox.items.add(FlexItem(minKnobWidth, minitemheight, *mReverbModelChoice).withMargin(0).withFlex(1));
    reverbCheckBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(0));


    
    reverbKnobBox.items.clear();
    reverbKnobBox.flexDirection = FlexBox::Direction::row;
    reverbKnobBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(0));
    reverbKnobBox.items.add(FlexItem(minKnobWidth, minitemheight, reverbPreDelayBox).withMargin(0).withFlex(1));
    reverbKnobBox.items.add(FlexItem(minKnobWidth, minitemheight, reverbLevelBox).withMargin(0).withFlex(1));
    reverbKnobBox.items.add(FlexItem(minKnobWidth, minitemheight, reverbSizeBox).withMargin(0).withFlex(1));
    reverbKnobBox.items.add(FlexItem(minKnobWidth, minitemheight, reverbDampBox).withMargin(0).withFlex(1));
    reverbKnobBox.items.add(FlexItem(5, 5).withMargin(0).withFlex(0));

    reverbBox.items.clear();
    reverbBox.flexDirection = FlexBox::Direction::column;
    reverbBox.items.add(FlexItem(6, 5).withMargin(0).withFlex(0));
    reverbBox.items.add(FlexItem(100, minitemheight, reverbCheckBox).withMargin(0).withFlex(0));
    reverbBox.items.add(FlexItem(6, 5).withMargin(0).withFlex(0));
    reverbBox.items.add(FlexItem(100, knoblabelheight + minitemheight, reverbKnobBox).withMargin(0).withFlex(1));

    
    effectsBox.items.clear();
    effectsBox.flexDirection = FlexBox::Direction::column;
    effectsBox.items.add(FlexItem(minKnobWidth, knoblabelheight + minitemheight + 10, reverbBox).withMargin(0).withFlex(1));
    
    
    toolbarBox.items.clear();
    toolbarBox.flexDirection = FlexBox::Direction::row;
    toolbarBox.items.add(FlexItem(7, 6).withMargin(0).withFlex(0));
    toolbarBox.items.add(FlexItem(toolwidth, minitemheight, *mMainMuteButton).withMargin(0).withFlex(0));
    toolbarBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1).withMaxWidth(8));
    toolbarBox.items.add(FlexItem(toolwidth, minitemheight, *mMainRecvMuteButton).withMargin(0).withFlex(0));
    toolbarBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1).withMaxWidth(8));
    toolbarBox.items.add(FlexItem(toolwidth, minitemheight, *mMainPushToTalkButton).withMargin(0).withFlex(0));
    toolbarBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1));
    toolbarBox.items.add(FlexItem(2, 5).withMargin(0).withFlex(0.1));

   
#if JUCE_IOS || JUCE_ANDROID
#else
    if (processor.getCurrentJoinedGroup().isEmpty() && processor.getNumberRemotePeers() > 1) {
        toolbarBox.items.add(FlexItem(6, 6).withMargin(1).withFlex(0.2));
        toolbarBox.items.add(FlexItem(60, minitemheight, *mPatchbayButton).withMargin(2).withFlex(0.5).withMaxWidth(120));
    }
#endif

    if (mRecordingButton) {



        transportWaveBox.items.clear();
        transportWaveBox.flexDirection = FlexBox::Direction::row;

        transportWaveBox.items.add(FlexItem(isNarrow ? 11 : 5, 6).withMargin(0).withFlex(0));
        transportWaveBox.items.add(FlexItem(isNarrow ? 17 : 3, 6).withMargin(0).withFlex(0));

        
        if ( ! isNarrow) {

        }
        else {

        }
            
#if JUCE_IOS || JUCE_ANDROID
#else
#endif

        toolbarBox.items.add(FlexItem(toolwidth, minitemheight, *mRecordingButton).withMargin(0).withFlex(0));
        toolbarBox.items.add(FlexItem(2, 6).withMargin(0).withFlex(0.1).withMaxWidth(6));
    }

    if (!isNarrow) {
        toolbarBox.items.add(FlexItem(1, 5).withMargin(0).withFlex(0.1));
        toolbarBox.items.add(FlexItem(120, minitemheight, outputMainBox).withMargin(0).withFlex(1).withMaxWidth(390));
        toolbarBox.items.add(FlexItem(6, 6).withMargin(0).withFlex(0));
    }
    else {    
        toolbarBox.items.add(FlexItem(14, 6).withMargin(0).withFlex(0));
    }
    

    
    int minheight = 0;
    
    mainBox.items.clear();
    mainBox.flexDirection = FlexBox::Direction::column;    
    mainBox.items.add(FlexItem(midboxminwidth, midboxminheight, middleBox).withMargin(3).withFlex(0)); minheight += midboxminheight;
    mainBox.items.add(FlexItem(120, 50, *mMainViewport).withMargin(3).withFlex(3)); minheight += 50 + 6;
    mainBox.items.add(FlexItem(10, 2).withFlex(0)); minheight += 2;

    
    
    if (isNarrow) {
        mainBox.items.add(FlexItem(100, minitemheight + 4, outputMainBox).withMargin(0).withFlex(0)); 
        mainBox.items.add(FlexItem(4, 4).withMargin(0).withFlex(0));
        minheight += minitemheight + 8;
    }
    mainBox.items.add(FlexItem(100, minitemheight + 4, toolbarBox).withMargin(0).withFlex(0)); minheight += minitemheight + 10;
    mainBox.items.add(FlexItem(10, 6).withFlex(0));
        

}

void CommsbusAudioProcessorEditor::showPopTip(const String & message, int timeoutMs, Component * target, int maxwidth)
{
    if (!popTip) {
        popTip = std::make_unique<BubbleMessageComponent>();
    }
    
    popTip->setAllowedPlacement(BubbleMessageComponent::BubblePlacement::above | BubbleMessageComponent::BubblePlacement::below);
    
    if (target) {
        if (auto * parent = target->findParentComponentOfClass<AudioProcessorEditor>()) {
            parent->addChildComponent (popTip.get());
            parent->toFront(false);
        }
        else {
            addChildComponent(popTip.get());            
        }
    }
    else 
    {
        addChildComponent(popTip.get());
    }
    
    
    AttributedString text(message);
    text.setJustification (Justification::centred);
    text.setColour (findColour (TextButton::textColourOffId));
    text.setFont(Font(12));
    if (target) {
        popTip->showAt(target, text, timeoutMs);
    }
    else {
        Rectangle<int> topbox(getWidth()/2 - maxwidth/2, 0, maxwidth, 2);
        popTip->showAt(topbox, text, timeoutMs);
    }

    popTip->toFront(false);

    //Timer::callAfterDelay(200, [message]() {
    //    AccessibilityHandler::postAnnouncement(message, AccessibilityHandler::AnnouncementPriority::high);
    //});
}


void CommsbusAudioProcessorEditor::genericItemChooserSelected(GenericItemChooser *comp, int index)
{
    int choosertag = comp->getTag();

    if (choosertag == 1000) {
        if (index == 0) {
            // trim to new
        }
        else if (index == 1) {
#if JUCE_IOS || JUCE_ANDROID
            // share
            Array<URL> urlarray;
            SafePointer<CommsbusAudioProcessorEditor> safeThis(this);
            mScopedShareBox = ContentSharer::shareFilesScoped(urlarray, [safeThis](bool result, const String& msg){ DBG("url share returned " << (int)result << " : " << msg);
                safeThis->mScopedShareBox = {};
            });
#else
            // reveal
            
#endif

        }
    }
    
    if (CallOutBox* const cb = comp->findParentComponentOfClass<CallOutBox>()) {
        cb->dismiss();
    }

}


void CommsbusAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster* /*source*/)
{
}

bool CommsbusAudioProcessorEditor::setupLocalisation(const String & overrideLang)
{
    String displang = SystemStats::getDisplayLanguage();
    String lang = SystemStats::getDisplayLanguage();

    String origslang = lang.initialSectionNotContaining("_").initialSectionNotContaining("-").toLowerCase();

    // currently ignore the system default language if it's one of our
    // non-vetted translations
    if (origslang == "nl"
        || origslang == "ja") {
        // force default to english
        displang = lang = "en-us";
    }

    if (overrideLang.isNotEmpty()) {
        displang = lang = overrideLang;
    }

    LocalisedStrings::setCurrentMappings(nullptr);

    bool retval = false;

    int retbytes = 0;
    int retfbytes = 0;
    String region = SystemStats::getUserRegion();

    String sflang = lang.initialSectionNotContaining("_").toLowerCase().replace("-", "");
    String slang = lang.initialSectionNotContaining("_").initialSectionNotContaining("-").toLowerCase();

    String resname = String("localized_") + slang + String("_txt");
    String resfname = String("localized_") + sflang + String("_txt");
    String resfullfilename = String("localized_") + lang.toLowerCase() + String(".txt");
    String resfilename = String("localized_") + slang + String(".txt");

    const char * rawdata = BinaryData::getNamedResource(resname.toRawUTF8(), retbytes);
    const char * rawdataf = BinaryData::getNamedResource(resfname.toRawUTF8(), retfbytes);

    File   userfilename;
    if (JUCEApplication::isStandaloneApp() && mSettingsFolder.getFullPathName().isNotEmpty()) {
        userfilename = mSettingsFolder.getChildFile(resfullfilename);
        if (!userfilename.existsAsFile()) {
            userfilename = mSettingsFolder.getChildFile(resfilename);
        }
    }


    if (userfilename.existsAsFile()) {
        DBG("Found user localization file for language: " << lang << "  region: " << region << " displang: " <<  displang <<  "  - resname: " << resfname);
        LocalisedStrings * lstrings = new LocalisedStrings(userfilename.loadFileAsString(), true);

        LocalisedStrings::setCurrentMappings(lstrings);
        retval = true;
    }
    else if (rawdataf) {
        DBG("Found fulldisp localization for language: " << lang << "  region: " << region << " displang: " <<  displang <<  "  - resname: " << resfname);
        LocalisedStrings * lstrings = new LocalisedStrings(String::createStringFromData(rawdataf, retfbytes), true);

        LocalisedStrings::setCurrentMappings(lstrings);
        retval = true;
    }
    else if (rawdata) {
        DBG("Found localization for language: " << lang << "  region: " << region << " displang: " <<  displang <<  "  - resname: " << resname);
        LocalisedStrings * lstrings = new LocalisedStrings(String::createStringFromData(rawdata, retbytes), true);

        LocalisedStrings::setCurrentMappings(lstrings);
        retval = true;
    }
    else if (lang.startsWith("en")) {
        // special case, since untranslated is english
        retval = true;
    }
    else {
        DBG("Couldn't find mapping for lang: " << lang << "  region: " << region << " displang: " <<  displang <<  "  - resname: " << resname);
        retval = false;
    }

#ifdef JUCE_ANDROID
   // Font::setFallbackFontName("Droid Sans Fallback");
#endif

    if (retval) {
        mActiveLanguageCode = displang.toStdString();
    } else {
        mActiveLanguageCode = "en-us"; // indicates we are using english
    }

    DBG("Setup localization: active lang code: " << mActiveLanguageCode);
    
    return retval;
}


#pragma mark - ApplicationCommandTarget

enum
{
    MenuFileIndex = 0,
    MenuConnectIndex,
    MenuGroupIndex,
    MenuTransportIndex,
    MenuViewIndex,
    MenuHelpIndex
};


void CommsbusAudioProcessorEditor::getCommandInfo (CommandID cmdID, ApplicationCommandInfo& info) {
    bool useKeybindings = !processor.getDisableKeyboardShortcuts();
    String name;

    switch (cmdID) {
        case CommsbusCommands::MuteAllInput:
            info.setInfo (TRANS("Mute All Input"),
                          TRANS("Toggle Mute all input"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('m', ModifierKeys::noModifiers);
                info.addDefaultKeypress('m', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::MuteAllPeers:
            info.setInfo (TRANS("Mute All Users"),
                          TRANS("Toggle Mute all users"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('u', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::LoadSetupFile:
            info.setInfo (TRANS("Load Setup..."),
                          TRANS("Load Setup file"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('l', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::SaveSetupFile:
            info.setInfo (TRANS("Save Setup..."),
                          TRANS("Save Setup file"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('s', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::ChatToggle:
            info.setInfo (TRANS("Show/Hide Chat"),
                          TRANS("Show or hide chat area"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('y', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::ToggleAllMonitorDelay:
            info.setInfo (TRANS("Enable/Disable Monitor Delay"),
                          TRANS("Enable/Disable Monitor Delay"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('b', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::Connect:
            info.setInfo (TRANS("Connect"),
                          TRANS("Connect"),
                          TRANS("Popup"), 0);
            info.setActive(!currConnected || currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('n', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::Disconnect:
            info.setInfo (TRANS("Disconnect"),
                          TRANS("Disconnect"),
                          TRANS("Popup"), 0);
            info.setActive(currConnected && currGroup.isNotEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('d', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::ShowOptions:
            info.setInfo (TRANS("Show Options"),
                          TRANS("Show Options"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress (',', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::RecordToggle:
            info.setInfo (TRANS("Record"),
                          TRANS("Toggle Record"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress ('r', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::CheckForNewVersion:
            info.setInfo (TRANS("Check For New Version"),
                          TRANS("Check for New Version"),
                          TRANS("Popup"), 0);
            info.setActive(true);
            break;
        case CommsbusCommands::ToggleFullInfoView:
            info.setInfo(TRANS("Toggle Full Info View"),
                TRANS("Toggle Full Info View"),
                TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress('i', ModifierKeys::commandModifier);
            }
            break;
        case CommsbusCommands::ShowConnectMenu:
            info.setInfo(TRANS("Show Connect Menu"),
                TRANS("Show Connect Menu"),
                TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress('c', ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::ShowGroupMenu:
            info.setInfo(TRANS("Show Group Menu"),
                TRANS("Show Group Menu"),
                TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress('g', ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::ShowViewMenu:
            info.setInfo(TRANS("Show View Menu"),
                TRANS("Show View Menu"),
                TRANS("Popup"), 0);
            info.setActive(true);
            if (useKeybindings) {
                info.addDefaultKeypress('v', ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::CopyGroupLink:
#if JUCE_IOS || JUCE_ANDROID
            name = TRANS("Share Group Link");
#else
            name = TRANS("Copy Group Link");
#endif

            info.setInfo (name, name,
                          TRANS("Popup"), 0);
            info.setActive(currConnected && !currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('c', ModifierKeys::commandModifier | ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::GroupLatencyMatch:
            info.setInfo (TRANS("Group Latency Match..."),
                          TRANS("Group Latency Match..."),
                          TRANS("Popup"), 0);
            info.setActive(currConnected && !currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('l', ModifierKeys::commandModifier | ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::VDONinjaVideoLink:
            info.setInfo (TRANS("VDO.Ninja Video Link..."),
                          TRANS("VDO.Ninja Video Link..."),
                          TRANS("Popup"), 0);
            info.setActive(currConnected && !currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('v', ModifierKeys::commandModifier | ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::SuggestNewGroup:
            info.setInfo (TRANS("Suggest New Group..."),
                          TRANS("Suggest New Group..."),
                          TRANS("Popup"), 0);
            info.setActive(currConnected && !currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('s', ModifierKeys::commandModifier | ModifierKeys::altModifier);
            }
            break;
        case CommsbusCommands::ResetAllJitterBuffers:
            info.setInfo (TRANS("Reset All Jitter Buffers"),
                          TRANS("Reset All Jitter Buffers"),
                          TRANS("Popup"), 0);
            info.setActive(currConnected && !currGroup.isEmpty());
            if (useKeybindings) {
                info.addDefaultKeypress ('j', ModifierKeys::commandModifier);
            }
            break;

    }
}

void CommsbusAudioProcessorEditor::getAllCommands (Array<CommandID>& cmds) {
    cmds.add(CommsbusCommands::MuteAllInput);
    cmds.add(CommsbusCommands::MuteAllPeers);
    cmds.add(CommsbusCommands::Connect);
    cmds.add(CommsbusCommands::Disconnect);
    cmds.add(CommsbusCommands::ShowOptions);
    cmds.add(CommsbusCommands::RecordToggle);
    cmds.add(CommsbusCommands::CheckForNewVersion);
    cmds.add(CommsbusCommands::LoadSetupFile);
    cmds.add(CommsbusCommands::SaveSetupFile);
    cmds.add(CommsbusCommands::ChatToggle);
    cmds.add(CommsbusCommands::ShowViewMenu);
    cmds.add(CommsbusCommands::ShowGroupMenu);
    cmds.add(CommsbusCommands::ShowConnectMenu);
    cmds.add(CommsbusCommands::ToggleFullInfoView);
    cmds.add(CommsbusCommands::ToggleAllMonitorDelay);
    cmds.add(CommsbusCommands::CopyGroupLink);
    cmds.add(CommsbusCommands::GroupLatencyMatch);
    cmds.add(CommsbusCommands::VDONinjaVideoLink);
    cmds.add(CommsbusCommands::SuggestNewGroup);
    cmds.add(CommsbusCommands::ResetAllJitterBuffers);

}

bool CommsbusAudioProcessorEditor::perform (const InvocationInfo& info) {
    bool ret = true;
    
    switch (info.commandID) {
        case CommsbusCommands::MuteAllInput:
            DBG("got mute toggle!");
            mMainMuteButton->setToggleState(!mMainMuteButton->getToggleState(), sendNotification);
            break;
        case CommsbusCommands::MuteAllPeers:
            DBG("got mute peers toggle!");
            mMainRecvMuteButton->setToggleState(!mMainRecvMuteButton->getToggleState(), sendNotification);
            break;
        case CommsbusCommands::ToggleAllMonitorDelay:
            if (getInputChannelGroupsView()) {
                getInputChannelGroupsView()->toggleAllMonitorDelay();
            }
            break;
        case CommsbusCommands::ToggleFullInfoView:

            buttonClicked(processor.getPeerDisplayMode() == CommsbusAudioProcessor::PeerDisplayModeMinimal ?
                          mPeerLayoutFullButton.get() : mPeerLayoutMinimalButton.get());
            break;
        case CommsbusCommands::ShowConnectMenu:
            if (mMenuBar) {
                mMenuBar->showMenu(MenuConnectIndex);
            }
            break;
        case CommsbusCommands::ShowGroupMenu:
            if (mMenuBar) {
                mMenuBar->showMenu(MenuGroupIndex);
            }
            break;
        case CommsbusCommands::ShowViewMenu:
            if (mMenuBar) {
                mMenuBar->showMenu(MenuViewIndex);
            }
            break;
        case CommsbusCommands::LoadSetupFile:
            DBG("got load setup file!");
            showLoadSettingsPreset();

            break;
        case CommsbusCommands::ChatToggle:
            showChatPanel(!mChatView->isVisible());
            resized();
            break;
        case CommsbusCommands::SaveSetupFile:
            DBG("got save setup file!");
            showSaveSettingsPreset();

            break;
        case CommsbusCommands::Connect:
            DBG("got connect!");
            if (!currConnected || currGroup.isEmpty()) {
                buttonClicked(mConnectButton.get());
            }
            break;
        case CommsbusCommands::Disconnect:
            DBG("got disconnect!");
            
            if (currConnected && currGroup.isNotEmpty()) {
                buttonClicked(mConnectButton.get());
            }

            break;
        case CommsbusCommands::ShowOptions:
            DBG("got show options!");
            buttonClicked(mSettingsButton.get());

            break;
        case CommsbusCommands::RecordToggle:
            DBG("got record toggle!");
            buttonClicked(mRecordingButton.get());

            break;
        case CommsbusCommands::CheckForNewVersion:   
            LatestVersionCheckerAndUpdater::getInstance()->checkForNewVersion (true); 
            break;

        case CommsbusCommands::CopyGroupLink:
            copyGroupLink();
            break;
        case CommsbusCommands::GroupLatencyMatch:
            showLatencyMatchView(true);
            break;
        case CommsbusCommands::VDONinjaVideoLink:
            showVDONinjaView(true, mVideoButton->isShowing());
            break;
        case CommsbusCommands::SuggestNewGroup:
            showSuggestGroupView(true);
            break;
        case CommsbusCommands::ResetAllJitterBuffers:
            resetJitterBufferForAll();
            break;

        default:
            ret = false;
    }
    
    return ret;
}

void CommsbusAudioProcessorEditor::populateRecentSetupsMenu(PopupMenu & popup)
{
    popup.clear();

    auto callback = [this](File file) {
        this->loadSettingsFromFile(file);
    };

    if (getRecentSetupFiles && getRecentSetupFiles()) {
        auto recents = getRecentSetupFiles();

        // load the recents in reverse order

        for (int i=recents->size()-1; i >= 0; --i) {
            const auto & fname = recents->getReference(i);
            File file(fname);
            if (file.existsAsFile()) {
                popup.addItem( file.getFileNameWithoutExtension() , [file, callback]() { callback(file); });
            }
            else {
                // remove it! (we can do this because we are traversing it from the end)
                recents->remove(i);
            }
        }

    }

}



#pragma MenuBarModel



StringArray CommsbusAudioProcessorEditor::CommsbusMenuBarModel::getMenuBarNames()
{
    return StringArray(TRANS("File"),
                       TRANS("Connect"),
                       TRANS("Group"),
                       TRANS("Transport"),
                       TRANS("View")
                       );
    //TRANS("Help"));
}

PopupMenu CommsbusAudioProcessorEditor::CommsbusMenuBarModel::getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/)
{
    PopupMenu retval;
    PopupMenu recents;

    switch (topLevelMenuIndex) {
        case MenuFileIndex:
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::LoadSetupFile);
            parent.populateRecentSetupsMenu(recents);
            retval.addSubMenu(TRANS("Load Recent Setup"), recents, recents.getNumItems() > 0);
            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::SaveSetupFile);

            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::CheckForNewVersion);

#if (JUCE_WINDOWS || JUCE_LINUX)
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::ShowOptions);
            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, StandardApplicationCommandIDs::quit);
#endif
            break;
        case MenuConnectIndex:
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::Connect);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::Disconnect);
            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::MuteAllInput);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::MuteAllPeers);
            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::ToggleAllMonitorDelay);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::ResetAllJitterBuffers);
            break;
        case MenuGroupIndex:
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::CopyGroupLink);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::GroupLatencyMatch);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::VDONinjaVideoLink);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::SuggestNewGroup);
            break;
        case MenuTransportIndex:
            retval.addSeparator();
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::RecordToggle);
            break;
        case MenuViewIndex:
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::ChatToggle);
            retval.addCommandItem (&parent.commandManager, CommsbusCommands::ToggleFullInfoView);
            break;

        case MenuHelpIndex:
            break;
    }
    
    return retval;
}

void CommsbusAudioProcessorEditor::CommsbusMenuBarModel::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
#if JUCE_MAC
    if (topLevelMenuIndex == -1) {
        switch (menuItemID) {
            case 1:
                // about
                break;
            case 2:
                parent.commandManager.invokeDirectly(CommsbusCommands::ShowOptions, true);
                break;
        }
    }
#endif
}

