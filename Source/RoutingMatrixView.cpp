// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "RoutingMatrixView.h"


//==============================================================================
// ReceiveRouting helpers

namespace ReceiveRouting
{

StringArray getOutputChannelLabels(CommsbusAudioProcessor & proc, AudioDeviceManager * adm)
{
    const int total = proc.getTotalNumOutputChannels();

    StringArray names;
    if (adm) {
        if (auto cad = adm->getCurrentAudioDevice()) {
            auto actives = cad->getActiveOutputChannels();
            auto allnames = cad->getOutputChannelNames();
            for (int ni = 0; ni < allnames.size(); ++ni) {
                if (actives[ni]) {
                    names.add(allnames[ni]);
                }
            }
        }
    }

    StringArray labels;
    for (int i = 0; i < total; ++i) {
        String label = String(i + 1).paddedLeft('0', 2) + " ";
        if (i < names.size() && names[i].trim().isNotEmpty()) {
            label << names[i].trim();
        } else {
            label << TRANS("Out") << " " << (i + 1);
        }
        labels.add(label);
    }
    return labels;
}

String getLocalDeviceName(AudioDeviceManager * adm)
{
    if (adm) {
        if (auto cad = adm->getCurrentAudioDevice()) {
            if (cad->getName().isNotEmpty()) {
                return cad->getName();
            }
        }
    }
    return TRANS("This Device");
}

String getStreamName(CommsbusAudioProcessor & proc, int peer, int group)
{
    auto name = proc.getRemotePeerChannelGroupName(peer, group).trim();
    if (name.isEmpty()) {
        name << TRANS("Stream") << " " << (group + 1);
    }
    return name;
}

String describeOutputSources(CommsbusAudioProcessor & proc, int outch, int * retCount, int * retBus)
{
    StringArray parts;
    int hidden = 0;
    int bus = -1;

    const int numpeers = proc.getNumberRemotePeers();
    for (int p = 0; p < numpeers; ++p) {
        const int groups = proc.getRemotePeerChannelGroupCount(p);
        for (int g = 0; g < groups; ++g) {
            int start = -1, count = 0, b = -1;
            if (!proc.getRemotePeerChannelGroupOutputs(p, g, start, count, b)) continue;
            if (outch < start || outch >= start + count) continue;

            if (b >= 0) bus = b;

            // star network: names of peers this machine does not list stay hidden
            if (proc.isPeerVisible(p)) {
                parts.add(getStreamName(proc, p, g) + "@" + proc.getRemotePeerDisplayName(p));
            } else {
                ++hidden;
            }
        }
    }

    if (retCount) *retCount = parts.size() + hidden;
    if (retBus) *retBus = bus;

    String text = parts.joinIntoString(", ");
    if (hidden > 0) {
        if (text.isNotEmpty()) text << ", ";
        text << "+" << hidden << " " << TRANS("hidden");
    }
    return text;
}

String describeStreamDestination(CommsbusAudioProcessor & proc, int peer, int group, const StringArray & outputLabels)
{
    int start = -1, count = 0, bus = -1;
    if (!proc.getRemotePeerChannelGroupOutputs(peer, group, start, count, bus)) {
        return {};
    }

    auto labelFor = [&outputLabels](int ch) {
        return isPositiveAndBelow(ch, outputLabels.size()) ? outputLabels[ch] : TRANS("Out") + " " + String(ch + 1);
    };

    String text;
    if (bus >= 0) {
        text << proc.getOutputBusName(bus) << " -> ";
    }
    text << labelFor(start);
    if (count > 1) {
        text << " - " << labelFor(start + count - 1);
    }
    return text;
}

void drawTick(Graphics & g, juce::Rectangle<float> r, Colour colour, float thickness)
{
    Path p;
    p.startNewSubPath(r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.55f);
    p.lineTo(r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.78f);
    p.lineTo(r.getX() + r.getWidth() * 0.84f, r.getY() + r.getHeight() * 0.24f);
    g.setColour(colour);
    g.strokePath(p, PathStrokeType(thickness, PathStrokeType::curved, PathStrokeType::rounded));
}

static const char * streamDragPrefix = "cbstream";
static const char * peerDragPrefix = "cbpeer";

String makeStreamDragDescription(int peer, int group, const String & peerName)
{
    return String(streamDragPrefix) + "|" + String(peer) + "|" + String(group) + "|" + peerName;
}

String makePeerDragDescription(int peer, const String & peerName)
{
    return String(peerDragPrefix) + "|" + String(peer) + "|-1|" + peerName;
}

bool parseDragDescription(CommsbusAudioProcessor & proc, const var & desc, int & retPeer, int & retGroup)
{
    const String text = desc.toString();
    if (!text.startsWith(streamDragPrefix) && !text.startsWith(peerDragPrefix)) return false;

    auto toks = StringArray::fromTokens(text, "|", "");
    if (toks.size() < 4) return false;

    const int peer = toks[1].getIntValue();
    const int group = toks[2].getIntValue();
    // the name may itself contain '|'
    StringArray nameparts;
    for (int i = 3; i < toks.size(); ++i) nameparts.add(toks[i]);
    const String name = nameparts.joinIntoString("|");

    if (!isPositiveAndBelow(peer, proc.getNumberRemotePeers())) return false;
    // peer indices shift when someone leaves; refuse a drag that now points elsewhere
    if (proc.getRemotePeerDisplayName(peer) != name) return false;
    if (group >= 0 && !isPositiveAndBelow(group, proc.getRemotePeerChannelGroupCount(peer))) return false;

    retPeer = peer;
    retGroup = text.startsWith(streamDragPrefix) ? group : -1;
    return true;
}

} // namespace ReceiveRouting


