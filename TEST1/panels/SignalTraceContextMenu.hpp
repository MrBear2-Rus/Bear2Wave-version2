#pragma once



#include <wx/menu.h>



/**

 * Menu IDs for right-click on displayed trace names.

 * wxWidgets requires custom IDs in [1, 32766] (not 91000+).

 */

namespace SignalTraceMenu {



enum : int {

    ID_HEX = 21001,

    ID_DECIMAL,

    ID_SIGNED_DECIMAL,

    ID_BINARY,

    ID_OCTAL,

    ID_ASCII,

    ID_TIME,

    ID_ENUM,

    ID_BITS_TO_REAL,

    ID_REAL_TO_BITS,

    ID_RJ_ON,

    ID_RJ_OFF,

    ID_INVERT_ON,

    ID_INVERT_OFF,

    ID_REVERSE_ON,

    ID_REVERSE_OFF,

    ID_TRANSLATE_FILE,

    ID_TRANSLATE_PROC,

    ID_TRANSACTION_PROC,

    ID_ANALOG_FMT,

    ID_RANGE_FILL_ON,

    ID_RANGE_FILL_OFF,

    ID_GRAY_OFF,

    ID_GRAY_LIGHT,

    ID_GRAY_MEDIUM,

    ID_GRAY_STRONG,

    ID_POPCNT,

    ID_FIXED_POINT,



    ID_COLOR_NORMAL = 21040,

    ID_COLOR_RED,

    ID_COLOR_ORANGE,

    ID_COLOR_YELLOW,

    ID_COLOR_GREEN,

    ID_COLOR_CYAN,

    ID_COLOR_BLUE,

    ID_COLOR_MAGENTA,

    ID_COLOR_VIOLET,

    ID_COLOR_GRAY,

    ID_COLOR_WHITE,

    ID_COLOR_BLACK,

    ID_COLOR_CYCLE,



    ID_INSERT_ANALOG_EXT = 21060,

    ID_INSERT_BLANK,

    ID_INSERT_COMMENT,

    ID_ALIAS_TRACE,

    ID_REMOVE_ALIASES,

    ID_CUT,

    ID_COPY,

    ID_PASTE,

    ID_DELETE,

    ID_OPEN_SCOPE

};



/** Build GTKWave-style trace context menu (caller owns returned submenus via parent). */

inline void Build(wxMenu& menu)

{

    wxMenu* dataFmt = new wxMenu;

    dataFmt->Append(ID_HEX, "Hex");

    dataFmt->Append(ID_DECIMAL, "Decimal");

    dataFmt->Append(ID_SIGNED_DECIMAL, "Signed Decimal");

    dataFmt->Append(ID_BINARY, "Binary");

    dataFmt->Append(ID_OCTAL, "Octal");

    dataFmt->Append(ID_ASCII, "ASCII");

    dataFmt->Append(ID_TIME, "Time");

    dataFmt->Append(ID_ENUM, "Enum");

    dataFmt->Append(ID_BITS_TO_REAL, "BitsToReal");

    dataFmt->Append(ID_REAL_TO_BITS, "RealToBits");



    {

        wxMenu* sub = new wxMenu;

        sub->Append(ID_RJ_ON, "Enable");

        sub->Append(ID_RJ_OFF, "Disable");

        dataFmt->AppendSubMenu(sub, "Right Justify");

    }

    {

        wxMenu* sub = new wxMenu;

        sub->Append(ID_INVERT_ON, "Enable");

        sub->Append(ID_INVERT_OFF, "Disable");

        dataFmt->AppendSubMenu(sub, "Invert");

    }

    {

        wxMenu* sub = new wxMenu;

        sub->Append(ID_REVERSE_ON, "Enable");

        sub->Append(ID_REVERSE_OFF, "Disable");

        dataFmt->AppendSubMenu(sub, "Reverse Bits");

    }



    dataFmt->Append(ID_TRANSLATE_FILE, "Translate Filter File...");

    dataFmt->Append(ID_TRANSLATE_PROC, "Translate Filter Process...");

    dataFmt->Append(ID_TRANSACTION_PROC, "Transaction Filter Process");

    dataFmt->Append(ID_ANALOG_FMT, "Analog Display");

    {

        wxMenu* sub = new wxMenu;

        sub->Append(ID_RANGE_FILL_ON, "Enable");

        sub->Append(ID_RANGE_FILL_OFF, "Disable");

        dataFmt->AppendSubMenu(sub, "Range Fill");

    }

    {

        wxMenu* sub = new wxMenu;

        sub->Append(ID_GRAY_OFF, "Off");

        sub->Append(ID_GRAY_LIGHT, "Light");

        sub->Append(ID_GRAY_MEDIUM, "Medium");

        sub->Append(ID_GRAY_STRONG, "Strong");

        dataFmt->AppendSubMenu(sub, "Gray Filters");

    }

    dataFmt->Append(ID_POPCNT, "Popcnt");

    dataFmt->Append(ID_FIXED_POINT, "Fixed Point Shift...");



    menu.AppendSubMenu(dataFmt, "Data Format");



    wxMenu* colorFmt = new wxMenu;

    colorFmt->Append(ID_COLOR_NORMAL, "Normal");

    colorFmt->Append(ID_COLOR_RED, "Red");

    colorFmt->Append(ID_COLOR_ORANGE, "Orange");

    colorFmt->Append(ID_COLOR_YELLOW, "Yellow");

    colorFmt->Append(ID_COLOR_GREEN, "Green");

    colorFmt->Append(ID_COLOR_CYAN, "Cyan");

    colorFmt->Append(ID_COLOR_BLUE, "Blue");

    colorFmt->Append(ID_COLOR_MAGENTA, "Magenta");

    colorFmt->Append(ID_COLOR_VIOLET, "Violet");

    colorFmt->AppendSeparator();

    colorFmt->Append(ID_COLOR_GRAY, "Gray");

    colorFmt->Append(ID_COLOR_WHITE, "White");

    colorFmt->Append(ID_COLOR_BLACK, "Black");

    colorFmt->AppendSeparator();

    colorFmt->Append(ID_COLOR_CYCLE, "Cycle Color");

    menu.AppendSubMenu(colorFmt, "Color Format");



    menu.AppendSeparator();

    menu.Append(ID_INSERT_ANALOG_EXT, "Insert Analog Height Extension");

    menu.AppendSeparator();

    menu.Append(ID_INSERT_BLANK, "Insert Blank");

    menu.Append(ID_INSERT_COMMENT, "Insert Comment");

    menu.Append(ID_ALIAS_TRACE, "Alias Highlighted Trace");

    menu.Append(ID_REMOVE_ALIASES, "Remove Highlighted Aliases");

    menu.AppendSeparator();

    menu.Append(ID_CUT, "Cut");

    menu.Append(ID_COPY, "Copy");

    menu.Append(ID_PASTE, "Paste");

    menu.Append(ID_DELETE, "Delete");

    menu.AppendSeparator();

    menu.Append(ID_OPEN_SCOPE, "Open Scope");

}



} // namespace SignalTraceMenu

