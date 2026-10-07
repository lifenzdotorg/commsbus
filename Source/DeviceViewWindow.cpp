// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "DeviceViewWindow.h"


namespace {

const Colour bgColour        = Colour::fromFloatRGBA(0.045f, 0.06f, 0.08f, 1.0f);
const Colour panelColour     = Colour::fromFloatRGBA(0.075f, 0.09f, 0.11f, 1.0f);
const Colour lineColour      = Colour::fromFloatRGBA(0.22f, 0.25f, 0.28f, 1.0f);
const Colour textColour      = Colour(0xe0eeeeee);
const Colour dimTextColour   = Colour(0xa0aaaaaa);
const Colour accentColour    = Colour::fromFloatRGBA(0.55f, 0.78f, 0.95f, 1.0f);
const Colour tickColour      = Colour(0xff4cd964);

void styleTable(TableListBox & table)
{
    table.setColour(ListBox::backgroundColourId, panelColour);
    table.setColour(ListBox::outlineColourId, lineColour);
    table.setOutlineThickness(1);
    table.setRowHeight(22);
    table.getHeader().setColour(TableHeaderComponent::backgroundColourId, Colour::fromFloatRGBA(0.12f, 0.15f, 0.19f, 1.0f));
    table.getHeader().setColour(TableHeaderComponent::textColourId, accentColour);
    table.getHeader().setColour(TableHeaderComponent::outlineColourId, lineColour);
    table.getHeader().setStretchToFitActive(true);
}

void paintRowShading(Graphics & g, int row, int width, int height, bool selected)
{
    if (selected) {
        g.fillAll(accentColour.withAlpha(0.28f));
    } else if (row % 2) {
        g.fillAll(Colours::white.withAlpha(0.025f));
    }
    g.setColour(lineColour.withAlpha(0.5f));
    g.fillRect(0, height - 1, width, 1);
}

void styleLabel(Label & lab, float size, bool bold, Colour colour)
{
    lab.setFont(Font(size, bold ? Font::bold : Font::plain));
    lab.setColour(Label::textColourId, colour);
    lab.setJustificationType(Justification::centredLeft);
    lab.setMinimumHorizontalScale(0.6f);
}

/** Finds the peer index for a name among the peers this machine lists, or -1. */
int findVisiblePeer(CommsbusAudioProcessor & proc, const String & name)
{
    const int numpeers = proc.getNumberRemotePeers();
    for (int p = 0; p < numpeers; ++p) {
        if (proc.isPeerVisible(p) && proc.getRemotePeerDisplayName(p) == name) return p;
    }
    return -1;
}

} // namespace


//==============================================================================
/**
 * A read-only table: rows of text cells, the last column showing a tick where
 * the row's flag is set. Used for the Transmit tabs.
 */
class InfoTablePanel : public Component, public TableListBoxModel
{
public:
    struct Rows {
        Array<StringArray> cells;
        Array<bool> ticks;
    };

    InfoTablePanel(const StringArray & columns, const Array<int> & widths, std::function<void(Rows &)> fetcher)
    : fetch(std::move(fetcher))
    {
        table = std::make_unique<TableListBox>("info", this);
        styleTable(*table);
        for (int i = 0; i < columns.size(); ++i) {
            table->getHeader().addColumn(columns[i], i + 1, widths[i], 40, -1, TableHeaderComponent::notSortable);
        }
        table->setMultipleSelectionEnabled(false);
        addAndMakeVisible(table.get());

        note = std::make_unique<Label>("note", "");
        styleLabel(*note, 12, false, dimTextColour);
        addAndMakeVisible(note.get());

        numColumns = columns.size();
    }

    void setNote(const String & text) { note->setText(text, dontSendNotification); }

    /** Makes the first column renamable by double-click: getEditText gives the
        text to start from, onRename gets what was typed (possibly empty). */
    void setRenamable(std::function<String(int row)> getEditText, std::function<void(int row, const String &)> onRename)
    {
        getRenameText = std::move(getEditText);
        renameRow = std::move(onRename);
    }