//==============================================================================
// child components: they only forward to the owner, which holds the model

class RoutingMatrixView::GridComponent : public Component, public TooltipClient
{
public:
    GridComponent(RoutingMatrixView & o) : owner(o) {
        setOpaque(true);
        setRepaintsOnMouseActivity(false);
    }

    void paint(Graphics & g) override { owner.paintGrid(g, g.getClipBounds()); }
    void mouseMove(const MouseEvent & e) override { owner.gridMouseMove(e.x, e.y); }
    void mouseExit(const MouseEvent &) override { owner.gridMouseExit(); }
    void mouseDown(const MouseEvent & e) override {
        if (e.mods.isPopupMenu()) return;
        owner.gridClicked(e.x, e.y);
    }
    String getTooltip() override { return owner.getGridTooltip(); }

    RoutingMatrixView & owner;
};

class RoutingMatrixView::GridViewport : public Viewport
{
public:
    GridViewport(RoutingMatrixView & o) : owner(o) {}
    void visibleAreaChanged(const juce::Rectangle<int> &) override { owner.viewportMoved(); }
    RoutingMatrixView & owner;
};

class RoutingMatrixView::ColumnHeader : public Component
{
public:
    ColumnHeader(RoutingMatrixView & o) : owner(o) { setRepaintsOnMouseActivity(false); }
    void paint(Graphics & g) override { owner.paintColumnHeader(g, getWidth(), getHeight()); }
    void mouseDown(const MouseEvent & e) override { owner.columnHeaderMouseDown(e); }
    void mouseMove(const MouseEvent & e) override { owner.columnHeaderMouseMove(e.x, e.y); }
    void mouseExit(const MouseEvent &) override { owner.headerMouseExit(); }
    void mouseWheelMove(const MouseEvent & e, const MouseWheelDetails & w) override {
        if (owner.mViewport) owner.mViewport->mouseWheelMove(e.getEventRelativeTo(owner.mViewport.get()), w);
    }
    RoutingMatrixView & owner;
};

class RoutingMatrixView::RowHeader : public Component
{
public:
    RowHeader(RoutingMatrixView & o) : owner(o) { setRepaintsOnMouseActivity(false); }
    void paint(Graphics & g) override { owner.paintRowHeader(g, getWidth(), getHeight()); }
    void mouseDown(const MouseEvent & e) override { owner.rowHeaderMouseDown(e); }
    void mouseMove(const MouseEvent & e) override { owner.rowHeaderMouseMove(e.x, e.y); }
    void mouseExit(const MouseEvent &) override { owner.headerMouseExit(); }
    void mouseWheelMove(const MouseEvent & e, const MouseWheelDetails & w) override {
        if (owner.mViewport) owner.mViewport->mouseWheelMove(e.getEventRelativeTo(owner.mViewport.get()), w);
    }
    RoutingMatrixView & owner;
};


//==============================================================================

namespace {
    // the +/- box drawn in device headers
    constexpr int expandBoxSize = 12;

    void drawExpandBox(Graphics & g, juce::Rectangle<float> r, bool collapsed, Colour colour)
    {
        g.setColour(colour.withMultipliedAlpha(0.25f));
        g.fillRect(r);
        g.setColour(colour);
        g.drawRect(r, 1.0f);
        const auto c = r.getCentre();
        const float hw = r.getWidth() * 0.28f;
        g.drawLine(c.x - hw, c.y, c.x + hw, c.y, 1.5f);
        if (collapsed) {
            g.drawLine(c.x, c.y - hw, c.x, c.y + hw, 1.5f);
        }
    }

    const String defaultStatusText()
    {
        return TRANS("Click a crosspoint to patch or unpatch. Double-click a device name to open its Device View.");
    }
}


