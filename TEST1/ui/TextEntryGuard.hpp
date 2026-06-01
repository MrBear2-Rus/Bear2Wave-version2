#pragma once

#include <wx/combobox.h>
#include <wx/textctrl.h>
#include <wx/window.h>

/** True when keyboard focus is in a control that accepts typed text. */
inline bool IsTextEntryFocused()
{
    for (wxWindow* w = wxWindow::FindFocus(); w; w = w->GetParent()) {
        if (dynamic_cast<wxTextCtrl*>(w))
            return true;
        if (dynamic_cast<wxComboBox*>(w))
            return true;
    }
    return false;
}

/** Prevent frame-level menu accelerators from stealing keys while typing. */
inline void InstallTextEntryAcceleratorShield(wxWindow* ctrl)
{
    if (!ctrl)
        return;
    ctrl->Bind(wxEVT_CHAR_HOOK, [](wxKeyEvent& e) { e.Skip(); });
}