    void cellDoubleClicked(int row, int columnId, const MouseEvent &) override
    {
        if (columnId != 1 || !renameRow || !isPositiveAndBelow(row, rows.cells.size())) return;

        Component::SafePointer<InfoTablePanel> safeThis(this);
        renamer.show(*table, table->getCellPosition(columnId, row, true).reduced(1),
                     getRenameText ? getRenameText(row) : rows.cells.getReference(row)[0],
                     [safeThis, row](const String & text) {
            if (safeThis && safeThis->renameRow) safeThis->renameRow(row, text);
        });
    }

    void refresh()
    {
        rows = Rows();
        if (fetch) fetch(rows);
        table->updateContent();
        table->repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(8);
        note->setBounds(b.removeFromBottom(note->getText().isEmpty() ? 0 : 22));
        table->setBounds(b);
    }

    void paint(Graphics & g) override { g.fillAll(bgColour); }

    int getNumRows() override { return rows.cells.size(); }

    void paintRowBackground(Graphics & g, int row, int width, int height, bool selected) override
    {
        paintRowShading(g, row, width, height, selected);
    }

    void paintCell(Graphics & g, int row, int columnId, int width, int height, bool) override
    {
        if (!isPositiveAndBelow(row, rows.cells.size())) return;

        if (columnId == numColumns) {
            if (rows.ticks[row]) {
                ReceiveRouting::drawTick(g, juce::Rectangle<float>(6.0f, 3.0f, (float) height - 6.0f, (float) height - 6.0f), tickColour, 2.0f);
            }
            return;
        }

        g.setColour(columnId == 1 ? textColour : dimTextColour.brighter(0.3f));
        g.setFont(Font(13));
        g.drawText(rows.cells.getReference(row)[columnId - 1], 6, 0, width - 10, height, Justification::centredLeft, true);
    }

private:
    std::function<void(Rows &)> fetch;
    std::function<String(int)> getRenameText;
    std::function<void(int, const String &)> renameRow;
    Rows rows;
    int numColumns = 0;
    std::unique_ptr<TableListBox> table;
    std::unique_ptr<Label> note;
    ReceiveRouting::InlineRenameEditor renamer; // after table: its editor is a child of it
};


//==============================================================================
/**
 * The local device's Receive tab: output channels on the left, Available
 * Channels (peers -> streams) on the right, drag one onto the other to patch.
 */
class ReceivePanel : public Component, public TableListBoxModel
{
public:
    ReceivePanel(CommsbusAudioProcessor & proc, std::function<AudioDeviceManager*()> getADM, std::function<void()> changed)
    : processor(proc), getAudioDeviceManager(std::move(getADM)), onRoutingChanged(std::move(changed))
    {
        table = std::make_unique<ReceiveTable>(*this);
        styleTable(*table);
        table->getHeader().addColumn(TRANS("Receive Channel"), 1, 170, 80, -1, TableHeaderComponent::notSortable);
        table->getHeader().addColumn(TRANS("Connected To"), 2, 300, 100, -1, TableHeaderComponent::notSortable);
        table->getHeader().addColumn(TRANS("Status"), 3, 56, 40, 80, TableHeaderComponent::notSortable);
        table->setMultipleSelectionEnabled(true);
        table->setTitle(TRANS("Receive Channels"));
        addAndMakeVisible(table.get());

        unsubscribeButton = std::make_unique<TextButton>(TRANS("Unsubscribe"));
        unsubscribeButton->setTooltip(TRANS("Disconnect everything patched to the selected receive channels"));
        unsubscribeButton->onClick = [this]() { unsubscribeSelected(); };
        addAndMakeVisible(unsubscribeButton.get());

        lockButton = std::make_unique<TextButton>();
        lockButton->onClick = [this]() {
            processor.setPatchingLocked(!processor.isPatchingLocked());
            updateLockButton();
            if (onRoutingChanged) onRoutingChanged();
        };
        addAndMakeVisible(lockButton.get());
        updateLockButton();

        hintLabel = std::make_unique<Label>("hint", TRANS("Drag a channel from Available Channels onto a receive channel to patch it."));
        styleLabel(*hintLabel, 12, false, dimTextColour);
        addAndMakeVisible(hintLabel.get());

        availLabel = std::make_unique<Label>("avail", TRANS("Available Channels"));
        styleLabel(*availLabel, 14, true, accentColour);
        addAndMakeVisible(availLabel.get());

        filterEditor = std::make_unique<TextEditor>("filter");
        filterEditor->setTextToShowWhenEmpty(TRANS("Filter"), dimTextColour);
        filterEditor->setTitle(TRANS("Filter Available Channels"));
        filterEditor->setFont(Font(13));
        filterEditor->setIndents(6, 3);
        filterEditor->setColour(TextEditor::backgroundColourId, Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.35f));
        filterEditor->setColour(TextEditor::outlineColourId, lineColour);
        filterEditor->setColour(TextEditor::textColourId, textColour);
        filterEditor->onTextChange = [this]() { refresh(); };
        filterEditor->onEscapeKey = [this]() { filterEditor->clear(); refresh(); };
        addAndMakeVisible(filterEditor.get());

