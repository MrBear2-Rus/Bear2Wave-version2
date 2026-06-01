#pragma once

/** Central menu / toolbar command IDs (non-overlapping ranges). */
namespace BearMenuId {

namespace File {
inline constexpr int OpenNewWindow = 1001;
inline constexpr int OpenNewTab = 1002;
inline constexpr int OpenNewLab = 1003;
inline constexpr int ReloadWaveform = 1004;
inline constexpr int Close = 1015;
inline constexpr int PrintToFile = 1005;
inline constexpr int GrabToFile = 1006;
inline constexpr int ReadSession = 1007;
inline constexpr int WriteSession = 1008;
inline constexpr int WriteSessionAs = 1009;
inline constexpr int ReadSimLogfile = 1010;
inline constexpr int ReadVerilogStemsfile = 1011;
inline constexpr int ReadTclScript = 1012;
inline constexpr int Quit = 1013;
inline constexpr int OpenTrace = 1014;
inline constexpr int ConvertTrace = 1016;
} // namespace File

namespace Export {
inline constexpr int AsciiText = 1101;
inline constexpr int Vcd = 1102;
inline constexpr int Csv = 1103;
inline constexpr int PostScript = 1104;
inline constexpr int Png = 1105;
inline constexpr int Svg = 1106;
inline constexpr int Mif = 1107;
} // namespace Export

namespace Measure {
inline constexpr int ShowMeasurement = 2001;
} // namespace Measure

namespace Time {
inline constexpr int MoveToTime = 7001;
} // namespace Time

namespace Zoom {
inline constexpr int In = 7101;
inline constexpr int Out = 7102;
inline constexpr int Full = 7103;
inline constexpr int Last = 7104;
} // namespace Zoom

namespace Fetch {
inline constexpr int More = 7201;
inline constexpr int All = 7202;
} // namespace Fetch

namespace Discard {
inline constexpr int ToStart = 7301;
inline constexpr int ToEnd = 7302;
} // namespace Discard

namespace Shift {
inline constexpr int Left = 7401;
inline constexpr int Right = 7402;
} // namespace Shift

namespace Page {
inline constexpr int Left = 7501;
inline constexpr int Right = 7502;
} // namespace Page

namespace Edit {
inline constexpr int SetTraceMaxHier = 8001;
inline constexpr int ToggleTraceHier = 8002;
inline constexpr int InsertBlank = 8003;
inline constexpr int InsertComment = 8004;
inline constexpr int InsertAnalogHeight = 8005;
inline constexpr int Cut = 8006;
inline constexpr int Copy = 8007;
inline constexpr int Paste = 8008;
inline constexpr int Delete = 8009;
inline constexpr int AliasHighlighted = 8010;
inline constexpr int RemoveAliases = 8011;
inline constexpr int Expand = 8012;
inline constexpr int CombineDown = 8013;
inline constexpr int CombineUp = 8014;
inline constexpr int ShowChangeAllHighlighted = 8015;
inline constexpr int ShowChangeAll = 8016;
inline constexpr int Exclude = 8017;
inline constexpr int Show = 8018;
inline constexpr int ToggleGroup = 8019;
inline constexpr int CreateGroup = 8020;
inline constexpr int HighlightRegexp = 8021;
inline constexpr int HighlightAll = 8022;
    inline constexpr int UnHighlightAll = 8023;
    inline constexpr int ExternalToolPaths = 8024;
} // namespace Edit

namespace DataFormat {
inline constexpr int Binary = 8101;
inline constexpr int Octal = 8102;
inline constexpr int Decimal = 8103;
inline constexpr int Hexadecimal = 8104;
inline constexpr int Ascii = 8105;
inline constexpr int SignedDecimal = 8106;
inline constexpr int Real = 8107;
inline constexpr int ApplyAll = 8108;
} // namespace DataFormat

namespace ColorFormat {
inline constexpr int Default = 8201;
inline constexpr int SignalName = 8202;
inline constexpr int Value = 8203;
inline constexpr int Module = 8204;
} // namespace ColorFormat

namespace TimeWarp {
inline constexpr int Enable = 8301;
inline constexpr int Disable = 8302;
inline constexpr int Set = 8303;
} // namespace TimeWarp

namespace Sort {
inline constexpr int ByName = 8401;
inline constexpr int ByGroup = 8402;
inline constexpr int ByValue = 8403;
inline constexpr int ByModule = 8404;
} // namespace Sort

namespace View {
inline constexpr int DebugLog = 8601;
inline constexpr int FstVerbose = 8602;
inline constexpr int WaveformSummary = 8603;
inline constexpr int ExportDiagnostics = 8604;
inline constexpr int RemovePatternMarks = 8605;
inline constexpr int ShowSimLog = 8606;
inline constexpr int ThemeLight = 8607;
inline constexpr int ThemeDark = 8608;
} // namespace View

namespace Search {
inline constexpr int PatternSearch = 8801;
inline constexpr int SetRepeatCount = 8802;
inline constexpr int PatternFindNext = 8803;
inline constexpr int PatternFindPrev = 8804;
} // namespace Search

namespace Markers {
inline constexpr int ShowChangeMarkerData = 8501;
inline constexpr int DropNamedMarker = 8502;
inline constexpr int CollectNamedMarker = 8503;
inline constexpr int CollectAllNamedMarkers = 8504;
inline constexpr int CopyPrimaryToB = 8505;
inline constexpr int DeletePrimaryMarker = 8506;
inline constexpr int FindPreviousEdge = 8507;
inline constexpr int FindNextEdge = 8508;
inline constexpr int AlternateWheelMode = 8509;
inline constexpr int WaveScrolling = 8510;
inline constexpr int Locking = 8511;
} // namespace Markers

namespace AI {
inline constexpr int TogglePanel = 9001;
inline constexpr int SetApiKey = 9002;
} // namespace AI

namespace Compare {
inline constexpr int OpenSecond = 9501;
inline constexpr int LinkPlayheads = 9502;
inline constexpr int LinkTimeView = 9503;
inline constexpr int TileHorizontally = 9504;
} // namespace Compare

namespace Help {
inline constexpr int Contents = 9701;
inline constexpr int Shortcuts = 9702;
inline constexpr int Environment = 9703;
inline constexpr int About = 9704;
} // namespace Help

namespace ModuleTree {
inline constexpr int AddAllSignals = 6001;
} // namespace ModuleTree

} // namespace BearMenuId