RoutingMatrixView::RoutingMatrixView(CommsbusAudioProcessor & proc)
 : Component("routingmatrix"), processor(proc)
{
    bgColor = Colour::fromFloatRGBA(0.045f, 0.06f, 0.08f, 1.0f);
    headerBgColor = Colour::fromFloatRGBA(0.075f, 0.09f, 0.11f, 1.0f);
    deviceHeaderBgColor = Colour::fromFloatRGBA(0.12f, 0.15f, 0.19f, 1.0f);
    gridLineColor = Colour::fromFloatRGBA(0.22f, 0.25f, 0.28f, 1.0f);
    textColor = Colour(0xe0eeeeee);
    dimTextColor = Colour(0xa0aaaaaa);
    accentColor = Colour::fromFloatRGBA(0.55f, 0.78f, 0.95f, 1.0f);
    tickColor = Colour(0xff4cd964);
    hoverColor = Colour::fromFloatRGBA(0.55f, 0.78f, 0.95f, 0.13f);

    mGrid = std::make_unique<GridComponent>(*this);
    mViewport = std::make_unique<GridViewport>(*this);
    mViewport->setViewedComponent(mGrid.get(), false);
    mViewport->setScrollBarsShown(true, true, true, true);
    mViewport->setScrollBarThickness(10);
    addAndMakeVisible(mViewport.get());

    mColHeader = std::make_unique<ColumnHeader>(*this);
    mRowHeader = std::make_unique<RowHeader>(*this);
    addAndMakeVisible(mColHeader.get());
    addAndMakeVisible(mRowHeader.get());

    auto setupFilter = [this](TextEditor * ed, const String & placeholder, const String & tip) {
        ed->setTextToShowWhenEmpty(placeholder, dimTextColor);
        ed->setTooltip(tip);
        ed->setTitle(placeholder);
        ed->setFont(Font(13));
        ed->setIndents(6, 3);
        ed->setColour(TextEditor::backgroundColourId, Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.35f));
        ed->setColour(TextEditor::outlineColourId, gridLineColor);
        ed->setColour(TextEditor::textColourId, textColor);
        ed->onTextChange = [this]() { refresh(); };
        ed->onEscapeKey = [ed]() { ed->clear(); ed->giveAwayKeyboardFocus(); };
        addAndMakeVisible(ed);
    };

    mLockButton = std::make_unique<TextButton>();
    mLockButton->onClick = [this]() {
        processor.setPatchingLocked(!processor.isPatchingLocked());
        updateLockButton();
        showStatus(processor.isPatchingLocked() ? TRANS("Patching locked.") : TRANS("Patching unlocked - changes take effect immediately."), !processor.isPatchingLocked());
    };
    addAndMakeVisible(mLockButton.get());
    updateLockButton();

    mFilterTxEditor = std::make_unique<TextEditor>("filtertx");
    setupFilter(mFilterTxEditor.get(), TRANS("Filter Transmitters"), TRANS("Show only received streams (or peers) whose name contains this text"));
    mFilterRxEditor = std::make_unique<TextEditor>("filterrx");
    setupFilter(mFilterRxEditor.get(), TRANS("Filter Receivers"), TRANS("Show only output channels whose name contains this text"));

    mTransmittersLabel = std::make_unique<Label>("tx", TRANS("Transmitters") + String(" ") + String(CharPointer_UTF8("\xe2\x96\xb6")));
    mTransmittersLabel->setJustificationType(Justification::centredRight);
    mTransmittersLabel->setFont(Font(12, Font::bold));
    mTransmittersLabel->setColour(Label::textColourId, accentColor);
    mTransmittersLabel->setTooltip(TRANS("Columns: streams received from the far end, grouped by peer"));
    addAndMakeVisible(mTransmittersLabel.get());

    mReceiversLabel = std::make_unique<Label>("rx", TRANS("Receivers") + String(" ") + String(CharPointer_UTF8("\xe2\x96\xbc")));
    mReceiversLabel->setJustificationType(Justification::centredLeft);
    mReceiversLabel->setFont(Font(12, Font::bold));
    mReceiversLabel->setColour(Label::textColourId, accentColor);
    mReceiversLabel->setTooltip(TRANS("Rows: this device's (Dante) output channels"));
    addAndMakeVisible(mReceiversLabel.get());

    mStatusLabel = std::make_unique<Label>("status", defaultStatusText());
    mStatusLabel->setFont(Font(12));
    mStatusLabel->setColour(Label::textColourId, dimTextColor);
    mStatusLabel->setJustificationType(Justification::centredLeft);
    mStatusLabel->setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(mStatusLabel.get());
}

RoutingMatrixView::~RoutingMatrixView()
{
}

int RoutingMatrixView::getNaturalHeight() const
{
    return colHeaderHeight + mRows.size() * cellSize + mViewport->getScrollBarThickness() + 2 + statusHeight;
}