        tree = std::make_unique<TreeView>("available");
        tree->setRootItemVisible(false);
        tree->setDefaultOpenness(true);
        tree->setMultiSelectEnabled(false);
        tree->setIndentSize(16);
        tree->setColour(TreeView::backgroundColourId, panelColour);
        tree->setColour(TreeView::linesColourId, lineColour);
        tree->setColour(TreeView::selectedItemBackgroundColourId, accentColour.withAlpha(0.28f));
        tree->setTitle(TRANS("Available Channels"));
        addAndMakeVisible(tree.get());

        statusLabel = std::make_unique<Label>("status", "");
        styleLabel(*statusLabel, 12, false, accentColour);
        addAndMakeVisible(statusLabel.get());
    }

    ~ReceivePanel() override
    {
        tree->setRootItem(nullptr);
    }

    //------------------------------------------------------------------------------
    class ReceiveTable : public TableListBox, public DragAndDropTarget
    {
    public:
        ReceiveTable(ReceivePanel & o) : TableListBox("receive", &o), owner(o) {}

        bool isInterestedInDragSource(const SourceDetails & details) override
        {
            int p = -1, g = -1;
            return ReceiveRouting::parseDragDescription(owner.processor, details.description, p, g);
        }

        void itemDragEnter(const SourceDetails & details) override { itemDragMove(details); }

        void itemDragMove(const SourceDetails & details) override
        {
            owner.setDropRow(getRowContainingPosition(details.localPosition.x, details.localPosition.y));
        }

        void itemDragExit(const SourceDetails &) override { owner.setDropRow(-1); }

        void itemDropped(const SourceDetails & details) override
        {
            const int row = getRowContainingPosition(details.localPosition.x, details.localPosition.y);
            owner.setDropRow(-1);
            owner.handleDrop(details.description, row);
        }

        ReceivePanel & owner;
    };

    //------------------------------------------------------------------------------
    class StreamItem : public TreeViewItem
    {
    public:
        StreamItem(ReceivePanel & o, int p, int g, const String & pname, const String & sname)
        : owner(o), peer(p), group(g), peerName(pname), streamName(sname) {}

        bool mightContainSubItems() override { return false; }
        String getUniqueName() const override { return "stream:" + String(group) + ":" + streamName; }
        int getItemHeight() const override { return 22; }

        var getDragSourceDescription() override
        {
            return ReceiveRouting::makeStreamDragDescription(peer, group, peerName);
        }

        String getTooltip() override
        {
            return TRANS("Drag onto a receive channel to patch") + " " + streamName + "@" + peerName;
        }

        void paintItem(Graphics & g, int width, int height) override
        {
            const String dest = ReceiveRouting::describeStreamDestination(owner.processor, peer, group, owner.outputLabels);
            const int destw = dest.isEmpty() ? 0 : jmin(width / 2, 150);

            g.setColour(textColour);
            g.setFont(Font(13));
            g.drawText(String(group + 1).paddedLeft('0', 2) + " " + streamName, 4, 0, width - destw - 8, height, Justification::centredLeft, true);

            if (dest.isNotEmpty()) {
                g.setColour(tickColour.withAlpha(0.8f));
                g.setFont(Font(11));
                g.drawText(dest, width - destw - 4, 0, destw, height, Justification::centredRight, true);
            }
        }

        ReceivePanel & owner;
        int peer;
        int group;
        String peerName;
        String streamName;
    };

    class PeerItem : public TreeViewItem
    {
    public:
        PeerItem(int p, const String & name) : peer(p), peerName(name) {}

        bool mightContainSubItems() override { return true; }
        String getUniqueName() const override { return "peer:" + peerName; }
        int getItemHeight() const override { return 22; }

        var getDragSourceDescription() override
        {
            return ReceiveRouting::makePeerDragDescription(peer, peerName);
        }

        String getTooltip() override
        {
            return TRANS("Drag onto a receive channel to patch all of this peer's streams to consecutive channels from there");
        }

        void paintItem(Graphics & g, int width, int height) override
        {
            g.setColour(accentColour);
            g.setFont(Font(13, Font::bold));
            g.drawText(peerName, 4, 0, width - 8, height, Justification::centredLeft, true);
        }

        int peer;
        String peerName;
    };

    //------------------------------------------------------------------------------
    void updateLockButton()
    {
        const bool locked = processor.isPatchingLocked();
        lockButton->setButtonText(locked ? TRANS("LOCKED") : TRANS("UNLOCKED"));
        lockButton->setTooltip(TRANS("Patching must be unlocked before routing can be changed"));
        lockButton->setColour(TextButton::buttonColourId, locked ? Colour(0xff3a3a3a) : Colour(0xffb5651d));
        lockButton->setColour(TextButton::textColourOffId, Colours::white);
    }

    void refresh()
    {
        updateLockButton();
        AudioDeviceManager * adm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;
        outputLabels = ReceiveRouting::getOutputChannelLabels(processor, adm);

        rows.clearQuick();
        for (int o = 0; o < outputLabels.size(); ++o) {
            Row r;
            r.label = outputLabels[o];
            r.connected = ReceiveRouting::describeOutputSources(processor, o, &r.count, &r.bus);
            if (r.bus >= 0) {
                r.connected << "  (" << TRANS("via bus") << " " << processor.getOutputBusName(r.bus) << ")";
            }
            rows.add(r);
        }
        table->updateContent();
        table->repaint();

        rebuildTreeIfNeeded();
        tree->repaint();
    }

    void rebuildTreeIfNeeded()
    {
        const String filt = filterEditor->getText().trim();

        // build a signature first, so an unchanged tree is left alone (keeps the
        // scroll position, selection and any drag in progress)
        struct PeerEntry { int peer; String name; Array<int> groups; StringArray names; };
        Array<PeerEntry> entries;
        String sig = filt + "\n";

        const int numpeers = processor.getNumberRemotePeers();
        for (int p = 0; p < numpeers; ++p) {
            if (!processor.isPeerVisible(p)) continue;
            PeerEntry e;
            e.peer = p;
            e.name = processor.getRemotePeerDisplayName(p);
            const bool peerMatches = filt.isEmpty() || e.name.containsIgnoreCase(filt);
            const int groups = processor.getRemotePeerChannelGroupCount(p);
            for (int g = 0; g < groups; ++g) {
                const auto sname = ReceiveRouting::getStreamName(processor, p, g);
                if (peerMatches || sname.containsIgnoreCase(filt)) {
                    e.groups.add(g);
                    e.names.add(sname);
                }
            }
            if (!peerMatches && e.groups.isEmpty()) continue;
            sig << p << ":" << e.name << "{" << e.names.joinIntoString("\t") << "}";
            entries.add(e);
        }

        if (sig == treeSignature && rootItem) return;
        treeSignature = sig;

        std::unique_ptr<XmlElement> openness(tree->getOpennessState(true));

        auto newroot = std::make_unique<PeerItem>(-1, String());
        for (auto & e : entries) {
            auto * pitem = new PeerItem(e.peer, e.name);
            newroot->addSubItem(pitem);
            for (int i = 0; i < e.groups.size(); ++i) {
                pitem->addSubItem(new StreamItem(*this, e.peer, e.groups[i], e.name, e.names[i]));
            }
        }

        tree->setRootItem(newroot.get());
        rootItem = std::move(newroot);

        if (openness) {
            tree->restoreOpennessState(*openness, true);
        }
    }

    void setDropRow(int row)
    {
        if (row != dropRow) {
            dropRow = row;
            table->repaint();
        }
    }

    void handleDrop(const var & description, int row)
    {
        if (!isPositiveAndBelow(row, outputLabels.size())) return;

        if (processor.isPatchingLocked()) {
            setStatus(TRANS("Patching is locked - unlock it to make changes."));
            return;
        }

        int peer = -1, group = -1;
        if (!ReceiveRouting::parseDragDescription(processor, description, peer, group)) {
            setStatus(TRANS("That channel is no longer available."));
            return;
        }

        const String peerName = processor.getRemotePeerDisplayName(peer);
        String msg;

        if (group >= 0) {
            String note;
            processor.patchRemotePeerChannelGroupToOutput(peer, group, row, &note);
            msg << TRANS("Patched") << " " << ReceiveRouting::getStreamName(processor, peer, group) << "@" << peerName
                << " -> " << outputLabels[row];
            if (note.isNotEmpty()) msg << ". " << note;
        }
        else {
            // a whole peer: its streams onto consecutive channels from here
            const int groups = processor.getRemotePeerChannelGroupCount(peer);
            int done = 0;
            StringArray notes;
            for (int g = 0; g < groups && row + g < outputLabels.size(); ++g) {
                String note;
                processor.patchRemotePeerChannelGroupToOutput(peer, g, row + g, &note);
                if (note.isNotEmpty()) notes.add(note);
                ++done;
            }
            msg << TRANS("Patched") << " " << done << " " << TRANS("streams from") << " " << peerName << " -> "
                << outputLabels[row];
            if (done > 1) msg << " - " << outputLabels[row + done - 1];
            if (done < groups) msg << " (" << (groups - done) << " " << TRANS("did not fit") << ")";
            if (notes.size() > 0) msg << ". " << notes.joinIntoString(" ");
        }

        setStatus(msg);
        refresh();
        if (onRoutingChanged) onRoutingChanged();
    }

    void unsubscribeSelected()
    {
        if (processor.isPatchingLocked()) {
            setStatus(TRANS("Patching is locked - unlock it to make changes."));
            return;
        }

        auto sel = table->getSelectedRows();
        if (sel.isEmpty()) {
            setStatus(TRANS("Select one or more receive channels to unsubscribe."));
            return;
        }

        int total = 0;
        for (int i = 0; i < sel.size(); ++i) {
            total += processor.unpatchOutputChannel(sel[i]);
        }

        setStatus(total > 0 ? TRANS("Unsubscribed") + " " + String(total) + " " + (total == 1 ? TRANS("stream") : TRANS("streams"))
                            : TRANS("Nothing was patched to the selected channels."));
        refresh();
        if (onRoutingChanged) onRoutingChanged();
    }

    void setStatus(const String & text)
    {
        statusLabel->setText(text, dontSendNotification);
    }

    //------------------------------------------------------------------------------
    int getNumRows() override { return rows.size(); }

    void paintRowBackground(Graphics & g, int row, int width, int height, bool selected) override
    {
        paintRowShading(g, row, width, height, selected);
        if (row == dropRow) {
            g.setColour(tickColour.withAlpha(0.25f));
            g.fillAll();
            g.setColour(tickColour);
            g.drawRect(0, 0, width, height, 1);
        }
    }

    void paintCell(Graphics & g, int row, int columnId, int width, int height, bool) override
    {
        if (!isPositiveAndBelow(row, rows.size())) return;
        const auto & r = rows.getReference(row);

        if (columnId == 1) {
            g.setColour(r.count > 0 ? textColour : dimTextColour);
            g.setFont(Font(13));
            g.drawText(r.label, 6, 0, width - 10, height, Justification::centredLeft, true);
        }
        else if (columnId == 2) {
            g.setColour(textColour);
            g.setFont(Font(13));
            g.drawText(r.connected, 6, 0, width - 10, height, Justification::centredLeft, true);
        }
        else if (columnId == 3 && r.count > 0) {
            ReceiveRouting::drawTick(g, juce::Rectangle<float>(6.0f, 3.0f, (float) height - 6.0f, (float) height - 6.0f), tickColour, 2.0f);
        }
    }

    String getCellTooltip(int row, int columnId) override
    {
        if (!isPositiveAndBelow(row, rows.size())) return {};
        const auto & r = rows.getReference(row);
        if (columnId == 2 && r.connected.isNotEmpty()) return r.connected;
        if (columnId == 1) return TRANS("Double-click to rename. Drop a channel from Available Channels here to patch it");
        return TRANS("Drop a channel from Available Channels here to patch it");
    }

    void cellDoubleClicked(int row, int columnId, const MouseEvent &) override
    {
        if (columnId != 1 || !isPositiveAndBelow(row, rows.size())) return;
        startRenamingOutput(row);
    }

    /** Renames output channel `row` in place. Allowed while patching is locked:
        a name changes no routing. */
    void startRenamingOutput(int row)
    {
        AudioDeviceManager * adm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;

        Component::SafePointer<ReceivePanel> safeThis(this);
        renamer.show(*table, table->getCellPosition(1, row, true).reduced(1),
                     ReceiveRouting::getOutputChannelEditName(processor, adm, row),
                     [safeThis, row](const String & text) {
            if (!safeThis) return;
            AudioDeviceManager * adm2 = safeThis->getAudioDeviceManager ? safeThis->getAudioDeviceManager() : nullptr;
            ReceiveRouting::renameOutputChannel(safeThis->processor, adm2, row, text);
            safeThis->refresh();
            // the matrix, the list view's menus and every other device view too
            if (safeThis->onRoutingChanged) safeThis->onRoutingChanged();
        });
    }

    void deleteKeyPressed(int) override { unsubscribeSelected(); }
    void backgroundClicked(const MouseEvent &) override { table->deselectAllRows(); }

    //------------------------------------------------------------------------------
    void paint(Graphics & g) override { g.fillAll(bgColour); }

    void resized() override
    {
        auto b = getLocalBounds().reduced(8);

        statusLabel->setBounds(b.removeFromBottom(22));
        b.removeFromBottom(4);

        const int rightw = jlimit(220, 340, (int) (b.getWidth() * 0.38f));
        auto right = b.removeFromRight(rightw);
        b.removeFromRight(8);

        availLabel->setBounds(right.removeFromTop(24));
        filterEditor->setBounds(right.removeFromTop(24));
        right.removeFromTop(4);
        tree->setBounds(right);

        auto bottom = b.removeFromBottom(28);
        unsubscribeButton->setBounds(bottom.removeFromLeft(110).reduced(0, 2));
        bottom.removeFromLeft(8);
        lockButton->setBounds(bottom.removeFromLeft(100).reduced(0, 2));
        bottom.removeFromLeft(8);
        hintLabel->setBounds(bottom);
        b.removeFromBottom(4);
        table->setBounds(b);
    }

    struct Row {
        String label;
        String connected;
        int count = 0;
        int bus = -1;
    };

    CommsbusAudioProcessor & processor;
    std::function<AudioDeviceManager*()> getAudioDeviceManager;
    std::function<void()> onRoutingChanged;

    StringArray outputLabels;
    Array<Row> rows;
    int dropRow = -1;
    String treeSignature;

    std::unique_ptr<ReceiveTable> table;
    std::unique_ptr<TextButton> unsubscribeButton;
    std::unique_ptr<TextButton> lockButton;
    std::unique_ptr<Label> hintLabel;
    std::unique_ptr<Label> availLabel;
    std::unique_ptr<TextEditor> filterEditor;
    std::unique_ptr<TreeView> tree;
    std::unique_ptr<TreeViewItem> rootItem;
    std::unique_ptr<Label> statusLabel;
    ReceiveRouting::InlineRenameEditor renamer; // after table: its editor is a child of it
};


