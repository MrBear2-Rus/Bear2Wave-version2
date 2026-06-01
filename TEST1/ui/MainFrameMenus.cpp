#include "ui/MainFrameMenus.hpp"

#include "ui/ExternalToolsSettingsDialog.hpp"
#include "ui/HelpDialog.hpp"
#include "ui/MainFrame.hpp"
#include "ui/MenuIds.hpp"
#include "ui/trace_file_filters.h"
#include "ui/WaveformCompareHub.h"
#include "core/trace_format_convert.h"

#include <wx/accel.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>

namespace MainFrameMenus {

using namespace BearMenuId;

namespace {

void InstallFrameAccelerators(MyFrame& frame)
{
    wxAcceleratorEntry entries[] = {
        wxAcceleratorEntry(wxACCEL_CTRL | wxACCEL_SHIFT, (int)'C', Compare::OpenSecond),
    };
    wxAcceleratorTable table(WXSIZEOF(entries), entries);
    frame.SetAcceleratorTable(table);
}

template<typename Method>
void BindMenu(wxMenuBar* bar, MyFrame& frame, int id, Method method)
{
    if (bar)
        bar->Bind(wxEVT_MENU, method, &frame, id);
    else
        frame.Bind(wxEVT_MENU, method, &frame, id);
}

} // namespace

void CreateMenuBar(MyFrame& frame)
{
    auto* fileMenu = new wxMenu;
    fileMenu->Append(File::OpenNewWindow, "Open New Window\tCtrl+N");
    fileMenu->Append(File::OpenNewTab, "Open New Tab\tCtrl+T");
    fileMenu->Append(File::OpenNewLab, "Open New Lab\tCtrl+L");
    fileMenu->Append(File::ReloadWaveform, "Reload Waveform\tShift+Ctrl+R");
    fileMenu->AppendSeparator();
    fileMenu->Append(File::ConvertTrace, "Convert Trace...");

    auto* exportSubMenu = new wxMenu;
    exportSubMenu->Append(Export::AsciiText, "ASCII Text");
    exportSubMenu->Append(Export::Vcd, "VCD");
    exportSubMenu->Append(Export::Csv, "CSV");
    exportSubMenu->Append(Export::PostScript, "PostScript");
    exportSubMenu->Append(Export::Mif, "FrameMaker (MIF)");
    exportSubMenu->Append(Export::Png, "PNG");
    exportSubMenu->Append(Export::Svg, "SVG");
    fileMenu->AppendSubMenu(exportSubMenu, "Export");

    fileMenu->Append(File::Close, "Close\tCtrl+W");
    fileMenu->AppendSeparator();
    fileMenu->Append(File::PrintToFile, "Print To File\tCtrl+P");
    fileMenu->Append(File::GrabToFile, "Grab To File");
    fileMenu->AppendSeparator();
    fileMenu->Append(File::ReadSession, "Read Session (.gtkw/.b2w)");
    fileMenu->Append(File::WriteSession, "Write Session\tCtrl+S");
    fileMenu->Append(File::WriteSessionAs, "Write Session As\tShift+Ctrl+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(File::ReadSimLogfile, "Read Sim Logfile\tCtrl+G");
    fileMenu->Append(File::ReadVerilogStemsfile, "Read Verilog Stemsfile");
    fileMenu->Append(File::ReadTclScript, "Read Tcl Script File");
    fileMenu->AppendSeparator();
    fileMenu->Append(File::Quit, "Quit\tCtrl+Q");

    auto* editMenu = new wxMenu;
    editMenu->Append(Edit::SetTraceMaxHier, "Set Trace Max Hier");
    editMenu->Append(Edit::ToggleTraceHier, "Toggle Trace Hier");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::InsertBlank, "Insert Blank\tCtrl+B");
    editMenu->Append(Edit::InsertComment, "Insert Comment");
    editMenu->Append(Edit::InsertAnalogHeight, "Insert Analog Height Extension");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::Cut, "Cut\tCtrl+X");
    editMenu->Append(Edit::Copy, "Copy\tCtrl+C");
    editMenu->Append(Edit::Paste, "Paste\tCtrl+V");
    editMenu->Append(Edit::Delete, "Delete\tCtrl+Delete");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::AliasHighlighted, "Alias Highlighted Trace\tAlt+A");
    editMenu->Append(Edit::RemoveAliases, "Remove Highlighted Aliases\tShift+Alt+A");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::Expand, "Expand");
    editMenu->Append(Edit::CombineDown, "Combine Down\tF3");
    editMenu->Append(Edit::CombineUp, "Combine Up\tF5");
    editMenu->AppendSeparator();

    auto* dataFormatSubMenu = new wxMenu;
    dataFormatSubMenu->Append(DataFormat::Binary, "Binary");
    dataFormatSubMenu->Append(DataFormat::Octal, "Octal");
    dataFormatSubMenu->Append(DataFormat::Decimal, "Decimal");
    dataFormatSubMenu->Append(DataFormat::Hexadecimal, "Hexadecimal");
    dataFormatSubMenu->Append(DataFormat::Ascii, "ASCII");
    dataFormatSubMenu->Append(DataFormat::SignedDecimal, "Signed Decimal");
    dataFormatSubMenu->Append(DataFormat::Real, "Real");
    dataFormatSubMenu->AppendSeparator();
    dataFormatSubMenu->Append(DataFormat::ApplyAll, "Apply to All Displayed Traces");
    editMenu->AppendSubMenu(dataFormatSubMenu, "Data Format");

    auto* colorFormatSubMenu = new wxMenu;
    colorFormatSubMenu->Append(ColorFormat::Default, "Default");
    colorFormatSubMenu->Append(ColorFormat::SignalName, "Signal Name");
    colorFormatSubMenu->Append(ColorFormat::Value, "Value");
    colorFormatSubMenu->Append(ColorFormat::Module, "Module");
    editMenu->AppendSubMenu(colorFormatSubMenu, "Color Format");

    editMenu->Append(Edit::ShowChangeAllHighlighted, "Show-Change All Highlighted\tCtrl+F");
    editMenu->Append(Edit::ShowChangeAll, "Show-Change All");
    editMenu->AppendSeparator();

    auto* timeWarpSubMenu = new wxMenu;
    timeWarpSubMenu->Append(TimeWarp::Enable, "Enable");
    timeWarpSubMenu->Append(TimeWarp::Disable, "Disable");
    timeWarpSubMenu->Append(TimeWarp::Set, "Set");
    editMenu->AppendSubMenu(timeWarpSubMenu, "Time Warp");

    editMenu->Append(Edit::Exclude, "Exclude\tShift+Alt+E");
    editMenu->Append(Edit::Show, "Show\tShift+Alt+S");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::ToggleGroup, "Toggle Group Open/Close\tCtrl+Shift+T");
    editMenu->Append(Edit::CreateGroup, "Create Group\tAlt+R");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::HighlightRegexp, "Highlight Regexp");
    editMenu->Append(Edit::HighlightAll, "Highlight All\tCtrl+A");
    editMenu->Append(Edit::UnHighlightAll, "UnHighlight All\tShift+Ctrl+A");
    editMenu->AppendSeparator();
    editMenu->Append(Edit::ExternalToolPaths, "External Tool Paths...");
    editMenu->AppendSeparator();

    auto* sortSubMenu = new wxMenu;
    sortSubMenu->Append(Sort::ByName, "By Name");
    sortSubMenu->Append(Sort::ByGroup, "By Group");
    sortSubMenu->Append(Sort::ByValue, "By Value");
    sortSubMenu->Append(Sort::ByModule, "By Module");
    editMenu->AppendSubMenu(sortSubMenu, "Sort");

    auto* timeMenu = new wxMenu;
    timeMenu->Append(Time::MoveToTime, "Move To Time\tF1");
    timeMenu->AppendSeparator();

    auto* zoomSubMenu = new wxMenu;
    zoomSubMenu->Append(Zoom::In, "Zoom In");
    zoomSubMenu->Append(Zoom::Out, "Zoom Out");
    zoomSubMenu->Append(Zoom::Full, "Zoom Full");
    zoomSubMenu->Append(Zoom::Last, "Zoom Last");
    timeMenu->AppendSubMenu(zoomSubMenu, "Zoom");

    auto* fetchSubMenu = new wxMenu;
    fetchSubMenu->Append(Fetch::More, "Fetch More");
    fetchSubMenu->Append(Fetch::All, "Fetch All");
    timeMenu->AppendSubMenu(fetchSubMenu, "Fetch");

    auto* discardSubMenu = new wxMenu;
    discardSubMenu->Append(Discard::ToStart, "Discard To Start");
    discardSubMenu->Append(Discard::ToEnd, "Discard To End");
    timeMenu->AppendSubMenu(discardSubMenu, "Discard");

    auto* shiftSubMenu = new wxMenu;
    shiftSubMenu->Append(Shift::Left, "Shift Left");
    shiftSubMenu->Append(Shift::Right, "Shift Right");
    timeMenu->AppendSubMenu(shiftSubMenu, "Shift");

    auto* pageSubMenu = new wxMenu;
    pageSubMenu->Append(Page::Left, "Page Left\tPgUp");
    pageSubMenu->Append(Page::Right, "Page Right\tPgDn");
    timeMenu->AppendSubMenu(pageSubMenu, "Page");

    auto* markersMenu = new wxMenu;
    markersMenu->Append(Markers::ShowChangeMarkerData, "Show-Change Marker Data\tAlt+M");
    markersMenu->Append(Markers::DropNamedMarker, "Drop Named Marker\tAlt+H");
    markersMenu->Append(Markers::CollectNamedMarker, "Collect Named Marker\tShift+Alt+H");
    markersMenu->Append(Markers::CollectAllNamedMarkers, "Collect All Named Markers\tShift+Ctrl+Alt+H");
    markersMenu->Append(Markers::CopyPrimaryToB, "Copy Primary->B Marker\tCtrl+Shift+B");
    markersMenu->Append(Markers::DeletePrimaryMarker, "Delete Primary Marker\tShift+Alt+M");
    markersMenu->AppendSeparator();
    markersMenu->Append(Markers::FindPreviousEdge, "Find Previous Edge");
    markersMenu->Append(Markers::FindNextEdge, "Find Next Edge");
    markersMenu->AppendSeparator();
    markersMenu->AppendCheckItem(Markers::AlternateWheelMode, "Alternate Wheel Mode");
    markersMenu->AppendCheckItem(Markers::WaveScrolling, "Wave Scrolling\tF9");
    markersMenu->AppendCheckItem(Markers::Locking, "Locking");

    auto* measureMenu = new wxMenu;
    measureMenu->Append(Measure::ShowMeasurement, "Show Measurement");

    auto* searchMenu = new wxMenu;
    searchMenu->Append(Search::PatternSearch, "Pattern Search...\tCtrl+Shift+P");
    searchMenu->Append(View::RemovePatternMarks, "Remove Pattern Marks");
    searchMenu->AppendSeparator();
    searchMenu->Append(Search::SetRepeatCount, "Set Pattern Search Repeat Count...");
    searchMenu->Append(Search::PatternFindNext, "Pattern Find Next\tCtrl+Shift+N");
    searchMenu->Append(Search::PatternFindPrev, "Pattern Find Previous\tCtrl+Shift+U");

    auto* compareMenu = new wxMenu;
    compareMenu->Append(Compare::OpenSecond, "Open in New Window...\tCtrl+Shift+C");
    compareMenu->AppendSeparator();
    compareMenu->AppendCheckItem(Compare::LinkPlayheads, "Link Playheads Across Windows");
    compareMenu->AppendCheckItem(Compare::LinkTimeView, "Link Time View Across Windows");
    compareMenu->AppendSeparator();
    compareMenu->Append(Compare::TileHorizontally, "Tile Windows Horizontally");

    auto* viewMenu = new wxMenu;
    viewMenu->AppendCheckItem(View::DebugLog, "Debug log window\tCtrl+Shift+L");
    viewMenu->AppendCheckItem(View::FstVerbose, "Verbose FST load to log window");
    viewMenu->AppendSeparator();
    auto* themeMenu = new wxMenu;
    themeMenu->AppendCheckItem(View::ThemeLight, "Light");
    themeMenu->AppendCheckItem(View::ThemeDark, "Dark");
    viewMenu->AppendSubMenu(themeMenu, "Theme");
    viewMenu->AppendSeparator();
    viewMenu->Append(View::WaveformSummary, "Dump waveform / trace summary");
    viewMenu->Append(View::ShowSimLog, "Show Simulation Log");
    viewMenu->Append(View::ExportDiagnostics, "Export diagnostics bundle...\tCtrl+Shift+D");

    auto* aiMenu = new wxMenu;
    aiMenu->Append(AI::TogglePanel, "Toggle AI Panel");
    aiMenu->Append(AI::SetApiKey, "Set API Key");

    auto* helpMenu = new wxMenu;
    helpMenu->Append(Help::Contents, "Help Contents\tCtrl+F1");
    helpMenu->Append(Help::Shortcuts, "Shortcuts");
    helpMenu->Append(Help::Environment, "Environment Variables");

    auto* mb = new wxMenuBar;
    mb->Append(fileMenu, "File");
    mb->Append(editMenu, "Edit");
    mb->Append(timeMenu, "Time");
    mb->Append(markersMenu, "Markers");
    mb->Append(searchMenu, "Search");
    mb->Append(measureMenu, "Measure");
    mb->Append(compareMenu, "Compare");
    mb->Append(viewMenu, "View");
    mb->Append(aiMenu, "AI");
    mb->Append(helpMenu, "Help");
    frame.SetMenuBar(mb);
}