void RoutingMatrixView::resized()
{
    auto bounds = getLocalBounds();

    mStatusLabel->setBounds(bounds.removeFromBottom(statusHeight).reduced(4, 0));

    const int rhw = jmin(rowHeaderWidth, jmax(80, bounds.getWidth() / 2));
    const int chh = jmin(colHeaderHeight, jmax(60, bounds.getHeight() / 2));

    mColHeader->setBounds(bounds.getX() + rhw, bounds.getY(), jmax(0, bounds.getWidth() - rhw), chh);
    mRowHeader->setBounds(bounds.getX(), bounds.getY() + chh, rhw, jmax(0, bounds.getHeight() - chh));
    mViewport->setBounds(bounds.getX() + rhw, bounds.getY() + chh, jmax(0, bounds.getWidth() - rhw), jmax(0, bounds.getHeight() - chh));

    // the corner: two filter boxes, then the axis labels
    auto corner = juce::Rectangle<int>(bounds.getX(), bounds.getY(), rhw, chh).reduced(6, 6);
    mLockButton->setBounds(corner.removeFromTop(24));
    corner.removeFromTop(4);
    mFilterTxEditor->setBounds(corner.removeFromTop(24));
    corner.removeFromTop(4);
    mFilterRxEditor->setBounds(corner.removeFromTop(24));
    mReceiversLabel->setBounds(corner.removeFromBottom(18));
    mTransmittersLabel->setBounds(corner.removeFromBottom(18));

    viewportMoved();
}

void RoutingMatrixView::paint(Graphics & g)
{
    g.fillAll(bgColor);

    const auto vpb = mViewport->getBounds();

    // corner frame lines, so the frozen headers read as one block
    g.setColour(gridLineColor);
    g.drawHorizontalLine(vpb.getY() - 1, 0.0f, (float) getWidth());
    g.drawVerticalLine(vpb.getX() - 1, 0.0f, (float) (getHeight() - statusHeight));

    if (mCols.isEmpty() || mOutputLabels.isEmpty()) {
        g.setColour(dimTextColor);
        g.setFont(Font(14));
        String msg;
        if (mOutputLabels.isEmpty()) {
            msg = TRANS("No audio device outputs. Choose an output device in Settings > AUDIO.");
        } else if (mPeers.isEmpty()) {
            msg = TRANS("No streams are being received yet.");
        } else {
            msg = TRANS("No transmitters match the filter.");
        }
        auto r = vpb;
        if (r.getHeight() > 60) r = r.withHeight(60);
        g.drawFittedText(msg, r.reduced(10, 4), Justification::centredLeft, 2);
    }
}

void RoutingMatrixView::viewportMoved()
{
    mColHeader->repaint();
    mRowHeader->repaint();
}

void RoutingMatrixView::showStatus(const String & text, bool important)
{
    mStatusLabel->setText(text, dontSendNotification);
    mStatusLabel->setColour(Label::textColourId, important ? accentColor : dimTextColor);
    mStatusStamp = important ? Time::getMillisecondCounter() : 0;
}


//==============================================================================
// model

String RoutingMatrixView::computeSignature() const
{
    String sig;
    sig << mDeviceName << "\n" << mOutputLabels.joinIntoString("\t") << "\n";
    for (auto & pinfo : mPeers) {
        sig << pinfo.peer << ":" << pinfo.name << "{";
        for (auto si : pinfo.streams) {
            sig << mStreams.getReference(si).name << "\t";
        }
        sig << "}";
    }
    sig << "\n" << mFilterTxEditor->getText() << "\n" << mFilterRxEditor->getText()
        << "\n" << mCollapsedPeers.joinIntoString("\t") << "\n" << (int) mDeviceCollapsed;
    return sig;
}

void RoutingMatrixView::updateLockButton()
{
    const bool locked = processor.isPatchingLocked();
    mLockButton->setButtonText(locked ? TRANS("LOCKED - click to unlock") : TRANS("UNLOCKED - click to lock"));
    mLockButton->setTooltip(TRANS("Patching must be unlocked before routing can be changed, to avoid accidental changes"));
    mLockButton->setColour(TextButton::buttonColourId, locked ? Colour(0xff3a3a3a) : Colour(0xffb5651d));
    mLockButton->setColour(TextButton::textColourOffId, Colours::white);
}