//==============================================================================

class DeviceViewWindow::Content : public Component, public DragAndDropContainer
{
public:
    Content(CommsbusAudioProcessor & proc, const String & peerName, std::function<AudioDeviceManager*()> getADM, DeviceViewWindow & w)
    : processor(proc), mPeerName(peerName), getAudioDeviceManager(getADM), window(w)
    {
        titleLabel = std::make_unique<Label>("title", "");
        styleLabel(*titleLabel, 18, true, textColour);
        addAndMakeVisible(titleLabel.get());

        subtitleLabel = std::make_unique<Label>("subtitle", "");
        styleLabel(*subtitleLabel, 12, false, dimTextColour);
        addAndMakeVisible(subtitleLabel.get());

        tabs = std::make_unique<TabbedComponent>(TabbedButtonBar::TabsAtTop);
        tabs->setTabBarDepth(28);
        tabs->setOutline(0);
        tabs->setColour(TabbedComponent::backgroundColourId, bgColour);
        addAndMakeVisible(tabs.get());

        auto changed = [this]() { if (window.onRoutingChanged) window.onRoutingChanged(); };

        if (mPeerName.isEmpty()) {
            receivePanel = new ReceivePanel(processor, getAudioDeviceManager, changed);
            tabs->addTab(TRANS("Receive"), panelColour, receivePanel, true);

            transmitPanel = new InfoTablePanel({ TRANS("Transmit Channel"), TRANS("Device Inputs"), TRANS("Sending") },
                                               { 220, 200, 70 },
                                               [this](InfoTablePanel::Rows & rows) { fetchLocalTransmit(rows); });
            transmitPanel->setNote(TRANS("Every transmit channel is sent to every connected peer as its own stream. Double-click a channel to rename it."));
            transmitPanel->setRenamable([this](int row) { return getInputGroupEditName(row); },
                                        [this](int row, const String & text) { renameInputGroup(row, text); });
            tabs->addTab(TRANS("Transmit"), panelColour, transmitPanel, true);
        }
        else {
            transmitPanel = new InfoTablePanel({ TRANS("Transmit Channel"), TRANS("Patched To (here)"), TRANS("Status") },
                                               { 220, 260, 60 },
                                               [this](InfoTablePanel::Rows & rows) { fetchPeerTransmit(rows); });
            transmitPanel->setNote(TRANS("Patch these streams from this device's Receive tab, or in the routing matrix. The peer's own receive routing is set at its end."));
            tabs->addTab(TRANS("Transmit"), panelColour, transmitPanel, true);
        }

        refresh();
    }