void BindMenuEvents(MyFrame& frame)
{
    wxMenuBar* bar = frame.GetMenuBar();

    BindMenu(bar, frame, File::OpenNewWindow, &MyFrame::OnOpenNewWindow);
    BindMenu(bar, frame, File::OpenNewTab, &MyFrame::OnOpenNewTab);
    BindMenu(bar, frame, File::OpenNewLab, &MyFrame::OnOpenNewLab);
    BindMenu(bar, frame, File::ReloadWaveform, &MyFrame::OnReloadWaveform);
    BindMenu(bar, frame, File::ConvertTrace, &MyFrame::OnConvertTrace);
    BindMenu(bar, frame, File::Close, &MyFrame::OnClose);
    BindMenu(bar, frame, File::PrintToFile, &MyFrame::OnPrintToFile);
    BindMenu(bar, frame, File::GrabToFile, &MyFrame::OnGrabToFile);
    BindMenu(bar, frame, File::ReadSession, &MyFrame::OnReadSaveFile);
    BindMenu(bar, frame, File::WriteSession, &MyFrame::OnWriteSaveFile);
    BindMenu(bar, frame, File::WriteSessionAs, &MyFrame::OnWriteSaveFileAs);
    BindMenu(bar, frame, File::ReadSimLogfile, &MyFrame::OnReadSimLogfile);
    BindMenu(bar, frame, File::ReadVerilogStemsfile, &MyFrame::OnReadVerilogStemsfile);
    BindMenu(bar, frame, File::ReadTclScript, &MyFrame::OnReadTclScriptFile);
    BindMenu(bar, frame, File::Quit, &MyFrame::OnQuit);

    BindMenu(bar, frame, Export::AsciiText, &MyFrame::OnExportAsciiText);
    BindMenu(bar, frame, Export::Vcd, &MyFrame::OnExportVCD);
    BindMenu(bar, frame, Export::Csv, &MyFrame::OnExportCSV);
    BindMenu(bar, frame, Export::PostScript, &MyFrame::OnExportPostScript);
    BindMenu(bar, frame, Export::Mif, &MyFrame::OnExportMif);
    BindMenu(bar, frame, Export::Png, &MyFrame::OnExportPNG);
    BindMenu(bar, frame, Export::Svg, &MyFrame::OnExportSVG);

    BindMenu(bar, frame, Edit::SetTraceMaxHier, &MyFrame::OnSetTraceMaxHier);
    BindMenu(bar, frame, Edit::ToggleTraceHier, &MyFrame::OnToggleTraceHier);
    BindMenu(bar, frame, Edit::InsertBlank, &MyFrame::OnInsertBlank);
    BindMenu(bar, frame, Edit::InsertComment, &MyFrame::OnInsertComment);
    BindMenu(bar, frame, Edit::InsertAnalogHeight, &MyFrame::OnInsertAnalogHeightExtension);
    BindMenu(bar, frame, Edit::Cut, &MyFrame::OnCut);
    BindMenu(bar, frame, Edit::Copy, &MyFrame::OnCopy);
    BindMenu(bar, frame, Edit::Paste, &MyFrame::OnPaste);
    BindMenu(bar, frame, Edit::Delete, &MyFrame::OnDelete);
    BindMenu(bar, frame, Edit::AliasHighlighted, &MyFrame::OnAliasHighlightedTrace);
    BindMenu(bar, frame, Edit::RemoveAliases, &MyFrame::OnRemoveHighlightedAliases);
    BindMenu(bar, frame, Edit::Expand, &MyFrame::OnExpand);
    BindMenu(bar, frame, Edit::CombineDown, &MyFrame::OnCombineDown);
    BindMenu(bar, frame, Edit::CombineUp, &MyFrame::OnCombineUp);
    BindMenu(bar, frame, Edit::ShowChangeAllHighlighted, &MyFrame::OnShowChangeAllHighlighted);
    BindMenu(bar, frame, Edit::ShowChangeAll, &MyFrame::OnShowChangeAll);
    BindMenu(bar, frame, Edit::Exclude, &MyFrame::OnExclude);
    BindMenu(bar, frame, Edit::Show, &MyFrame::OnShow);
    BindMenu(bar, frame, Edit::ToggleGroup, &MyFrame::OnToggleGroupOpenClose);
    BindMenu(bar, frame, Edit::CreateGroup, &MyFrame::OnCreateGroup);
    BindMenu(bar, frame, Edit::HighlightRegexp, &MyFrame::OnHighlightRegexp);
    BindMenu(bar, frame, Edit::HighlightAll, &MyFrame::OnHighlightAll);
    BindMenu(bar, frame, Edit::UnHighlightAll, &MyFrame::OnUnHighlightAll);
    BindMenu(bar, frame, Edit::ExternalToolPaths, &MyFrame::OnExternalToolPaths);

    BindMenu(bar, frame, DataFormat::Binary, &MyFrame::OnDataFormatBinary);
    BindMenu(bar, frame, DataFormat::Octal, &MyFrame::OnDataFormatOctal);
    BindMenu(bar, frame, DataFormat::Decimal, &MyFrame::OnDataFormatDecimal);
    BindMenu(bar, frame, DataFormat::Hexadecimal, &MyFrame::OnDataFormatHexadecimal);
    BindMenu(bar, frame, DataFormat::Ascii, &MyFrame::OnDataFormatASCII);
    BindMenu(bar, frame, DataFormat::SignedDecimal, &MyFrame::OnDataFormatSignedDecimal);
    BindMenu(bar, frame, DataFormat::Real, &MyFrame::OnDataFormatReal);
    BindMenu(bar, frame, DataFormat::ApplyAll, &MyFrame::OnDataFormatApplyAll);

    BindMenu(bar, frame, ColorFormat::Default, &MyFrame::OnColorFormatDefault);
    BindMenu(bar, frame, ColorFormat::SignalName, &MyFrame::OnColorFormatSignalName);
    BindMenu(bar, frame, ColorFormat::Value, &MyFrame::OnColorFormatValue);
    BindMenu(bar, frame, ColorFormat::Module, &MyFrame::OnColorFormatModule);

    BindMenu(bar, frame, TimeWarp::Enable, &MyFrame::OnTimeWarpEnable);
    BindMenu(bar, frame, TimeWarp::Disable, &MyFrame::OnTimeWarpDisable);
    BindMenu(bar, frame, TimeWarp::Set, &MyFrame::OnTimeWarpSet);

    BindMenu(bar, frame, Sort::ByName, &MyFrame::OnSortByName);
    BindMenu(bar, frame, Sort::ByGroup, &MyFrame::OnSortByGroup);
    BindMenu(bar, frame, Sort::ByValue, &MyFrame::OnSortByValue);
    BindMenu(bar, frame, Sort::ByModule, &MyFrame::OnSortByModule);

    BindMenu(bar, frame, Time::MoveToTime, &MyFrame::OnMoveToTime);
    BindMenu(bar, frame, Zoom::In, &MyFrame::OnZoomIn);
    BindMenu(bar, frame, Zoom::Out, &MyFrame::OnZoomOut);
    BindMenu(bar, frame, Zoom::Full, &MyFrame::OnZoomFull);
    BindMenu(bar, frame, Zoom::Last, &MyFrame::OnZoomLast);
    BindMenu(bar, frame, Fetch::More, &MyFrame::OnFetchMore);
    BindMenu(bar, frame, Fetch::All, &MyFrame::OnFetchAll);
    BindMenu(bar, frame, Discard::ToStart, &MyFrame::OnDiscardToStart);
    BindMenu(bar, frame, Discard::ToEnd, &MyFrame::OnDiscardToEnd);
    BindMenu(bar, frame, Shift::Left, &MyFrame::OnShiftLeft);
    BindMenu(bar, frame, Shift::Right, &MyFrame::OnShiftRight);
    BindMenu(bar, frame, Page::Left, &MyFrame::OnPageLeft);
    BindMenu(bar, frame, Page::Right, &MyFrame::OnPageRight);

    BindMenu(bar, frame, Markers::ShowChangeMarkerData, &MyFrame::OnShowChangeMarkerData);
    BindMenu(bar, frame, Markers::DropNamedMarker, &MyFrame::OnDropNamedMarker);
    BindMenu(bar, frame, Markers::CollectNamedMarker, &MyFrame::OnCollectNamedMarker);
    BindMenu(bar, frame, Markers::CollectAllNamedMarkers, &MyFrame::OnCollectAllNamedMarkers);
    BindMenu(bar, frame, Markers::CopyPrimaryToB, &MyFrame::OnCopyPrimaryToBMarker);
    BindMenu(bar, frame, Markers::DeletePrimaryMarker, &MyFrame::OnDeletePrimaryMarker);
    BindMenu(bar, frame, Markers::FindPreviousEdge, &MyFrame::OnFindPreviousEdge);
    BindMenu(bar, frame, Markers::FindNextEdge, &MyFrame::OnFindNextEdge);
    BindMenu(bar, frame, Markers::AlternateWheelMode, &MyFrame::OnAlternateWheelMode);
    BindMenu(bar, frame, Markers::WaveScrolling, &MyFrame::OnWaveScrolling);
    BindMenu(bar, frame, Markers::Locking, &MyFrame::OnLocking);

    BindMenu(bar, frame, Measure::ShowMeasurement, &MyFrame::OnShowMeasure);

    BindMenu(bar, frame, Search::PatternSearch, &MyFrame::OnPatternSearch);
    BindMenu(bar, frame, View::RemovePatternMarks, &MyFrame::OnRemovePatternMarks);
    BindMenu(bar, frame, Search::SetRepeatCount, &MyFrame::OnSetPatternRepeatCount);
    BindMenu(bar, frame, Search::PatternFindNext, &MyFrame::OnPatternFindNext);
    BindMenu(bar, frame, Search::PatternFindPrev, &MyFrame::OnPatternFindPrev);

    BindMenu(bar, frame, View::DebugLog, &MyFrame::OnViewDebugLog);
    BindMenu(bar, frame, View::FstVerbose, &MyFrame::OnViewFstVerbose);
    BindMenu(bar, frame, View::ThemeLight, &MyFrame::OnThemeLight);
    BindMenu(bar, frame, View::ThemeDark, &MyFrame::OnThemeDark);
    BindMenu(bar, frame, View::WaveformSummary, &MyFrame::OnViewWaveformSummary);
    BindMenu(bar, frame, View::ShowSimLog, &MyFrame::OnViewShowSimLog);
    BindMenu(bar, frame, View::ExportDiagnostics, &MyFrame::OnExportDiagnostics);

    BindMenu(bar, frame, AI::TogglePanel, &MyFrame::OnToggleAIPanel);
    BindMenu(bar, frame, AI::SetApiKey, &MyFrame::OnSetAPIKey);

    BindMenu(bar, frame, Compare::OpenSecond, &MyFrame::OnCompareOpenSecond);
    BindMenu(bar, frame, Compare::LinkPlayheads, &MyFrame::OnCompareLinkPlayheads);
    BindMenu(bar, frame, Compare::LinkTimeView, &MyFrame::OnCompareLinkTimeView);
    BindMenu(bar, frame, Compare::TileHorizontally, &MyFrame::OnCompareTileWindows);

    BindMenu(bar, frame, Help::Contents, &MyFrame::OnHelpContents);
    BindMenu(bar, frame, Help::Shortcuts, &MyFrame::OnHelpShortcuts);
    BindMenu(bar, frame, Help::Environment, &MyFrame::OnHelpEnvironment);

    InstallFrameAccelerators(frame);
    frame.Bind(wxEVT_CHAR_HOOK, &MyFrame::OnFrameCharHook, &frame);

    WaveformCompare::RegisterFrame(&frame);
    WaveformCompare::SyncThemeMenuChecks();
}

} // namespace MainFrameMenus