void RoutingMatrixView::refresh()
{
    updateLockButton();
    AudioDeviceManager * adm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;

    mOutputLabels = ReceiveRouting::getOutputChannelLabels(processor, adm);
    mDeviceName = ReceiveRouting::getLocalDeviceName(adm);

    mPeers.clearQuick();
    mStreams.clearQuick();

    const int numpeers = processor.getNumberRemotePeers();
    for (int p = 0; p < numpeers; ++p) {
        if (!processor.isPeerVisible(p)) continue;

        PeerInfo pinfo;
        pinfo.peer = p;
        pinfo.name = processor.getRemotePeerDisplayName(p);
        if (pinfo.name.isEmpty()) pinfo.name = TRANS("Peer") + " " + String(p + 1);

        const int groups = processor.getRemotePeerChannelGroupCount(p);
        for (int g = 0; g < groups; ++g) {
            StreamInfo sinfo;
            sinfo.peer = p;
            sinfo.group = g;
            sinfo.name = ReceiveRouting::getStreamName(processor, p, g);
            processor.getRemotePeerChannelGroupOutputs(p, g, sinfo.destStart, sinfo.destCount, sinfo.destBus);
            pinfo.streams.add(mStreams.size());
            mStreams.add(sinfo);
        }
        mPeers.add(pinfo);
    }

    const auto sig = computeSignature();
    if (sig != mLastSignature) {
        mLastSignature = sig;
        rebuildAxes();
    }

    // an important message (a bus was created, etc.) stays up for a while
    if (mStatusStamp != 0 && (int32) (Time::getMillisecondCounter() - mStatusStamp) > 10000) {
        showStatus(defaultStatusText(), false);
    }

    repaint();
    mGrid->repaint();
    mColHeader->repaint();
    mRowHeader->repaint();
}

void RoutingMatrixView::rebuildAxes()
{
    mCols.clearQuick();
    mRows.clearQuick();

    const String txfilt = mFilterTxEditor->getText().trim();
    const String rxfilt = mFilterRxEditor->getText().trim();

    for (int pi = 0; pi < mPeers.size(); ++pi) {
        const auto & pinfo = mPeers.getReference(pi);
        const bool peerMatches = txfilt.isEmpty() || pinfo.name.containsIgnoreCase(txfilt);

        Array<int> matching;
        for (auto si : pinfo.streams) {
            if (peerMatches || mStreams.getReference(si).name.containsIgnoreCase(txfilt)) {
                matching.add(si);
            }
        }
        if (!peerMatches && matching.isEmpty()) continue;

        AxisItem header;
        header.isHeader = true;
        header.owner = pi;
        mCols.add(header);

        if (!mCollapsedPeers.contains(pinfo.name)) {
            for (auto si : matching) {
                AxisItem item;
                item.owner = pi;
                item.index = si;
                mCols.add(item);
            }
        }
    }

    if (mOutputLabels.size() > 0) {
        AxisItem devheader;
        devheader.isHeader = true;
        mRows.add(devheader);

        if (!mDeviceCollapsed) {
            for (int o = 0; o < mOutputLabels.size(); ++o) {
                if (rxfilt.isEmpty() || mOutputLabels[o].containsIgnoreCase(rxfilt)) {
                    AxisItem item;
                    item.index = o;
                    mRows.add(item);
                }
            }
        }
    }

    mHoverRow = mHoverCol = -1;

    mGrid->setSize(jmax(1, mCols.size() * cellSize), jmax(1, mRows.size() * cellSize));

    // the natural height moved, so let the editor re-lay out the receive area
    if (onLayoutChanged) onLayoutChanged();
}

RoutingMatrixView::CellState RoutingMatrixView::getCellState(int row, int col, String * retTip) const
{
    if (!isPositiveAndBelow(row, mRows.size()) || !isPositiveAndBelow(col, mCols.size())) return CellNone;

    const auto & r = mRows.getReference(row);
    const auto & c = mCols.getReference(col);

    if (!r.isHeader && !c.isHeader) {
        const auto & s = mStreams.getReference(c.index);
        if (!s.isPatchedTo(r.index)) return CellNone;
        if (retTip && s.destBus >= 0) {
            *retTip = processor.getOutputBusName(s.destBus);
        }
        return s.destBus >= 0 ? CellPatchedViaBus : CellPatched;
    }

    // summary cells: is anything under this header patched here?
    auto streamPatchedToRow = [this, &r](const StreamInfo & s) {
        return r.isHeader ? s.destStart >= 0 : s.isPatchedTo(r.index);
    };

    if (c.isHeader) {
        for (auto si : mPeers.getReference(c.owner).streams) {
            if (streamPatchedToRow(mStreams.getReference(si))) return CellSummary;
        }
        return CellNone;
    }

    return streamPatchedToRow(mStreams.getReference(c.index)) ? CellSummary : CellNone;
}

int RoutingMatrixView::columnAtX(int contentX) const
{
    if (contentX < 0) return -1;
    const int c = contentX / cellSize;
    return c < mCols.size() ? c : -1;
}

int RoutingMatrixView::rowAtY(int contentY) const
{
    if (contentY < 0) return -1;
    const int r = contentY / cellSize;
    return r < mRows.size() ? r : -1;
}


//==============================================================================
// painting