    static String defaultInputGroupName(int g)
    {
        return TRANS("Input") + " " + String(g + 1);
    }

    String getInputGroupEditName(int g)
    {
        const String name = processor.getInputGroupName(g).trim();
        return name.isNotEmpty() ? name : defaultInputGroupName(g);
    }

    /** Renames local input group `g` -- the transmit strip's name, and the stream
        name the far end sees. Empty text (or the default) clears it. */
    void renameInputGroup(int g, const String & text)
    {
        if (!isPositiveAndBelow(g, processor.getInputGroupCount())) return;

        String name = text.trim();
        if (name == defaultInputGroupName(g)) name.clear();

        if (processor.getInputGroupName(g) != name) {
            processor.setInputGroupName(g, name);
            // send the new layout (with the name) to every peer
            processor.updateRemotePeerUserFormat();
        }

        refresh();
        if (window.onRoutingChanged) window.onRoutingChanged();
    }

    void fetchLocalTransmit(InfoTablePanel::Rows & rows)
    {
        const int groups = processor.getInputGroupCount();
        for (int g = 0; g < groups; ++g) {
            int start = 0, count = 1;
            processor.getInputGroupChannelStartAndCount(g, start, count);
            String name = processor.getInputGroupName(g).trim();
            if (name.isEmpty()) name = defaultInputGroupName(g);

            String ins;
            ins << TRANS("In") << " " << (start + 1);
            if (count > 1) ins << " - " << (start + count);

            rows.cells.add(StringArray { String(g + 1).paddedLeft('0', 2) + " " + name, ins, String() });
            rows.ticks.add(!processor.getInputGroupMuted(g));
        }
    }

