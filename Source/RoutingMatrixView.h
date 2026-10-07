// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#pragma once

#include "JuceHeader.h"

#include "CommsbusAudioProcessor.h"


/**
 * Shared helpers for the Dante-Controller-style receive routing UI (the routing
 * matrix and the device view). Receivers are this machine's device output
 * channels; transmitters are the streams (channel groups) received from visible
 * peers. The patch itself is always written by the processor
 * (patchRemotePeerChannelGroupToOutput / unpatchRemotePeerChannelGroup).
 */
namespace ReceiveRouting
{
    /** "01 Out 1" style labels, one per processor output channel: the user's
        name for it (getOutputChannelUserName) if it has one, else the audio device's
        active output channel name, else "Out N". */
    juce::StringArray getOutputChannelLabels(CommsbusAudioProcessor & proc, AudioDeviceManager * adm);

    /** Output channel `outch`'s name without the user's override: the device's
        name for it, or "Out N". */
    juce::String getDefaultOutputChannelName(CommsbusAudioProcessor & proc, AudioDeviceManager * adm, int outch);

    /** The text to start a rename of `outch` from: its current name, unnumbered. */
    juce::String getOutputChannelEditName(CommsbusAudioProcessor & proc, AudioDeviceManager * adm, int outch);

    /** Renames output channel `outch`. Empty text, or the default name, removes
        the user's name. Not subject to the patching lock. */
    void renameOutputChannel(CommsbusAudioProcessor & proc, AudioDeviceManager * adm, int outch, const juce::String & text);

    /**
     * A one-line TextEditor laid over part of another component, for renaming in
     * place: Enter or clicking away commits, Esc cancels. Only one at a time.
     */
    class InlineRenameEditor
    {
    public:
        InlineRenameEditor() = default;
        ~InlineRenameEditor();

        void show(Component & parent, juce::Rectangle<int> area, const juce::String & text,
                  std::function<void(const juce::String &)> onCommit);
        void cancel() { finish(false); }
        bool isShowing() const { return editor != nullptr; }

    private:
        void finish(bool commit);

        std::unique_ptr<TextEditor> editor;
        std::function<void(const juce::String &)> commitFunc;
        bool finishing = false;

        JUCE_DECLARE_NON_COPYABLE (InlineRenameEditor)
    };

    /** The local audio device name, or a generic fallback. */
    juce::String getLocalDeviceName(AudioDeviceManager * adm);

    /** A received group's name, falling back to "Stream N". */
    juce::String getStreamName(CommsbusAudioProcessor & proc, int peer, int group);

    /** Dante style "stream@peer" for everything landing on output `outch` (all
        peers, since audio routing ignores star-network visibility). */
    juce::String describeOutputSources(CommsbusAudioProcessor & proc, int outch, int * retCount = nullptr, int * retBus = nullptr);

    /** Where a received group goes, e.g. "02 Out 2" or "Out 3 Mix -> 03 Out 3"; empty when unpatched. */
    juce::String describeStreamDestination(CommsbusAudioProcessor & proc, int peer, int group, const juce::StringArray & outputLabels);

    /** Draws the green subscription tick inside `r`. */
    void drawTick(Graphics & g, juce::Rectangle<float> r, Colour colour, float thickness = 2.0f);

    /** Drag descriptions used between the Available Channels tree and drop targets. */
    juce::String makeStreamDragDescription(int peer, int group, const juce::String & peerName);
    juce::String makePeerDragDescription(int peer, const juce::String & peerName);
    /** Parses either form. retGroup is -1 for a whole peer. False if not one of ours,
        or if the peer at that index is no longer the one named. */
    bool parseDragDescription(CommsbusAudioProcessor & proc, const var & desc, int & retPeer, int & retGroup);
}


/**
 * The receive routing matrix, laid out like Dante Controller's Routing view:
 * transmitters (received streams, grouped by peer, collapsible) as rotated
 * column headers along the top, receivers (this device's output channels,
 * grouped under the device, collapsible) as rows down the left, and a green tick
 * at each patched crosspoint. Clicking a crosspoint toggles the patch. The
 * headers stay frozen while the grid scrolls both ways.
 *
 * Polled by the editor's periodic timer (refresh()), like BusesView, so routing
 * changed from the per-stream destination menus shows up here too.
 */
class RoutingMatrixView : public Component
{
public:
    RoutingMatrixView(CommsbusAudioProcessor & proc);
    ~RoutingMatrixView() override;

    void paint(Graphics & g) override;
    void resized() override;