void RoutingMatrixView::paintGrid(Graphics & g, juce::Rectangle<int> clip)
{
    g.setColour(bgColor);
    g.fillRect(clip);

    if (mCols.isEmpty() || mRows.isEmpty()) return;

    const int c0 = jmax(0, clip.getX() / cellSize);
    const int c1 = jmin(mCols.size() - 1, clip.getRight() / cellSize);
    const int r0 = jmax(0, clip.getY() / cellSize);
    const int r1 = jmin(mRows.size() - 1, clip.getBottom() / cellSize);

    const int gridW = mCols.size() * cellSize;
    const int gridH = mRows.size() * cellSize;

    // header bands (device rows / peer columns) are shaded like Dante's
    for (int r = r0; r <= r1; ++r) {
        if (mRows.getReference(r).isHeader) {
            g.setColour(deviceHeaderBgColor.withAlpha(0.6f));
            g.fillRect(0, r * cellSize, gridW, cellSize);
        }
    }
    for (int c = c0; c <= c1; ++c) {
        if (mCols.getReference(c).isHeader) {
            g.setColour(deviceHeaderBgColor.withAlpha(0.6f));
            g.fillRect(c * cellSize, 0, cellSize, gridH);
        }
    }

    // hover cross
    if (isPositiveAndBelow(mHoverRow, mRows.size())) {
        g.setColour(hoverColor);
        g.fillRect(0, mHoverRow * cellSize, gridW, cellSize);
    }
    if (isPositiveAndBelow(mHoverCol, mCols.size())) {
        g.setColour(hoverColor);
        g.fillRect(mHoverCol * cellSize, 0, cellSize, gridH);
    }

    const Colour squareColor = Colour::fromFloatRGBA(0.16f, 0.19f, 0.23f, 1.0f);
    const Colour patchedSquareColor = Colour::fromFloatRGBA(0.10f, 0.30f, 0.16f, 1.0f);
    const Colour busMarkColor = Colour(0xffffb340);

    for (int r = r0; r <= r1; ++r) {
        const bool rowHeader = mRows.getReference(r).isHeader;
        for (int c = c0; c <= c1; ++c) {
            const bool colHeader = mCols.getReference(c).isHeader;
            const auto cell = juce::Rectangle<float>((float) (c * cellSize), (float) (r * cellSize), (float) cellSize, (float) cellSize);
            const auto state = getCellState(r, c);

            if (rowHeader || colHeader) {
                if (state == CellSummary) {
                    // something under this header is patched here
                    g.setColour(tickColor.withAlpha(0.55f));
                    g.fillEllipse(cell.withSizeKeepingCentre(6.0f, 6.0f));
                }
                continue;
            }

            const auto sq = cell.reduced(3.0f);
            if (state == CellPatched || state == CellPatchedViaBus) {
                g.setColour(patchedSquareColor);
                g.fillRect(sq);
                ReceiveRouting::drawTick(g, sq.reduced(1.0f), tickColor, 2.0f);
                if (state == CellPatchedViaBus) {
                    // summed through a bus with other streams
                    g.setColour(busMarkColor);
                    g.fillEllipse(sq.getRight() - 4.0f, sq.getY() - 1.0f, 5.0f, 5.0f);
                }
            }
            else {
                g.setColour(squareColor);
                g.fillRect(sq);
            }
        }
    }

    // grid lines between devices, so groups read as blocks
    g.setColour(gridLineColor);
    for (int c = c0; c <= c1; ++c) {
        if (mCols.getReference(c).isHeader && c > 0) {
            g.fillRect(c * cellSize, 0, 1, gridH);
        }
    }
    for (int r = r0; r <= r1; ++r) {
        if (mRows.getReference(r).isHeader && r > 0) {
            g.fillRect(0, r * cellSize, gridW, 1);
        }
    }
}

void RoutingMatrixView::paintColumnHeader(Graphics & g, int width, int height)
{
    g.fillAll(headerBgColor);

    const int viewX = mViewport->getViewPositionX();
    const int c0 = jmax(0, viewX / cellSize);
    const int c1 = jmin(mCols.size() - 1, (viewX + width) / cellSize);

    for (int c = c0; c <= c1; ++c) {
        const auto & item = mCols.getReference(c);
        const int x0 = c * cellSize - viewX;

        if (item.isHeader) {
            g.setColour(deviceHeaderBgColor);
            g.fillRect(x0, 0, cellSize, height);
        }
        if (c == mHoverCol) {
            g.setColour(hoverColor);
            g.fillRect(x0, 0, cellSize, height);
        }

        String text;
        float bottomPad = 4.0f;
        if (item.isHeader) {
            const auto & pinfo = mPeers.getReference(item.owner);
            text = pinfo.name;
            bottomPad = (float) (expandBoxSize + 8);
            drawExpandBox(g, juce::Rectangle<float>((float) (x0 + (cellSize - expandBoxSize) / 2), (float) (height - expandBoxSize - 4),
                                                    (float) expandBoxSize, (float) expandBoxSize),
                          mCollapsedPeers.contains(pinfo.name), accentColor);
            g.setColour(accentColor);
            g.setFont(Font(13, Font::bold));
        }
        else {
            text = mStreams.getReference(item.index).name;
            const bool patched = mStreams.getReference(item.index).destStart >= 0;
            g.setColour(patched ? textColor : dimTextColor);
            g.setFont(Font(12));
        }

        // rotated 90 degrees anticlockwise, reading bottom to top: local u runs up
        // the column from its bottom edge, v across it
        {
            Graphics::ScopedSaveState ss(g);
            g.addTransform(AffineTransform::rotation(-MathConstants<float>::halfPi).translated((float) x0, (float) height));
            g.drawText(text, juce::Rectangle<float>(bottomPad, 0.0f, (float) height - bottomPad - 4.0f, (float) cellSize),
                       Justification::centredLeft, true);
        }

        g.setColour(item.isHeader ? gridLineColor : gridLineColor.withAlpha(0.5f));
        g.fillRect(x0, 0, 1, height);
    }

    g.setColour(gridLineColor);
    g.fillRect(0, height - 1, width, 1);
}