    void fetchPeerTransmit(InfoTablePanel::Rows & rows)
    {
        const int peer = findVisiblePeer(processor, mPeerName);
        if (peer < 0) return;

        AudioDeviceManager * adm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;
        const auto outlabels = ReceiveRouting::getOutputChannelLabels(processor, adm);

        const int groups = processor.getRemotePeerChannelGroupCount(peer);
        for (int g = 0; g < groups; ++g) {
            const auto dest = ReceiveRouting::describeStreamDestination(processor, peer, g, outlabels);
            rows.cells.add(StringArray { String(g + 1).paddedLeft('0', 2) + " " + ReceiveRouting::getStreamName(processor, peer, g),
                                         dest.isEmpty() ? String(TRANS("not patched")) : dest, String() });
            rows.ticks.add(dest.isNotEmpty());
        }
    }

    void refresh()
    {
        AudioDeviceManager * adm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;

        if (mPeerName.isEmpty()) {
            titleLabel->setText(ReceiveRouting::getLocalDeviceName(adm), dontSendNotification);
            String sub;
            sub << TRANS("This device") << "  -  " << processor.getTotalNumOutputChannels() << " " << TRANS("receive (output) channels") << ", "
                << processor.getInputGroupCount() << " " << TRANS("transmit channels");
            subtitleLabel->setText(sub, dontSendNotification);
        }
        else {
            titleLabel->setText(mPeerName, dontSendNotification);
            const int peer = findVisiblePeer(processor, mPeerName);
            if (peer < 0) {
                subtitleLabel->setText(TRANS("Not connected"), dontSendNotification);
            } else {
                String sub;
                sub << TRANS("Peer") << "  -  " << processor.getRemotePeerChannelGroupCount(peer) << " " << TRANS("streams received from it");
                subtitleLabel->setText(sub, dontSendNotification);
            }
        }

        if (receivePanel) receivePanel->refresh();
        if (transmitPanel) transmitPanel->refresh();
    }

