// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2021 Jesse Chappell

#include "ConnectView.h"

#include "RandomSentenceGenerator.h"

using namespace SonoAudio;

class CommsbusConnectTabbedComponent : public TabbedComponent
{
public:
    CommsbusConnectTabbedComponent(TabbedButtonBar::Orientation orientation, ConnectView & editor_) : TabbedComponent(orientation), editor(editor_) {

    }

    void currentTabChanged (int newCurrentTabIndex, const String& newCurrentTabName) override {

        editor.connectTabChanged(newCurrentTabIndex);
    }

protected:
    ConnectView & editor;

};


enum {
    nameTextColourId = 0x1002830,
    selectedColourId = 0x1002840,
    separatorColourId = 0x1002850,
};


ConnectView::ConnectView(CommsbusAudioProcessor& proc, AooServerConnectionInfo & info)
: Component(), processor(proc), currConnectionInfo(info),
recentsListModel(this),
recentsGroupFont (17.0 * SonoLookAndFeel::getFontScale(), Font::bold), recentsNameFont(15 * SonoLookAndFeel::getFontScale(), Font::plain), recentsInfoFont(13 * SonoLookAndFeel::getFontScale(), Font::plain)
{
    setColour (nameTextColourId, Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.9f));
    setColour (selectedColourId, Colour::fromFloatRGBA(0.0f, 0.4f, 0.8f, 0.5f));
    setColour (separatorColourId, Colour::fromFloatRGBA(0.3f, 0.3f, 0.3f, 0.3f));

    mConnectTab = std::make_unique<CommsbusConnectTabbedComponent>(TabbedButtonBar::Orientation::TabsAtTop, *this);
    mConnectTab->setOutline(0);
    mConnectTab->setTabBarDepth(36);
    mConnectTab->getTabbedButtonBar().setMinimumTabScaleFactor(0.1f);
    mConnectTab->getTabbedButtonBar().setColour(TabbedButtonBar::frontTextColourId, Colour::fromFloatRGBA(0.4, 0.8, 1.0, 1.0));
    mConnectTab->getTabbedButtonBar().setColour(TabbedButtonBar::frontOutlineColourId, Colour::fromFloatRGBA(0.4, 0.8, 1.0, 0.5));

    mDirectConnectContainer = std::make_unique<Component>();
    mServerConnectContainer = std::make_unique<Component>();
    mServerConnectViewport = std::make_unique<Viewport>();
    mRecentsContainer = std::make_unique<Component>();

    mServerConnectViewport->setViewedComponent(mServerConnectContainer.get());

    mDirectConnectViewport = std::make_unique<Viewport>();
    mDirectConnectViewport->setViewedComponent(mDirectConnectContainer.get(), false);



    mRecentsGroup = std::make_unique<GroupComponent>("", TRANS("RECENTS"));
    mRecentsGroup->setColour(GroupComponent::textColourId, Colour::fromFloatRGBA(0.8, 0.8, 0.8, 0.8));
    mRecentsGroup->setColour(GroupComponent::outlineColourId, Colour::fromFloatRGBA(0.8, 0.8, 0.8, 0.1));
    mRecentsGroup->setTextLabelPosition(Justification::centred);

    // Commsbus leads with PRIVATE GROUP -- both ends point at one connection
    // server (which every Commsbus instance can be, see mServerHostHintLabel) and
    // find each other by group name, which survives an address change at either
    // end. DIRECT is second, for when the addresses really are fixed. Upstream
    // SonoBus shipped DIRECT commented out entirely.
    mConnectTab->addTab(TRANS("PRIVATE GROUP"), Colour::fromFloatRGBA(0.1, 0.1, 0.1, 1.0), mServerConnectViewport.get(), false);
    mConnectTab->addTab(TRANS("DIRECT"), Colour::fromFloatRGBA(0.1, 0.1, 0.1, 1.0), mDirectConnectViewport.get(), false);
    mConnectTab->addTab(TRANS("RECENTS"), Colour::fromFloatRGBA(0.1, 0.1, 0.1, 1.0), mRecentsContainer.get(), false);




    mLocalAddressLabel = std::make_unique<TextEditor>("localaddr");
    mLocalAddressLabel->setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
    mLocalAddressLabel->setTitle(TRANS("Local Address:Port"));
    mLocalAddressLabel->setReadOnly(true);
    mLocalAddressLabel->setWantsKeyboardFocus(true);
    //mLocalAddressLabel->setJustificationType(Justification::centredLeft);
    mLocalAddressStaticLabel = std::make_unique<Label>("localaddrst", TRANS("Local Address:"));
    mLocalAddressStaticLabel->setJustificationType(Justification::centredRight);


    mRemoteAddressStaticLabel = std::make_unique<Label>("remaddrst", TRANS("Host: "));
    mRemoteAddressStaticLabel->setJustificationType(Justification::centredRight);

    mDirectConnectDescriptionLabel = std::make_unique<Label>("dirconndesc", TRANS("Connect straight to another Commsbus machine using the address it advertises below. No connection server is involved. Peers added this way are remembered and reconnected automatically."));
    mDirectConnectDescriptionLabel->setJustificationType(Justification::topLeft);

    mAddRemoteHostEditor = std::make_unique<TextEditor>("remaddredit");
    mAddRemoteHostEditor->setTitle(TRANS("Remote Host:Port"));
    mAddRemoteHostEditor->setFont(Font(16 * SonoLookAndFeel::getFontScale()));
    mAddRemoteHostEditor->setText("", false); // 100.36.128.246:11000
    mAddRemoteHostEditor->setTextToShowWhenEmpty(TRANS("IPaddress:port"), Colour(0x44ffffff));


    mConnectTitle = std::make_unique<Label>("conntime", TRANS("Connect"));
    mConnectTitle->setJustificationType(Justification::centred);
    mConnectTitle->setFont(Font(20, Font::bold));
    mConnectTitle->setColour(Label::textColourId, Colour(0x66ffffff));

    mConnectComponentBg = std::make_unique<DrawableRectangle>();
    mConnectComponentBg->setFill (Colour::fromFloatRGBA(0.1, 0.1, 0.1, 1.0));

    mConnectCloseButton = std::make_unique<SonoDrawableButton>("x", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> ximg(Drawable::createFromImageData(BinaryData::x_icon_svg, BinaryData::x_icon_svgSize));
    mConnectCloseButton->setTitle(TRANS("Close"));
    mConnectCloseButton->setImages(ximg.get());
    mConnectCloseButton->addListener(this);
    mConnectCloseButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);

    mConnectMenuButton = std::make_unique<SonoDrawableButton>("menu", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> dotimg(Drawable::createFromImageData(BinaryData::dots_icon_png, BinaryData::dots_icon_pngSize));
    mConnectMenuButton->setTitle(TRANS("Menu"));
    mConnectMenuButton->setImages(dotimg.get());
    mConnectMenuButton->addListener(this);
    mConnectMenuButton->setColour(DrawableButton::backgroundColourId, Colours::transparentBlack);
    mConnectMenuButton->setAlpha(0.7f);

    mDirectConnectButton = std::make_unique<TextButton>("directconnect");
    mDirectConnectButton->setButtonText(TRANS("Direct Connect"));
    mDirectConnectButton->addListener(this);
    mDirectConnectButton->setColour(TextButton::buttonColourId, Colour::fromFloatRGBA(0.1, 0.4, 0.6, 0.6));
    mDirectConnectButton->setWantsKeyboardFocus(true);


    mServerConnectButton = std::make_unique<TextButton>("serverconnect");
    mServerConnectButton->setButtonText(TRANS("Connect to Group"));
    mServerConnectButton->addListener(this);
    mServerConnectButton->setColour(TextButton::buttonColourId, Colour::fromFloatRGBA(0.1, 0.4, 0.6, 0.6));
    mServerConnectButton->setWantsKeyboardFocus(true);

    mServerHostEditor = std::make_unique<TextEditor>("srvaddredit");
    mServerHostEditor->setTitle(TRANS("Connection Server"));
    mServerHostEditor->setFont(Font(14 * SonoLookAndFeel::getFontScale()));
    configEditor(mServerHostEditor.get());

    mServerUsernameEditor = std::make_unique<TextEditor>("srvaddredit");
    mServerUsernameEditor->setTitle(TRANS("Your Displayed Name:"));
    mServerUsernameEditor->setFont(Font(16 * SonoLookAndFeel::getFontScale()));
    mServerUsernameEditor->setText(processor.getCurrentUsername(), false);
    configEditor(mServerUsernameEditor.get());

    mServerUserPasswordEditor = std::make_unique<TextEditor>("userpass"); // 0x25cf
    mServerUserPasswordEditor->setFont(Font(14 * SonoLookAndFeel::getFontScale()));
    mServerUserPasswordEditor->setTextToShowWhenEmpty(TRANS("optional"), Colour(0x44ffffff));
    configEditor(mServerUserPasswordEditor.get());


    mServerUserStaticLabel = std::make_unique<Label>("serveruserst", TRANS("Your Displayed Name:"));
    configServerLabel(mServerUserStaticLabel.get());
    mServerUserStaticLabel->setMinimumHorizontalScale(0.7);

    mServerUserPassStaticLabel = std::make_unique<Label>("serveruserpassst", TRANS("Password:"));
    configServerLabel(mServerUserPassStaticLabel.get());

    mServerGroupStaticLabel = std::make_unique<Label>("servergroupst", TRANS("Group Name:"));
    configServerLabel(mServerGroupStaticLabel.get());
    mServerGroupStaticLabel->setMinimumHorizontalScale(0.8);

    mServerGroupPassStaticLabel = std::make_unique<Label>("servergrouppassst", TRANS("Password:"));
    configServerLabel(mServerGroupPassStaticLabel.get());

    mServerHostStaticLabel = std::make_unique<Label>("serverhostst", TRANS("Connection Server:"));
    configServerLabel(mServerHostStaticLabel.get());

    // Every Commsbus instance already runs a connection server of its own, so a
    // pair of them needs nothing on the internet -- point both ends at one of the
    // two machines. Worth saying out loud; it is not discoverable otherwise.
    mServerHostHintLabel = std::make_unique<Label>("serverhosthint",
        TRANS("Every Commsbus runs its own server on port 10999 -- to stay off the internet, point both ends at one machine, e.g. 192.168.1.50:10999"));
    mServerHostHintLabel->setJustificationType(Justification::topLeft);
    mServerHostHintLabel->setFont(12);
    mServerHostHintLabel->setColour(Label::textColourId, Colour(0x99aaaaaa));
    mServerHostHintLabel->setMinimumHorizontalScale(0.75);


    mServerGroupEditor = std::make_unique<TextEditor>("groupedit");
    mServerGroupEditor->setTitle(TRANS("Group Name:"));
    mServerGroupEditor->setFont(Font(16 * SonoLookAndFeel::getFontScale()));
    mServerGroupEditor->setText(currConnectionInfo.groupName, false);
    configEditor(mServerGroupEditor.get());

    mServerGroupPasswordEditor = std::make_unique<TextEditor>("grouppass"); // 0x25cf
    mServerGroupPasswordEditor->setTitle(TRANS("Optional Group Password"));
    mServerGroupPasswordEditor->setFont(Font(14 * SonoLookAndFeel::getFontScale()));
    mServerGroupPasswordEditor->setTextToShowWhenEmpty(TRANS("optional"), Colour(0x44ffffff));
    mServerGroupPasswordEditor->setText(currConnectionInfo.groupPassword, false);
    configEditor(mServerGroupPasswordEditor.get());

    mServerGroupPasswordShowButton = std::make_unique<SonoDrawableButton>("passshow", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> eyeimg(Drawable::createFromImageData(BinaryData::eye_svg, BinaryData::eye_svgSize));
    std::unique_ptr<Drawable> eyeoffimg(Drawable::createFromImageData(BinaryData::eyeoff_svg, BinaryData::eyeoff_svgSize));
    mServerGroupPasswordShowButton->setTitle(TRANS("Show Password"));
    mServerGroupPasswordShowButton->setImages(eyeoffimg.get(), {}, {}, {}, eyeimg.get());
    mServerGroupPasswordShowButton->addListener(this);
    mServerGroupPasswordShowButton->setTooltip(TRANS("Show password"));
    mServerGroupPasswordShowButton->setColour(SonoDrawableButton::backgroundOnColourId, Colours::transparentBlack);

    
    mServerGroupRandomButton = std::make_unique<SonoDrawableButton>("randgroup", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> randimg(Drawable::createFromImageData(BinaryData::dice_icon_128_png, BinaryData::dice_icon_128_pngSize));
    mServerGroupRandomButton->setTitle(TRANS("Randomize Group Name"));
    mServerGroupRandomButton->setImages(randimg.get());
    mServerGroupRandomButton->addListener(this);
    mServerGroupRandomButton->setTooltip(TRANS("Generate a random group name"));

    mServerCopyButton = std::make_unique<SonoDrawableButton>("copy", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> copyimg(Drawable::createFromImageData(BinaryData::copy_icon_svg, BinaryData::copy_icon_svgSize));
    mServerCopyButton->setTitle(TRANS("Copy Share Link"));
    mServerCopyButton->setImages(copyimg.get());
    mServerCopyButton->addListener(this);
    mServerCopyButton->setTooltip(TRANS("Copy connection information to the clipboard to share"));

    mServerPasteButton = std::make_unique<SonoDrawableButton>("paste", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> pasteimg(Drawable::createFromImageData(BinaryData::paste_icon_svg, BinaryData::paste_icon_svgSize));
    mServerPasteButton->setTitle(TRANS("Paste Share Link"));
    mServerPasteButton->setImages(pasteimg.get());
    mServerPasteButton->addListener(this);
    mServerPasteButton->setTooltip(TRANS("Paste connection information from the clipboard"));

    mServerShareButton = std::make_unique<SonoDrawableButton>("share", DrawableButton::ButtonStyle::ImageFitted);
    std::unique_ptr<Drawable> shareimg(Drawable::createFromImageData(BinaryData::copy_icon_svg, BinaryData::copy_icon_svgSize));
    mServerShareButton->setImages(shareimg.get());
    mServerShareButton->addListener(this);


    mServerStatusLabel = std::make_unique<Label>("servstat", "");
    mServerStatusLabel->setJustificationType(Justification::centred);
    mServerStatusLabel->setFont(16);
    mServerStatusLabel->setColour(Label::textColourId, Colour(0x99ffaaaa));
    mServerStatusLabel->setMinimumHorizontalScale(0.75);

    mServerInfoLabel = std::make_unique<Label>("servinfo", "");
    mServerInfoLabel->setJustificationType(Justification::centred);
    mServerInfoLabel->setFont(16);
    mServerInfoLabel->setColour(Label::textColourId, Colour(0x99dddddd));
    mServerInfoLabel->setMinimumHorizontalScale(0.85);

    String servaudioinfo = TRANS("The connection server is only used to help users find each other, no audio passes through it. All audio is sent directly between users (peer to peer).");
    mServerAudioInfoLabel = std::make_unique<Label>("servaudioinfo", servaudioinfo);
    mServerAudioInfoLabel->setJustificationType(Justification::centredTop);
    mServerAudioInfoLabel->setFont(14);
    mServerAudioInfoLabel->setColour(Label::textColourId, Colour(0x99aaaaaa));
    mServerAudioInfoLabel->setMinimumHorizontalScale(0.75);

    mMainStatusLabel = std::make_unique<Label>("mainstat", "");
    mMainStatusLabel->setJustificationType(Justification::centredRight);
    mMainStatusLabel->setFont(13);
    mMainStatusLabel->setColour(Label::textColourId, Colour(0x66ffffff));


    mClearRecentsButton = std::make_unique<SonoTextButton>("clearrecent");
    mClearRecentsButton->setButtonText(TRANS("Clear All"));
    mClearRecentsButton->addListener(this);
    mClearRecentsButton->setTextJustification(Justification::centred);

    mRecentsListBox = std::make_unique<ListBox>("recentslist");
    mRecentsListBox->setColour (ListBox::outlineColourId, Colour::fromFloatRGBA(0.7, 0.7, 0.7, 0.0));
    mRecentsListBox->setColour (ListBox::backgroundColourId, Colour::fromFloatRGBA(0.1, 0.12, 0.1, 0.0f));
    mRecentsListBox->setColour (ListBox::textColourId, Colours::whitesmoke.withAlpha(0.8f));
    mRecentsListBox->setTitle(TRANS("Recents List"));
    mRecentsListBox->setOutlineThickness (1);
#if JUCE_IOS || JUCE_ANDROID
    mRecentsListBox->getViewport()->setScrollOnDragEnabled(true);
#endif
    mRecentsListBox->getViewport()->setScrollBarsShown(true, false);
    mRecentsListBox->setMultipleSelectionEnabled (false);
    mRecentsListBox->setRowHeight(50);
    mRecentsListBox->setModel (&recentsListModel);
    mRecentsListBox->setRowSelectedOnMouseDown(true);
    mRecentsListBox->setRowClickedOnMouseDown(false);


    // parenting
    mDirectConnectContainer->addAndMakeVisible(mDirectConnectButton.get());
    mDirectConnectContainer->addAndMakeVisible(mAddRemoteHostEditor.get());
    mDirectConnectContainer->addAndMakeVisible(mRemoteAddressStaticLabel.get());
    mDirectConnectContainer->addAndMakeVisible(mDirectConnectDescriptionLabel.get());
    mDirectConnectContainer->addAndMakeVisible(mLocalAddressLabel.get());
    mDirectConnectContainer->addAndMakeVisible(mLocalAddressStaticLabel.get());

    mServerConnectContainer->addAndMakeVisible(mServerConnectButton.get());
    mServerConnectContainer->addAndMakeVisible(mServerHostEditor.get());
    mServerConnectContainer->addAndMakeVisible(mServerUsernameEditor.get());
    mServerConnectContainer->addAndMakeVisible(mServerUserStaticLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerGroupEditor.get());
    mServerConnectContainer->addAndMakeVisible(mServerGroupRandomButton.get());
    mServerConnectContainer->addAndMakeVisible(mServerGroupPasswordShowButton.get());
#if ! (JUCE_IOS || JUCE_ANDROID)
    mServerConnectContainer->addAndMakeVisible(mServerPasteButton.get());
    mServerConnectContainer->addAndMakeVisible(mServerCopyButton.get());
#else
    mServerConnectContainer->addAndMakeVisible(mServerShareButton.get());
#endif
    mServerConnectContainer->addAndMakeVisible(mServerGroupStaticLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerHostStaticLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerHostHintLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerGroupPassStaticLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerGroupPasswordEditor.get());
    mServerConnectContainer->addAndMakeVisible(mServerStatusLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerInfoLabel.get());
    mServerConnectContainer->addAndMakeVisible(mServerAudioInfoLabel.get());

    mRecentsContainer->addAndMakeVisible(mRecentsListBox.get());
    mRecentsContainer->addAndMakeVisible(mClearRecentsButton.get());

    addAndMakeVisible(mConnectComponentBg.get());
    addAndMakeVisible(mConnectTab.get());
    addAndMakeVisible(mConnectTitle.get());
    addAndMakeVisible(mConnectCloseButton.get());
    addAndMakeVisible(mConnectMenuButton.get());


    std::istringstream gramstream(std::string(BinaryData::wordmaker_g, BinaryData::wordmaker_gSize));
    mRandomSentence = std::make_unique<RandomSentenceGenerator>(gramstream);
    mRandomSentence->capEveryWord = true;

    setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    mConnectTab->setFocusContainerType(FocusContainerType::none);
    mConnectTab->getTabbedButtonBar().setFocusContainerType(FocusContainerType::none);
    mConnectTab->getTabbedButtonBar().setWantsKeyboardFocus(true);
    mConnectTab->setWantsKeyboardFocus(true);
    for (int i=0; i < mConnectTab->getTabbedButtonBar().getNumTabs(); ++i) {
        if (auto tabbut = mConnectTab->getTabbedButtonBar().getTabButton(i)) {
            tabbut->setWantsKeyboardFocus(true);
            tabbut->setRadioGroupId(2);
        }
    }


    updateLayout();
}

ConnectView::~ConnectView() {}

juce::Rectangle<int> ConnectView::getMinimumContentBounds() const {
    int defWidth = 200;
    int defHeight = 100;
    return Rectangle<int>(0,0,defWidth,defHeight);
}

void ConnectView::grabInitialFocus()
{
    if (auto * butt = mConnectTab->getTabbedButtonBar().getTabButton(mConnectTab->getCurrentTabIndex())) {
        butt->setWantsKeyboardFocus(true);
        if (butt->isShowing())
            butt->grabKeyboardFocus();
    }
}

void ConnectView::escapePressed()
{
    if (!mServerGroupEditor->hasKeyboardFocus(false)
        && !mServerHostEditor->hasKeyboardFocus(false)
        && !mServerUsernameEditor->hasKeyboardFocus(false)
        && !mServerUserPasswordEditor->hasKeyboardFocus(false)
        && !mServerGroupPasswordEditor->hasKeyboardFocus(false)
        )
    {
        // close us down
        setVisible(false);
    }
}



void ConnectView::timerCallback(int timerid)
{

}

void ConnectView::configServerLabel(Label *label)
{
    label->setFont(14);
    label->setColour(Label::textColourId, Colour(0x90eeeeee));
    label->setJustificationType(Justification::centredRight);
}

void ConnectView::configEditor(TextEditor *editor, bool passwd)
{
    editor->addListener(this);
    if (passwd)  {
        editor->setIndents(8, 6);
    } else {
        editor->setIndents(8, 8);
    }
}

void ConnectView::updateState()
{
    String locstr;
    locstr << processor.getLocalIPAddress().toString() << ":" << processor.getUdpLocalPort();
    mLocalAddressLabel->setText(locstr, dontSendNotification);

    resetPrivateGroupLabels();
    updateServerFieldsFromConnectionInfo();

    updateRecents();

    if (firstTimeConnectShow) {
        // Commsbus opens on PRIVATE GROUP.
        showPrivateGroupTab();
        firstTimeConnectShow = false;
    }
}


void ConnectView::updateLayout()
{
    int minKnobWidth = 50;
    int minSliderWidth = 50;
    int minPannerWidth = 40;
    int maxPannerWidth = 100;
    int minitemheight = 36;
    int knobitemheight = 80;
    int minpassheight = 30;
    int setitemheight = 36;
    int minButtonWidth = 90;
    int sliderheight = 44;
    int inmeterwidth = 22 ;
    int outmeterwidth = 22 ;
    int servLabelWidth = 82;
    int iconheight = 24;
    int iconwidth = iconheight;
    int knoblabelheight = 18;
    int panbuttwidth = 26;

#if JUCE_IOS || JUCE_ANDROID
    // make the button heights a bit more for touchscreen purposes
    minitemheight = 44;
    knobitemheight = 90;
    minpassheight = 38;
    panbuttwidth = 32;
#endif

    localAddressBox.items.clear();
    localAddressBox.flexDirection = FlexBox::Direction::row;
    localAddressBox.items.add(FlexItem(60, 18, *mLocalAddressStaticLabel).withMargin(2).withFlex(1).withMaxWidth(110));
    localAddressBox.items.add(FlexItem(100, minitemheight, *mLocalAddressLabel).withMargin(2).withFlex(1).withMaxWidth(160));


    addressBox.items.clear();
    addressBox.flexDirection = FlexBox::Direction::row;
    addressBox.items.add(FlexItem(servLabelWidth, 18, *mRemoteAddressStaticLabel).withMargin(2).withFlex(0.25));
    addressBox.items.add(FlexItem(172, minitemheight, *mAddRemoteHostEditor).withMargin(2).withFlex(1));


    remoteBox.items.clear();
    remoteBox.flexDirection = FlexBox::Direction::column;
    remoteBox.items.add(FlexItem(5, 8).withFlex(0));
    remoteBox.items.add(FlexItem(180, minitemheight, addressBox).withMargin(2).withFlex(0));
    remoteBox.items.add(FlexItem(minButtonWidth, minitemheight, *mDirectConnectButton).withMargin(8).withFlex(0).withMinWidth(100));
    remoteBox.items.add(FlexItem(180, 2*minitemheight, *mDirectConnectDescriptionLabel).withMargin(2).withFlex(1).withMaxHeight(150));
    remoteBox.items.add(FlexItem(60, minitemheight, localAddressBox).withMargin(2).withFlex(0));
    remoteBox.items.add(FlexItem(10, 0).withFlex(1));

    servHostHintBox.items.clear();
    servHostHintBox.flexDirection = FlexBox::Direction::row;
    servHostHintBox.items.add(FlexItem(4, 4).withFlex(0));
    servHostHintBox.items.add(FlexItem(100, 46, *mServerHostHintLabel).withMargin(2).withFlex(1));

    servAddressBox.items.clear();
    servAddressBox.flexDirection = FlexBox::Direction::row;
    servAddressBox.items.add(FlexItem(servLabelWidth, minitemheight, *mServerHostStaticLabel).withMargin(2).withFlex(1));
    servAddressBox.items.add(FlexItem(172, minpassheight, *mServerHostEditor).withMargin(2).withFlex(1));

    servUserBox.items.clear();
    servUserBox.flexDirection = FlexBox::Direction::row;
    servUserBox.items.add(FlexItem(servLabelWidth, minitemheight, *mServerUserStaticLabel).withMargin(2).withFlex(0.25));
    servUserBox.items.add(FlexItem(120, minitemheight, *mServerUsernameEditor).withMargin(2).withFlex(1));

    servUserPassBox.items.clear();
    servUserPassBox.flexDirection = FlexBox::Direction::row;
    servUserPassBox.items.add(FlexItem(servLabelWidth, minpassheight, *mServerUserPassStaticLabel).withMargin(2).withFlex(1));
    servUserPassBox.items.add(FlexItem(90, minpassheight, *mServerUserPasswordEditor).withMargin(2).withFlex(1));

    servGroupBox.items.clear();
    servGroupBox.flexDirection = FlexBox::Direction::row;
    servGroupBox.items.add(FlexItem(servLabelWidth, minitemheight, *mServerGroupStaticLabel).withMargin(2).withFlex(0.25));
    servGroupBox.items.add(FlexItem(120, minitemheight, *mServerGroupEditor).withMargin(2).withFlex(1));
    servGroupBox.items.add(FlexItem(minPannerWidth, minitemheight, *mServerGroupRandomButton).withMargin(2).withFlex(0));

    servGroupPassBox.items.clear();
    servGroupPassBox.flexDirection = FlexBox::Direction::row;
    servGroupPassBox.items.add(FlexItem(servLabelWidth, minpassheight, *mServerGroupPassStaticLabel).withMargin(2).withFlex(1));
    servGroupPassBox.items.add(FlexItem(90, minpassheight, *mServerGroupPasswordEditor).withMargin(2).withFlex(1));
    servGroupPassBox.items.add(FlexItem(minPannerWidth, minitemheight, *mServerGroupPasswordShowButton).withMargin(2).withFlex(0));

    servStatusBox.items.clear();
    servStatusBox.flexDirection = FlexBox::Direction::row;
#if !(JUCE_IOS || JUCE_ANDROID)
    servStatusBox.items.add(FlexItem(minPannerWidth, minitemheight, *mServerPasteButton).withMargin(2).withFlex(0).withMaxHeight(minitemheight));
#endif
    servStatusBox.items.add(FlexItem(servLabelWidth, minpassheight, *mServerStatusLabel).withMargin(2).withFlex(1));
#if !(JUCE_IOS || JUCE_ANDROID)
    servStatusBox.items.add(FlexItem(minPannerWidth, minitemheight, *mServerCopyButton).withMargin(2).withFlex(0).withMaxHeight(minitemheight));
#else
    servStatusBox.items.add(FlexItem(minPannerWidth, minitemheight, *mServerShareButton).withMargin(2).withFlex(0).withMaxHeight(minitemheight));
#endif

    servButtonBox.items.clear();
    servButtonBox.flexDirection = FlexBox::Direction::row;
    servButtonBox.items.add(FlexItem(5, 3).withFlex(0.1));
    servButtonBox.items.add(FlexItem(minButtonWidth, minitemheight, *mServerConnectButton).withMargin(2).withFlex(1).withMaxWidth(300));
    servButtonBox.items.add(FlexItem(5, 3).withFlex(0.1));



    int maxservboxwidth = 400;

    serverBox.items.clear();
    serverBox.flexDirection = FlexBox::Direction::column;
    serverBox.items.add(FlexItem(5, 3).withFlex(0));
    serverBox.items.add(FlexItem(80, minpassheight, servStatusBox).withMargin(2).withFlex(1).withMaxWidth(maxservboxwidth).withMaxHeight(60));
    serverBox.items.add(FlexItem(5, 4).withFlex(0));
    serverBox.items.add(FlexItem(180, minitemheight, servGroupBox).withMargin(2).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(180, minpassheight, servGroupPassBox).withMargin(2).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(5, 6).withFlex(0));
    serverBox.items.add(FlexItem(180, minitemheight, servUserBox).withMargin(2).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(5, 6).withFlex(1).withMaxHeight(14));
    serverBox.items.add(FlexItem(minButtonWidth, minitemheight, servButtonBox).withMargin(2).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(5, 10).withFlex(1));
    serverBox.items.add(FlexItem(100, minitemheight, servAddressBox).withMargin(2).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(100, 46, servHostHintBox).withMargin(0).withFlex(0).withMaxWidth(maxservboxwidth));
    serverBox.items.add(FlexItem(80, minpassheight, *mServerAudioInfoLabel).withMargin(2).withFlex(1).withMaxWidth(maxservboxwidth).withMaxHeight(60));
    serverBox.items.add(FlexItem(5, 8).withFlex(0));

    minHeight = 4*minitemheight + 3*minpassheight + 58 + 46; // + server hint

    // recents
    clearRecentsBox.items.clear();
    clearRecentsBox.flexDirection = FlexBox::Direction::row;
    clearRecentsBox.items.add(FlexItem(10, 5).withMargin(0).withFlex(1));
    clearRecentsBox.items.add(FlexItem(minButtonWidth, minitemheight, *mClearRecentsButton).withMargin(0).withFlex(0));
    clearRecentsBox.items.add(FlexItem(10, 5).withMargin(0).withFlex(1));

    recentsBox.items.clear();
    recentsBox.flexDirection = FlexBox::Direction::column;
    recentsBox.items.add(FlexItem(minButtonWidth, minitemheight, *mRecentsListBox).withMargin(6).withFlex(1));
    recentsBox.items.add(FlexItem(minButtonWidth, minitemheight, clearRecentsBox).withMargin(1).withFlex(0));
    recentsBox.items.add(FlexItem(10, 5).withMargin(0).withFlex(0));

    // main layout
    connectTitleBox.items.clear();
    connectTitleBox.flexDirection = FlexBox::Direction::row;
    connectTitleBox.items.add(FlexItem(50, minitemheight, *mConnectCloseButton).withMargin(2));
    connectTitleBox.items.add(FlexItem(80, minitemheight, *mConnectTitle).withMargin(2).withFlex(1));
    connectTitleBox.items.add(FlexItem(50, minitemheight, *mConnectMenuButton).withMargin(2));

    connectHorizBox.items.clear();
    connectHorizBox.flexDirection = FlexBox::Direction::row;
    connectHorizBox.items.add(FlexItem(100, 100, *mConnectTab).withMargin(3).withFlex(1));
    if (mConnectTab->getTabNames().indexOf(TRANS("RECENTS")) < 0) {
        connectHorizBox.items.add(FlexItem(335, 100, *mRecentsGroup).withMargin(3).withFlex(0));
    }

    mainBox.items.clear();
    mainBox.flexDirection = FlexBox::Direction::column;
    mainBox.items.add(FlexItem(100, minitemheight, connectTitleBox).withMargin(3).withFlex(0));
    mainBox.items.add(FlexItem(100, 100, connectHorizBox).withMargin(3).withFlex(1));


}

void ConnectView::resized()  {



    mConnectComponentBg->setRectangle (getLocalBounds().toFloat());

    // Wide layouts pull RECENTS out of the tab strip into its own panel. This
    // tracks the tab by name rather than by a fixed index: DIRECT now occupies
    // index 0, so the old hard-coded removeTab(0)/moveTab(2,0) would have moved
    // the wrong tab.
    const int recentsTabIndex = mConnectTab->getTabNames().indexOf(TRANS("RECENTS"));

    if (getWidth() > 700) {
        if (recentsTabIndex >= 0) {
            // move recents to main connect component, out of tab
            int adjcurrtab = mConnectTab->getCurrentTabIndex();
            if (adjcurrtab >= recentsTabIndex) {
                adjcurrtab = jmax(0, adjcurrtab - 1);
            }

            mConnectTab->removeTab(recentsTabIndex);
            mRecentsGroup->addAndMakeVisible(mRecentsContainer.get());
            addAndMakeVisible(mRecentsGroup.get());

            mConnectTab->setCurrentTabIndex(adjcurrtab);
            updateLayout();
        }
    } else {
        if (recentsTabIndex < 0) {
            int tabsel = mConnectTab->getCurrentTabIndex();
            mRecentsGroup->removeChildComponent(mRecentsContainer.get());
            mRecentsGroup->setVisible(false);

            mConnectTab->addTab(TRANS("RECENTS"), Colour::fromFloatRGBA(0.1, 0.1, 0.1, 1.0), mRecentsContainer.get(), false);
            // RECENTS belongs after PRIVATE GROUP and DIRECT
            mConnectTab->moveTab(mConnectTab->getNumTabs() - 1, recentsTabPosition);
            if (tabsel >= recentsTabPosition) {
                ++tabsel;
            }
            mConnectTab->setCurrentTabIndex(tabsel);
            updateLayout();
        }
    }


    mainBox.performLayout(getLocalBounds().reduced(2, 2));

    mServerConnectContainer->setBounds(0,0,
                                       mServerConnectViewport->getWidth() - (mServerConnectViewport->getHeight() < minHeight ? mServerConnectViewport->getScrollBarThickness() : 0 ),
                                       jmax(minHeight, mServerConnectViewport->getHeight()));

    mDirectConnectContainer->setBounds(0, 0,
                                       mDirectConnectViewport->getWidth() - (mDirectConnectViewport->getHeight() < minHeight ? mDirectConnectViewport->getScrollBarThickness() : 0),
                                       jmax(minHeight, mDirectConnectViewport->getHeight()));

    remoteBox.performLayout(mDirectConnectContainer->getLocalBounds().withSizeKeepingCentre(jmin(400, mDirectConnectContainer->getWidth()), mDirectConnectContainer->getHeight()));
    serverBox.performLayout(mServerConnectContainer->getLocalBounds().withSizeKeepingCentre(jmin(400, mServerConnectContainer->getWidth()), mServerConnectContainer->getHeight()));

    if (mConnectTab->getTabNames().indexOf(TRANS("RECENTS")) < 0) {
        mRecentsContainer->setBounds(mRecentsGroup->getLocalBounds().reduced(4).withTrimmedTop(10));
    }
    recentsBox.performLayout(mRecentsContainer->getLocalBounds());

    mServerInfoLabel->setBounds(mServerStatusLabel->getBounds());

    if (isVisible()) {
        mRecentsListBox->updateContent();
    }
}

void ConnectView::groupJoinFailed()
{
    mServerGroupPasswordEditor->setColour(TextEditor::backgroundColourId, Colour(0xff880000));
    mServerGroupPasswordEditor->repaint();
}


void ConnectView::showActiveGroupTab()
{

    if (mConnectTab->getCurrentContentComponent() != mServerConnectViewport.get()) {
        showPrivateGroupTab();
    }
}

void ConnectView::showPrivateGroupTab()
{
    const int idx = mConnectTab->getTabNames().indexOf(TRANS("PRIVATE GROUP"));
    if (idx >= 0) {
        mConnectTab->setCurrentTabIndex(idx);
    }
}

bool ConnectView::getServerGroupAndPasswordText(String & retgroup, String & retpass) const
{
    if (mConnectTab->getCurrentContentComponent() == mServerConnectViewport.get()) {
        retgroup = mServerGroupEditor->getText().trim();
        retpass = mServerGroupPasswordEditor->getText();
        return true;
    }
    return false;
}

void ConnectView::visibilityChanged ()
{
    if (!isVisible()) {
        // force password show to off here
        mServerGroupPasswordShowButton->setToggleState(false, dontSendNotification);
        currConnectionInfo.userName = mServerUsernameEditor->getText().trim();
        currConnectionInfo.groupName = mServerGroupEditor->getText().trim();
        currConnectionInfo.groupPassword = mServerGroupPasswordEditor->getText();
        updateState();
    }
}


void ConnectView::connectTabChanged (int newCurrentTabIndex)
{
    ignoreUnused(newCurrentTabIndex);

    if (mConnectTab->getCurrentContentComponent() == mServerConnectViewport.get()) {
        resetPrivateGroupLabels();
    }
}

bool ConnectView::copyInfoToClipboard(bool singleURL, String * retmessage)
{
    String message = TRANS("Share this link with others to connect with Commsbus:") + " \n";

    String hostport = mServerHostEditor->getText();
    if (hostport.isEmpty()) {
        hostport = DEFAULT_SERVER_HOST;
    }

    String groupName;
    String groupPassword;

    if (!processor.isConnectedToServer()) {
        if (mConnectTab->getCurrentContentComponent() == mServerConnectViewport.get()) {
            groupName = mServerGroupEditor->getText().trim();
            groupPassword = mServerGroupPasswordEditor->getText();
        }
    } else {
        groupName = currConnectionInfo.groupName;
        groupPassword = currConnectionInfo.groupPassword;
    }

    String urlstr1;
    urlstr1 << String("commsbus://") << hostport << String("/");
    URL url(urlstr1);
    URL url2("http://go.sonobus.net/sblaunch");

    if (url.isWellFormed() && groupName.isNotEmpty()) {

        url2 = url2.withParameter("s", hostport);

        url = url.withParameter("g", groupName);
        url2 = url2.withParameter("g", groupName);

        if (groupPassword.isNotEmpty()) {
            url = url.withParameter("p", groupPassword);
            url2 = url2.withParameter("p", groupPassword);
        }

        //message += url.toString(true);
        //message += "\n\n";

        //message += TRANS("Or share this link:") + "\n";
        message += url2.toString(true);
        message += "\n";

        if (singleURL) {
            message = url2.toString(true);
        }
        SystemClipboard::copyTextToClipboard(message);

        if (retmessage) {
            *retmessage = message;
        }

        return true;
    }
    return false;
}


void ConnectView::textEditorReturnKeyPressed (TextEditor& ed)
{
    DBG("Return pressed");

    if (isVisible() && mServerConnectButton->isShowing()) {
        //mServerConnectButton->setWantsKeyboardFocus(true);
        mServerConnectButton->grabKeyboardFocus();
        //mServerConnectButton->setWantsKeyboardFocus(false);
    }
    //else if (isVisible() && mDirectConnectButton->isShowing()) {
        //mServerConnectButton->setWantsKeyboardFocus(true);
    //    mDirectConnectButton->grabKeyboardFocus();
        //mServerConnectButton->setWantsKeyboardFocus(false);
    //}
}

void ConnectView::textEditorEscapeKeyPressed (TextEditor& ed)
{
    DBG("escape pressed");
    if (isVisible()) {
        //mServerConnectButton->setWantsKeyboardFocus(true);
        mServerConnectButton->grabKeyboardFocus();
        //mServerConnectButton->setWantsKeyboardFocus(false);
    }
    //grabKeyboardFocus();
}

void ConnectView::textEditorTextChanged (TextEditor& ed)
{
    if (&ed == mServerUsernameEditor.get()) {
        // try to set the current username, it will fail if we are connected, no big deal
        processor.setCurrentUsername(ed.getText().trim());
    }
}


void ConnectView::textEditorFocusLost (TextEditor& ed)
{
    ignoreUnused(ed);
}

void ConnectView::buttonClicked (Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == mDirectConnectButton.get()) {
        String hostport = mAddRemoteHostEditor->getText();
        //String port = mAddRemotePortEditor->getText();
        // parse it
        StringArray toks = StringArray::fromTokens(hostport, ":/ ", "");
        String host;
        int port = 11000;

        if (toks.size() >= 1) {
            host = toks[0].trim();
        }
        if (toks.size() >= 2) {
            port = toks[1].trim().getIntValue();
        }

        if (host.isNotEmpty() && port != 0) {
            if (processor.connectRemotePeer(host, port, "", "", processor.getValueTreeState().getParameter(CommsbusAudioProcessor::paramMainRecvMute)->getValue() == 0)) {
                // Remember it, so this peer is reconnected on the next launch and
                // after any drop, without anyone retyping the address.
                processor.getAutoConnectManager().addPeer(DirectPeerEntry(host, port));
                processor.startAutoConnect();

                setVisible(false);
                if (auto * callout = dynamic_cast<CallOutBox*>(directConnectCalloutBox.get())) {
                    callout->dismiss();
                    directConnectCalloutBox = nullptr;
                }
            }
        }

    }
    else if (buttonThatWasClicked == mServerConnectButton.get()) {
        bool wasconnected = false;
        if (processor.isConnectedToServer()) {

            //mConnectionTimeLabel->setText(TRANS("Total: ") + SonoUtility::durationToString(processor.getElapsedConnectedTime(), true), dontSendNotification);

            processor.cancelAutoReconnect();
            processor.disconnectFromServer();
            //updateState();
            wasconnected = true;

        }

        String hostport = mServerHostEditor->getText();

        // parse it
        StringArray toks = StringArray::fromTokens(hostport, ":", "");
        String host = DEFAULT_SERVER_HOST;
        int port = DEFAULT_SERVER_PORT;

        if (toks.size() >= 1) {
            host = toks[0].trim();
        }
        if (toks.size() >= 2) {
            port = toks[1].trim().getIntValue();
        }

        AooServerConnectionInfo info;
        info.userName = mServerUsernameEditor->getText().trim();
        info.groupName = mServerGroupEditor->getText().trim();
        info.groupPassword = mServerGroupPasswordEditor->getText();
        info.serverHost = host;
        info.serverPort = port;

        connectWithInfo(info);

        listeners.call(&ConnectView::Listener::connectionsChanged, this);

        //mConnectionTimeLabel->setText("", dontSendNotification);

    }
    else if (buttonThatWasClicked == mServerGroupPasswordShowButton.get()) {
        mServerGroupPasswordShowButton->setToggleState(!mServerGroupPasswordShowButton->getToggleState(), dontSendNotification);
        currConnectionInfo.userName = mServerUsernameEditor->getText().trim();
        currConnectionInfo.groupName = mServerGroupEditor->getText().trim();
        currConnectionInfo.groupPassword = mServerGroupPasswordEditor->getText();
        updateState();
    }
    else if (buttonThatWasClicked == mServerGroupRandomButton.get()) {
        // randomize group name
        String rgroup = mRandomSentence->randomSentence();
        mServerGroupEditor->setText(rgroup, dontSendNotification);
    }
    else if (buttonThatWasClicked == mServerPasteButton.get()) {
        if (attemptToPasteConnectionFromClipboard()) {
            updateServerFieldsFromConnectionInfo();
            updateServerStatusLabel(TRANS("Filled in Group information from clipboard! Press 'Connect to Group' to join..."), false);
        }
    }
    else if (buttonThatWasClicked == mServerCopyButton.get()) {
        if (copyInfoToClipboard()) {
            showPopTip(TRANS("Copied connection info to clipboard for you to share with others"), 3000, mServerCopyButton.get());
        }
    }
    else if (buttonThatWasClicked == mServerShareButton.get()) {
        String message;
        bool singleurl = true;
#if JUCE_IOS || JUCE_ANDROID
        singleurl = true;
#endif
        if (copyInfoToClipboard(singleurl, &message)) {
            URL url(message);
            SafePointer<ConnectView> safeThis(this);
            if (url.isWellFormed()) {
                Array<URL> urlarray;
                urlarray.add(url);
                mScopedShareBox = ContentSharer::shareFilesScoped(urlarray, [safeThis](bool result, const String& msg){ DBG("url share returned " << (int)result << " : " <<  msg);
                    safeThis->mScopedShareBox = {};
                });
            } else {
                mScopedShareBox = ContentSharer::shareTextScoped(message, [safeThis](bool result, const String& msg){ DBG("share returned " << (int)result << " : " << msg);
                    safeThis->mScopedShareBox = {};
                });
            }
        }

        //copyInfoToClipboard();
        //showPopTip(TRANS("Copied connection info to clipboard for you to share with others"), 3000, mServerCopyButton.get());
    }
    else if (buttonThatWasClicked == mConnectCloseButton.get()) {
        setVisible(false);

        updateState();
    }
    else if (buttonThatWasClicked == mConnectMenuButton.get()) {

        showAdvancedMenu();
    }
    else if (buttonThatWasClicked == mClearRecentsButton.get()) {
        processor.clearRecentServerConnectionInfos();
        updateRecents();
    }

}

void ConnectView::showAdvancedMenu()
{
    // jlc
    Array<GenericItemChooserItem> items;
    items.add(GenericItemChooserItem(TRANS("Connect to Raw Address...")));

    Component* dw = mConnectMenuButton->findParentComponentOfClass<AudioProcessorEditor>();
    if (!dw) dw = mConnectMenuButton->findParentComponentOfClass<Component>();
    Rectangle<int> bounds =  dw->getLocalArea(nullptr, mConnectMenuButton->getScreenBounds());

    SafePointer<ConnectView> safeThis(this);

    auto callback = [safeThis,dw,bounds](GenericItemChooser* chooser,int index) mutable {
        if (!safeThis) return;
        auto wrap = std::make_unique<Viewport>();

        int defWidth = 320;
#if JUCE_IOS || JUCE_ANDROID
        int defHeight = 300;
#else
        int defHeight = 250;
#endif

        int extrawidth = 0;
        if (defHeight > dw->getHeight() - 24) {
            extrawidth = wrap->getScrollBarThickness() + 1;
        }

        wrap->setSize(jmin(defWidth + extrawidth, dw->getWidth() - 10), jmin(defHeight, dw->getHeight() - 24));

        safeThis->mDirectConnectContainer->setBounds(Rectangle<int>(0,0,defWidth,defHeight));

        wrap->setViewedComponent(safeThis->mDirectConnectContainer.get(), false);
        safeThis->mDirectConnectContainer->setVisible(true);

        safeThis->remoteBox.performLayout(safeThis->mDirectConnectContainer->getLocalBounds().withSizeKeepingCentre(jmin(400, safeThis->mDirectConnectContainer->getWidth()), safeThis->mDirectConnectContainer->getHeight()));

        // show direct connect container
        safeThis->directConnectCalloutBox = & CallOutBox::launchAsynchronously (std::move(wrap), bounds , dw, false);
        if (CallOutBox * box = dynamic_cast<CallOutBox*>(safeThis->directConnectCalloutBox.get())) {
            box->setDismissalMouseClicksAreAlwaysConsumed(true);
        }

    };

    GenericItemChooser::launchPopupChooser(items, bounds, dw, callback, -1, dw ? dw->getHeight()-30 : 0);

}

void ConnectView::connectWithInfo(const AooServerConnectionInfo & info, bool allowEmptyGroup)
{
    currConnectionInfo = info;

    if (currConnectionInfo.groupName.isEmpty() && !allowEmptyGroup) {
        mServerStatusLabel->setText(TRANS("You need to specify a group name!"), dontSendNotification);
        mServerGroupEditor->setColour(TextEditor::backgroundColourId, Colour(0xff880000));
        mServerGroupEditor->repaint();
        mServerInfoLabel->setVisible(false);
        mServerStatusLabel->setVisible(true);
        return;
    }
    else {
        mServerGroupEditor->setColour(TextEditor::backgroundColourId, Colour(0xff050505));
        mServerGroupEditor->repaint();
    }

    if (currConnectionInfo.userName.trim().isEmpty()) {
        String mesg = TRANS("You need to specify a user name!");

        mServerStatusLabel->setText(mesg, dontSendNotification);
        mServerUsernameEditor->setColour(TextEditor::backgroundColourId, Colour(0xff880000));
        mServerUsernameEditor->repaint();

        mServerInfoLabel->setVisible(false);
        mServerStatusLabel->setVisible(true);
        return;
    }
    else {
        mServerUsernameEditor->setColour(TextEditor::backgroundColourId, Colour(0xff050505));
        mServerUsernameEditor->repaint();
    }

    //mServerGroupPasswordEditor->setColour(TextEditor::backgroundColourId, Colour(0xff880000));
    mServerGroupPasswordEditor->setColour(TextEditor::backgroundColourId, Colour(0xff050505));
    mServerGroupPasswordEditor->repaint();

    if (currConnectionInfo.serverHost.isNotEmpty() && currConnectionInfo.serverPort != 0)
    {
        processor.cancelAutoReconnect(); // the user is connecting somewhere on purpose
        processor.disconnectFromServer();

        Timer::callAfterDelay(100, [this] {
            processor.connectToServer(currConnectionInfo.serverHost, currConnectionInfo.serverPort, currConnectionInfo.userName, currConnectionInfo.userPassword);
           // updateState();
            listeners.call(&ConnectView::Listener::connectionsChanged, this);

        });

        mServerHostEditor->setColour(TextEditor::backgroundColourId, Colour(0xff050505));
        mServerHostEditor->repaint();
    }
    else {
        String mesg = TRANS("Server address is invalid!");
        mServerStatusLabel->setText(mesg, dontSendNotification);
        mServerHostEditor->setColour(TextEditor::backgroundColourId, Colour(0xff880000));
        mServerHostEditor->repaint();

        mServerInfoLabel->setVisible(false);
        mServerStatusLabel->setVisible(true);
    }
}

void ConnectView::updateRecents()
{
    recentsListModel.updateState();
    mRecentsListBox->updateContent();
    mRecentsListBox->deselectAllRows();
}

void ConnectView::resetPrivateGroupLabels()
{
    if (!mServerInfoLabel) return;

    mServerInfoLabel->setText(TRANS("All who join the same Group will be able to connect with each other."), dontSendNotification);
    mServerInfoLabel->setVisible(true);
    mServerStatusLabel->setVisible(false);
}


bool ConnectView::attemptToPasteConnectionFromClipboard()
{
    auto clip = SystemClipboard::getTextFromClipboard();

    if (clip.isNotEmpty()) {
        // look for commsbus URL anywhere in it
        String urlpart = clip.fromFirstOccurrenceOf("commsbus://", true, true);
        if (urlpart.isNotEmpty()) {
            // find the end (whitespace) and strip it out
            urlpart = urlpart.upToFirstOccurrenceOf("\n", false, true).trim();
            urlpart = urlpart.upToFirstOccurrenceOf(" ", false, true).trim();
            URL url(urlpart);

            if (url.isWellFormed()) {
                DBG("Got good commsbus URL: " << urlpart);

                // clear clipboard
                SystemClipboard::copyTextToClipboard("");

                return handleCommsbusURL(url);
            }
        }
        else {
            // look for go.sonobus.net/sblaunch  url
            urlpart = clip.fromFirstOccurrenceOf("http://go.sonobus.net/sblaunch?", true, false);
            if (urlpart.isEmpty()) urlpart = clip.fromFirstOccurrenceOf("https://go.sonobus.net/sblaunch?", true, false);

            if (urlpart.isNotEmpty()) {
                urlpart = urlpart.upToFirstOccurrenceOf("\n", false, true).trim();
                urlpart = urlpart.upToFirstOccurrenceOf(" ", false, true).trim();
                URL url(urlpart);

                if (url.isWellFormed()) {
                    DBG("Got good http commsbus URL: " << urlpart);

                    SystemClipboard::copyTextToClipboard("");

                    return handleCommsbusURL(url);
                }
            }
        }
    }

    return false;
}

bool ConnectView::handleCommsbusURL(const URL & url)
{
    // look for either  http://go.sonobus.net/sblaunch?  style url
    // or commsbus://host:port/? style

    auto & pnames = url.getParameterNames();
    auto & pvals = url.getParameterValues();

    int ind;
    if ((ind = pnames.indexOf("g", true)) >= 0) {
        currConnectionInfo.groupName = pvals[ind];

        if ((ind = pnames.indexOf("p", true)) >= 0) {
            currConnectionInfo.groupPassword = pvals[ind];
        } else {
            currConnectionInfo.groupPassword = "";
        }

    }

    if (url.getScheme() == "commsbus") {
        // use domain part as host:port
        String hostpart = url.getDomain();
        currConnectionInfo.serverHost =  hostpart.upToFirstOccurrenceOf(":", false, true);
        int port = url.getPort();
        if (port > 0) {
            currConnectionInfo.serverPort = port;
        } else {
            currConnectionInfo.serverPort = DEFAULT_SERVER_PORT;
        }
    }
    else {
        if ((ind = pnames.indexOf("s", true)) >= 0) {
            String hostpart = pvals[ind];
            currConnectionInfo.serverHost =  hostpart.upToFirstOccurrenceOf(":", false, true);
            String portpart = hostpart.fromFirstOccurrenceOf(":", false, false);
            int port = portpart.getIntValue();
            if (port > 0) {
                currConnectionInfo.serverPort = port;
            } else {
                currConnectionInfo.serverPort = DEFAULT_SERVER_PORT;
            }
        }
    }

    return true;
}


void ConnectView::updateServerStatusLabel(const String & mesg, bool mainonly)
{
    const double fadeAfterSec = 5.0;
    //mMainStatusLabel->setText(mesg, dontSendNotification);
    //Desktop::getInstance().getAnimator().fadeIn(mMainStatusLabel.get(), 200);
    //serverStatusFadeTimestamp = Time::getMillisecondCounterHiRes() * 1e-3 + fadeAfterSec;

    if (!mainonly) {
        mServerStatusLabel->setText(mesg, dontSendNotification);
        mServerInfoLabel->setVisible(false);
        mServerStatusLabel->setVisible(true);
    }
}


void ConnectView::updateServerFieldsFromConnectionInfo()
{
    if (currConnectionInfo.serverPort == DEFAULT_SERVER_PORT) {
        mServerHostEditor->setText( currConnectionInfo.serverHost, false);
    } else {
        String hostport;
        hostport << currConnectionInfo.serverHost << ":" << currConnectionInfo.serverPort;
        mServerHostEditor->setText( hostport, false);
    }
    mServerUsernameEditor->setText(currConnectionInfo.userName, false);
    if (currConnectionInfo.groupName.isNotEmpty()){
        mServerGroupEditor->setText(currConnectionInfo.groupName, false);
    }

    mServerGroupPasswordEditor->setPasswordCharacter(mServerGroupPasswordShowButton->getToggleState() ? 0 : 0x2022); // bullet if not showing password
    
    mServerGroupPasswordEditor->setText(currConnectionInfo.groupPassword, false);

}

void ConnectView::showPopTip(const String & message, int timeoutMs, Component * target, int maxwidth)
{
    popTip.reset(new BubbleMessageComponent());
    popTip->setAllowedPlacement(BubbleComponent::above);
    
    if (target) {
        if (auto * parent = target->findParentComponentOfClass<AudioProcessorEditor>()) {
            parent->addChildComponent (popTip.get());
        } else {
            addChildComponent(popTip.get());            
        }
    }
    else {
        addChildComponent(popTip.get());
    }
    
    AttributedString text(message);
    text.setJustification (Justification::centred);
    text.setColour (findColour (TextButton::textColourOffId));
    text.setFont(Font(12 * SonoLookAndFeel::getFontScale()));
    if (target) {
        popTip->showAt(target, text, timeoutMs);
    }
    else {
        Rectangle<int> topbox(getWidth()/2 - maxwidth/2, 0, maxwidth, 2);
        popTip->showAt(topbox, text, timeoutMs);
    }
    popTip->toFront(false);
    //AccessibilityHandler::postAnnouncement(message, AccessibilityHandler::AnnouncementPriority::medium);
}

void ConnectView::paint(Graphics & g)
{
    /*
    //g.fillAll (Colours::black);
    Rectangle<int> bounds = getLocalBounds();

    bounds.reduce(1, 1);
    bounds.removeFromLeft(3);
    
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    g.setColour(outlineColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 0.5f);
*/
}


#pragma RecentsListModel


ConnectView::RecentsListModel::RecentsListModel(ConnectView * parent_) : parent(parent_)
{
    groupImage = ImageCache::getFromMemory(BinaryData::people_png, BinaryData::people_pngSize);
    personImage = ImageCache::getFromMemory(BinaryData::person_png, BinaryData::person_pngSize);
    removeImage = Drawable::createFromImageData(BinaryData::x_icon_svg, BinaryData::x_icon_svgSize);
}


void ConnectView::RecentsListModel::updateState()
{
    parent->processor.getRecentServerConnectionInfos(recents);

}

int ConnectView::RecentsListModel::getNumRows()
{
    return recents.size();
}

String ConnectView::RecentsListModel::getNameForRow (int rowNumber)
{
    if (rowNumber < recents.size()) {
        return recents.getReference(rowNumber).groupName;
    }
    return ListBoxModel::getNameForRow(rowNumber);
}


void ConnectView::RecentsListModel::paintListBoxItem (int rowNumber, Graphics &g, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= recents.size()) return;

    if (rowIsSelected) {
        g.setColour (parent->findColour(selectedColourId));
        g.fillRect(Rectangle<int>(0,0,width,height));
    }

    g.setColour(parent->findColour(separatorColourId));
    g.drawLine(0, height-1, width, height);


    g.setColour (parent->findColour(nameTextColourId));
    g.setFont (parent->recentsGroupFont);

    AooServerConnectionInfo & info = recents.getReference(rowNumber);

    float xratio = 0.5;
    int removewidth = jmin(36, height - 6);
    float yratio = 0.6;
    float adjwidth = width - removewidth;

    // DebugLogC("Paint %s", text.toRawUTF8());
    float iconsize = height*yratio;
    float groupheight = height*yratio;
    g.drawImageWithin(groupImage, 0, 0, iconsize, iconsize, RectanglePlacement::fillDestination);
    String grouptext;
    grouptext << info.groupName;
    g.drawFittedText (grouptext, iconsize + 4, 0, adjwidth*xratio - 8 - iconsize, groupheight, Justification::centredLeft, true);

    g.setFont (parent->recentsNameFont);
    g.setColour (parent->findColour(nameTextColourId).withAlpha(0.8f));
    g.drawImageWithin(personImage, adjwidth*xratio, 0, iconsize, iconsize, RectanglePlacement::fillDestination);
    g.drawFittedText (info.userName, adjwidth*xratio + iconsize, 0, adjwidth*(1.0f - xratio) - 4 - iconsize, groupheight, Justification::centredLeft, true);

    String infostr;

    if (info.groupPassword.isNotEmpty()) {
        infostr += TRANS("password protected,") + " ";
    }

    infostr += TRANS("on") + " " + Time(info.timestamp).toString(true, true, false) + " " ;

    if (info.serverHost != DEFAULT_SERVER_HOST) {
        infostr += TRANS("to") + " " +  info.serverHost;
    }

    g.setColour (parent->findColour(nameTextColourId).withAlpha(0.5f));
    g.setFont (parent->recentsInfoFont);

    g.drawFittedText (infostr, 14, height * yratio, adjwidth - 24, height * (1.0f - yratio), Justification::centredTop, true);

    removeImage->drawWithin(g, Rectangle<float>(adjwidth + removewidth*0.25*yratio, height*0.5 - removewidth*0.5*yratio, removewidth*yratio, removewidth*yratio), RectanglePlacement::fillDestination, 0.9);

    removeButtonX = adjwidth;
    cachedWidth = width;
}

void ConnectView::RecentsListModel::listBoxItemClicked (int rowNumber, const MouseEvent& e)
{
    // use this
    DBG("Clicked " << rowNumber << "  x: " << e.getPosition().x << "  width: " << cachedWidth);

    if (e.getPosition().x > removeButtonX) {
        parent->processor.removeRecentServerConnectionInfo(rowNumber);
        parent->updateRecents();
    }
    else {
        parent->connectWithInfo(recents.getReference(rowNumber));
    }
}

void ConnectView::RecentsListModel::selectedRowsChanged(int rowNumber)
{

}

void ConnectView::RecentsListModel::deleteKeyPressed (int rowNumber)
{
    DBG("delete key pressed");
    if (rowNumber < recents.size()) {
        parent->processor.removeRecentServerConnectionInfo(rowNumber);
        parent->updateRecents();
    }
}

void ConnectView::RecentsListModel::returnKeyPressed (int rowNumber)
{
    DBG("return key pressed: " << rowNumber);

    if (rowNumber < recents.size()) {
        parent->connectWithInfo(recents.getReference(rowNumber));
    }
}