void RoutingMatrixView::paintRowHeader(Graphics & g, int width, int height)
{
    g.fillAll(headerBgColor);

    const int viewY = mViewport->getViewPositionY();
    const int r0 = jmax(0, viewY / cellSize);
    const int r1 = jmin(mRows.size() - 1, (viewY + height) / cellSize);

    for (int r = r0; r <= r1; ++r) {
        const auto & item = mRows.getReference(r);
        const int y0 = r * cellSize - viewY;

        if (item.isHeader) {
            g.setColour(deviceHeaderBgColor);
            g.fillRect(0, y0, width, cellSize);
        }
        if (r == mHoverRow) {
            g.setColour(hoverColor);
            g.fillRect(0, y0, width, cellSize);
        }

        if (item.isHeader) {
            drawExpandBox(g, juce::Rectangle<float>(4.0f, (float) (y0 + (cellSize - expandBoxSize) / 2), (float) expandBoxSize, (float) expandBoxSize),
                          mDeviceCollapsed, accentColor);
            g.setColour(accentColor);
            g.setFont(Font(13, Font::bold));
            g.drawText(mDeviceName, 22, y0, width - 26, cellSize, Justification::centredLeft, true);
        }
        else {
            bool patched = false;
            for (auto & s : mStreams) {
                if (s.isPatchedTo(item.index)) { patched = true; break; }
            }
            g.setColour(patched ? textColor : dimTextColor);
            g.setFont(Font(12));
            g.drawText(mOutputLabels[item.index], 22, y0, width - 26, cellSize, Justification::centredLeft, true);
        }

        g.setColour(item.isHeader ? gridLineColor : gridLineColor.withAlpha(0.5f));
        g.fillRect(0, y0, width, 1);
    }

    g.setColour(gridLineColor);
    g.fillRect(width - 1, 0, 1, height);
}


//==============================================================================
// interaction

void RoutingMatrixView::setHover(int row, int col)
{
    if (row == mHoverRow && col == mHoverCol) return;

    const int gw = mGrid->getWidth();
    const int gh = mGrid->getHeight();

    // repaint only the bands that change
    auto repaintRow = [this, gw](int r) { if (r >= 0) mGrid->repaint(0, r * cellSize, gw, cellSize); };
    auto repaintCol = [this, gh](int c) { if (c >= 0) mGrid->repaint(c * cellSize, 0, cellSize, gh); };

    repaintRow(mHoverRow);
    repaintCol(mHoverCol);
    mHoverRow = row;
    mHoverCol = col;
    repaintRow(mHoverRow);
    repaintCol(mHoverCol);

    mColHeader->repaint();
    mRowHeader->repaint();
}

void RoutingMatrixView::gridMouseMove(int px, int py)
{
    setHover(rowAtY(py), columnAtX(px));
}

void RoutingMatrixView::gridMouseExit()
{
    setHover(-1, -1);
}

void RoutingMatrixView::headerMouseExit()
{
    setHover(-1, -1);
}

void RoutingMatrixView::columnHeaderMouseMove(int px, int py)
{
    ignoreUnused(py);
    setHover(-1, columnAtX(px + mViewport->getViewPositionX()));
}

void RoutingMatrixView::rowHeaderMouseMove(int px, int py)
{
    ignoreUnused(px);
    setHover(rowAtY(py + mViewport->getViewPositionY()), -1);
}