void MyFrame::OnExternalToolPaths(wxCommandEvent&)
{
    ExternalToolsSettingsDialog dlg(this);
    dlg.CentreOnParent();
    dlg.ShowModal();
}

void MyFrame::OnConvertTrace(wxCommandEvent&)
{
    wxFileDialog openDlg(
        this,
        wxT("选择源波形文件"),
        wxEmptyString,
        wxEmptyString,
        Bear2WaveTraceUi::OpenTraceDialogFilter(),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openDlg.ShowModal() != wxID_OK)
        return;

    wxString saveFilter = wxT(
        "FST (*.fst)|*.fst|"
        "VCD (*.vcd)|*.vcd|"
        "LXT2 (*.lxt2)|*.lxt2|"
        "LXT (*.lxt)|*.lxt|"
        "VZT (*.vzt)|*.vzt|"
        "All files (*.*)|*.*");

    wxFileDialog saveDlg(
        this,
        wxT("保存转换结果"),
        wxEmptyString,
        wxEmptyString,
        saveFilter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() != wxID_OK)
        return;

    const wxString src = openDlg.GetPath();
    const wxString dst = saveDlg.GetPath();

    wxProgressDialog progress(
        wxT("转换波形"),
        wxT("正在转换，请稍候…"),
        100,
        this,
        wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH);
    progress.Pulse();
    wxSafeYield(this);

    char err[768] = {};
    int rc = -1;
    {
        wxBusyCursor busy;
        rc = trace_convert_path(src.utf8_string().c_str(), dst.utf8_string().c_str(), err, sizeof(err));
    }
    progress.Destroy();

    if (rc != 0) {
        wxString msg = wxString::FromUTF8(err);
        if (msg.empty())
            msg = wxT("转换失败。");
        Raise();
        SetFocus();
        wxMessageBox(msg, wxT("转换失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    // Stay on the current trace view; do not open/reload the converted file.
    Raise();
    SetFocus();
    wxMessageBox(wxT("已成功转换"), wxT("转换完成"), wxOK | wxICON_INFORMATION, this);
}