    void paint(Graphics & g) override { g.fillAll(bgColour); }

    void resized() override
    {
        auto b = getLocalBounds();
        auto top = b.removeFromTop(52).reduced(10, 4);
        titleLabel->setBounds(top.removeFromTop(26));
        subtitleLabel->setBounds(top);
        tabs->setBounds(b);
    }

    CommsbusAudioProcessor & processor;
    String mPeerName;
    std::function<AudioDeviceManager*()> getAudioDeviceManager;
    DeviceViewWindow & window;

    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> subtitleLabel;
    std::unique_ptr<TabbedComponent> tabs;

    // a separate desktop window, so it needs its own tooltips
    TooltipWindow tooltipWindow { this, 600 };

    // owned by the tabs
    ReceivePanel * receivePanel = nullptr;
    InfoTablePanel * transmitPanel = nullptr;
};


//==============================================================================

DeviceViewWindow::DeviceViewWindow(CommsbusAudioProcessor & proc, const String & peerName,
                                   std::function<AudioDeviceManager*()> getADM)
: DocumentWindow(TRANS("Device View"), bgColour, DocumentWindow::closeButton | DocumentWindow::minimiseButton),
  mPeerName(peerName)
{
    mContent = new Content(proc, peerName, std::move(getADM), *this);

    const String devname = peerName.isEmpty() ? mContent->titleLabel->getText() : peerName;
    setName(TRANS("Device View") + " - " + devname);

    setUsingNativeTitleBar(true);
    setContentOwned(mContent, false);
    setResizable(true, false);
    setResizeLimits(560, 380, 4000, 3000);
    centreWithSize(800, 650);
    setVisible(true);
}

DeviceViewWindow::~DeviceViewWindow()
{
}

void DeviceViewWindow::closeButtonPressed()
{
    if (onCloseRequested) {
        onCloseRequested(this);
    } else {
        setVisible(false);
    }
}

void DeviceViewWindow::refresh()
{
    if (mContent) {
        mContent->refresh();
    }
}