String RoutingMatrixView::getGridTooltip() const
{
    const int r = mHoverRow, c = mHoverCol;
    if (!isPositiveAndBelow(r, mRows.size()) || !isPositiveAndBelow(c, mCols.size())) return {};

    const auto & row = mRows.getReference(r);
    const auto & col = mCols.getReference(c);

    if (row.isHeader || col.isHeader) {
        if (col.isHeader && mCollapsedPeers.contains(mPeers.getReference(col.owner).name)) {
            return TRANS("Click to expand") + " " + mPeers.getReference(col.owner).name;
        }
        if (row.isHeader && mDeviceCollapsed) {
            return TRANS("Click to expand the output channels");
        }
        return {};
    }

    const auto & s = mStreams.getReference(col.index);
    const auto & pinfo = mPeers.getReference(col.owner);
    const String what = s.name + "@" + pinfo.name;
    const String where = mOutputLabels[row.index];

    String tip;
    if (s.isPatchedTo(row.index)) {
        tip << what << " -> " << where;
        if (s.destBus >= 0) {
            tip << "\n" << TRANS("Combined with other streams through bus") << " \"" << processor.getOutputBusName(s.destBus) << "\"";
        }
        tip << "\n" << TRANS("Click to unpatch");
        return tip;
    }

    tip << TRANS("Click to patch") << " " << what << " -> " << where;

    if (s.destStart >= 0) {
        tip << "\n" << TRANS("A stream goes to one output, so this moves it from")
            << " " << (isPositiveAndBelow(s.destStart, mOutputLabels.size()) ? mOutputLabels[s.destStart] : String(s.destStart + 1));
    }

    int count = 0;
    const String existing = ReceiveRouting::describeOutputSources(processor, row.index, &count);
    if (count > 0) {
        tip << "\n" << where << " " << TRANS("already carries") << " " << existing << ". "
            << TRANS("They will be combined through a bus on that output.");
    }
    return tip;
}

void RoutingMatrixView::gridClicked(int px, int py)
{
    const int r = rowAtY(py);
    const int c = columnAtX(px);
    if (r < 0 || c < 0) return;

    const auto row = mRows[r];
    const auto col = mCols[c];

    if (row.isHeader || col.isHeader) {
        // clicking a collapsed device's band opens it back up
        if (col.isHeader && mCollapsedPeers.contains(mPeers.getReference(col.owner).name)) {
            mCollapsedPeers.removeString(mPeers.getReference(col.owner).name);
            refresh();
        }
        else if (row.isHeader && mDeviceCollapsed) {
            mDeviceCollapsed = false;
            refresh();
        }
        return;
    }

    if (processor.isPatchingLocked()) {
        showStatus(TRANS("Patching is locked - click the lock button to unlock it before making changes."), true);
        return;
    }

    const auto s = mStreams[col.index];
    const String what = s.name + "@" + mPeers.getReference(col.owner).name;
    const String where = mOutputLabels[row.index];

    if (s.isPatchedTo(row.index)) {
        processor.unpatchRemotePeerChannelGroup(s.peer, s.group);
        showStatus(TRANS("Unpatched") + " " + what + " " + TRANS("from") + " " + where, false);
    }
    else {
        String note;
        const int prevStart = s.destStart;
        processor.patchRemotePeerChannelGroupToOutput(s.peer, s.group, row.index, &note);

        String msg;
        msg << TRANS("Patched") << " " << what << " -> " << where;
        if (prevStart >= 0 && prevStart != row.index && isPositiveAndBelow(prevStart, mOutputLabels.size())) {
            msg << " (" << TRANS("moved from") << " " << mOutputLabels[prevStart] << ")";
        }
        if (note.isNotEmpty()) {
            msg << ". " << note;
        }
        showStatus(msg, note.isNotEmpty() || prevStart >= 0);
    }

    refresh();

    if (onRoutingChanged) onRoutingChanged();
}

void RoutingMatrixView::columnHeaderMouseDown(const MouseEvent & e)
{
    const auto pos = e.getPosition();
    const int c = columnAtX(pos.x + mViewport->getViewPositionX());
    if (c < 0) return;

    const auto item = mCols[c];
    if (!item.isHeader) {
        if (e.getNumberOfClicks() >= 2) {
            if (onOpenDeviceView) onOpenDeviceView(mStreams[item.index].peer);
        }
        return;
    }

    const auto pinfo = mPeers[item.owner];
    const bool inBox = pos.y >= mColHeader->getHeight() - expandBoxSize - 8;

    if (inBox) {
        if (e.getNumberOfClicks() == 1) {
            if (mCollapsedPeers.contains(pinfo.name)) mCollapsedPeers.removeString(pinfo.name);
            else mCollapsedPeers.add(pinfo.name);
            refresh();
        }
    }
    else if (e.getNumberOfClicks() >= 2) {
        if (onOpenDeviceView) onOpenDeviceView(pinfo.peer);
    }
}

void RoutingMatrixView::rowHeaderMouseDown(const MouseEvent & e)
{
    const auto pos = e.getPosition();
    const int r = rowAtY(pos.y + mViewport->getViewPositionY());
    if (r < 0) return;

    const auto item = mRows[r];
    const bool inBox = item.isHeader && pos.x < expandBoxSize + 10;

    if (inBox) {
        if (e.getNumberOfClicks() == 1) {
            mDeviceCollapsed = !mDeviceCollapsed;
            refresh();
        }
    }
    else if (e.getNumberOfClicks() >= 2) {
        if (onOpenDeviceView) onOpenDeviceView(-1);
    }
}