    /** Re-reads peers, streams, outputs and patches. Cheap when nothing moved. */
    void refresh();

    /** Natural height of the whole matrix with everything visible (headers, all
        rows, scrollbar and status line), for the editor's layout. */
    int getNaturalHeight() const;

    std::function<AudioDeviceManager*()> getAudioDeviceManager;

    /** Called after this view changed a patch, so the list view, buses panel and
        device windows can update at once. */
    std::function<void()> onRoutingChanged;

    /** Rows or columns were added or removed (natural size changed). */
    std::function<void()> onLayoutChanged;

    /** Double-click on a device header: peer index, or -1 for this device. */
    std::function<void(int peerIndex)> onOpenDeviceView;

    static constexpr int cellSize = 20;
    static constexpr int rowHeaderWidth = 210;
    static constexpr int colHeaderHeight = 150;
    static constexpr int statusHeight = 20;

private:
    struct StreamInfo {
        int peer = 0;
        int group = 0;
        juce::String name;
        // where it lands now: destStart -1 when unpatched; destBus -1 when direct
        int destStart = -1;
        int destCount = 0;
        int destBus = -1;

        bool isPatchedTo(int outch) const { return destStart >= 0 && outch >= destStart && outch < destStart + destCount; }
    };

    struct PeerInfo {
        int peer = 0;
        juce::String name;
        Array<int> streams;  // indices into mStreams
    };

    // one row or column of the matrix
    struct AxisItem {
        bool isHeader = false;
        int  owner = -1;     // columns: index into mPeers (-1 never); rows: unused
        int  index = -1;     // columns: index into mStreams; rows: output channel
    };

    class GridComponent;
    class ColumnHeader;
    class RowHeader;
    class GridViewport;

    void rebuildAxes();
    juce::String computeSignature() const;

    // drawing, called by the child components with their own coordinates
    void paintGrid(Graphics & g, juce::Rectangle<int> clip);
    void paintColumnHeader(Graphics & g, int width, int height);
    void paintRowHeader(Graphics & g, int width, int height);

    // cell state
    enum CellState { CellNone = 0, CellPatched, CellPatchedViaBus, CellSummary };
    CellState getCellState(int row, int col, juce::String * retTip = nullptr) const;

    void gridMouseMove(int px, int py);
    void gridMouseExit();
    void gridClicked(int px, int py);
    juce::String getGridTooltip() const;

    void columnHeaderMouseDown(const MouseEvent & e);
    void columnHeaderMouseMove(int px, int py);
    void rowHeaderMouseDown(const MouseEvent & e);
    void startRenamingOutput(int row);
    ReceiveRouting::InlineRenameEditor mRowRenamer;
    int mRenameViewY = 0;
    void rowHeaderMouseMove(int px, int py);
    void headerMouseExit();

    int columnAtX(int contentX) const;   // in grid content coordinates
    int rowAtY(int contentY) const;

    void setHover(int row, int col);
    void viewportMoved();
    void showStatus(const juce::String & text, bool important);

    CommsbusAudioProcessor & processor;

    Array<PeerInfo>   mPeers;
    Array<StreamInfo> mStreams;
    juce::StringArray mOutputLabels;
    juce::String      mDeviceName;

    Array<AxisItem> mCols;
    Array<AxisItem> mRows;

    // collapse state survives peers coming and going, keyed by peer name
    juce::StringArray mCollapsedPeers;
    bool mDeviceCollapsed = false;

    juce::String mLastSignature;

    int mHoverRow = -1;
    int mHoverCol = -1;

    std::unique_ptr<GridComponent> mGrid;
    std::unique_ptr<GridViewport>  mViewport;
    std::unique_ptr<ColumnHeader>  mColHeader;
    std::unique_ptr<RowHeader>     mRowHeader;

    std::unique_ptr<Label>      mTransmittersLabel;
    std::unique_ptr<Label>      mReceiversLabel;
    std::unique_ptr<TextButton> mLockButton;
    void updateLockButton();
    std::unique_ptr<TextEditor> mFilterTxEditor;
    std::unique_ptr<TextEditor> mFilterRxEditor;
    std::unique_ptr<Label>      mStatusLabel;
    uint32 mStatusStamp = 0;

    Colour bgColor;
    Colour headerBgColor;
    Colour deviceHeaderBgColor;
    Colour gridLineColor;
    Colour textColor;
    Colour dimTextColor;
    Colour accentColor;
    Colour tickColor;
    Colour hoverColor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingMatrixView)
};
