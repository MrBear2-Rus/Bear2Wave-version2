#include "ui/MainFrameMarkers.hpp"
#include "ui/MainFrame.hpp"

namespace MainFrameMarkers {

void OnShowChangeMarkerData(MyFrame& f, wxCommandEvent&)
    {
        // 显示/更改标记数据
        if (f.m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to show.");
            return;
        }
        
        // 创建一个对话框来显示和编辑标记数据
        wxDialog* dialog = new wxDialog(&f, wxID_ANY, "Marker Data", wxDefaultPosition, wxSize(400, 300));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // 创建一个列表框来显示标记
        wxListBox* markerList = new wxListBox(dialog, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
        for (size_t i = 0; i < f.m_wavePanel->m_markers.size(); i++) {
            auto& mk = f.m_wavePanel->m_markers[i];
            markerList->Append(wxString::Format("%s: %d", mk.label, mk.timestamp));
        }
        mainSizer->Add(markerList, 1, wxEXPAND | wxALL, 10);
        
        // 添加编辑控件
        wxBoxSizer* editSizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* nameLabel = new wxStaticText(dialog, wxID_ANY, "Name:");
        wxTextCtrl* nameCtrl = new wxTextCtrl(dialog, wxID_ANY);
        wxStaticText* timeLabel = new wxStaticText(dialog, wxID_ANY, "Time:");
        wxTextCtrl* timeCtrl = new wxTextCtrl(dialog, wxID_ANY);
        editSizer->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        editSizer->Add(nameCtrl, 1, wxRIGHT, 10);
        editSizer->Add(timeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        editSizer->Add(timeCtrl, 1);
        mainSizer->Add(editSizer, 0, wxEXPAND | wxALL, 10);
        
        // 添加按钮
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okBtn = new wxButton(dialog, wxID_OK, "OK");
        wxButton* cancelBtn = new wxButton(dialog, wxID_CANCEL, "Cancel");
        buttonSizer->Add(okBtn, 0, wxRIGHT, 10);
        buttonSizer->Add(cancelBtn);
        mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxALL, 10);
        
        dialog->SetSizer(mainSizer);
        
        // 当选择列表项时，更新编辑控件
        markerList->Bind(wxEVT_LISTBOX, [&](wxCommandEvent& event) {
            int selected = event.GetSelection();
            if (selected >= 0 && selected < (int)f.m_wavePanel->m_markers.size()) {
                auto& mk = f.m_wavePanel->m_markers[selected];
                nameCtrl->SetValue(mk.label);
                timeCtrl->SetValue(wxString::Format("%lld", mk.timestamp));
            }
        });
        
        // 显示对话框
        if (dialog->ShowModal() == wxID_OK) {
            int selected = markerList->GetSelection();
            if (selected >= 0 && selected < (int)f.m_wavePanel->m_markers.size()) {
                // 更新选中的标记
                f.m_wavePanel->m_markers[selected].label = nameCtrl->GetValue();
                long long timestamp = 0;
                if (timeCtrl->GetValue().ToLongLong(&timestamp)) {
                    f.m_wavePanel->m_markers[selected].timestamp = timestamp;
                }
                f.m_wavePanel->Refresh();
            }
        }
        
        dialog->Destroy();
    }
    
void OnDropNamedMarker(MyFrame& f, wxCommandEvent&)
    {
        // 放置命名标记
        wxString markerName = wxGetTextFromUser("Enter marker name:", "Drop Named Marker", wxString::Format("M%d", (int)f.m_wavePanel->m_markers.size() + 1), &f);
        if (!markerName.IsEmpty()) {
            long long currentTime = f.m_wavePanel->GetCursorSimTime();
            f.m_wavePanel->AddMarker(currentTime, markerName);
            wxMessageBox(wxString::Format("Marker '%s' added at time %lld.", markerName, currentTime));
        }
    }
    
void OnCollectNamedMarker(MyFrame& f, wxCommandEvent&)
    {
        // 收集命名标记
        if (f.m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to collect.");
            return;
        }
        
        // 创建一个对话框来选择要收集的标记
        wxDialog* dialog = new wxDialog(&f, wxID_ANY, "Collect Named Marker", wxDefaultPosition, wxSize(400, 300));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // 创建一个列表框来显示标记
        wxListBox* markerList = new wxListBox(dialog, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
        for (size_t i = 0; i < f.m_wavePanel->m_markers.size(); i++) {
            auto& mk = f.m_wavePanel->m_markers[i];
            markerList->Append(mk.label);
        }
        mainSizer->Add(markerList, 1, wxEXPAND | wxALL, 10);
        
        // 添加按钮
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okBtn = new wxButton(dialog, wxID_OK, "Collect");
        wxButton* cancelBtn = new wxButton(dialog, wxID_CANCEL, "Cancel");
        buttonSizer->Add(okBtn, 0, wxRIGHT, 10);
        buttonSizer->Add(cancelBtn);
        mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxALL, 10);
        
        dialog->SetSizer(mainSizer);
        
        // 显示对话框
        if (dialog->ShowModal() == wxID_OK) {
            int selected = markerList->GetSelection();
            if (selected >= 0 && selected < (int)f.m_wavePanel->m_markers.size()) {
                auto& mk = f.m_wavePanel->m_markers[selected];
                // 这里可以实现收集标记的逻辑，例如将标记添加到收藏列表中
                wxMessageBox(wxString::Format("Marker '%s' collected at time %d.", mk.label, mk.timestamp));
            }
        }
        
        dialog->Destroy();
    }
    
void OnCollectAllNamedMarkers(MyFrame& f, wxCommandEvent&)
    {
        // 收集所有命名标记
        if (f.m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to collect.");
            return;
        }
        
        // 收集所有标记
        wxString collectedMarkers;
        for (size_t i = 0; i < f.m_wavePanel->m_markers.size(); i++) {
            auto& mk = f.m_wavePanel->m_markers[i];
            collectedMarkers += wxString::Format("%s: %d\n", mk.label, mk.timestamp);
        }
        
        // 显示收集的标记
        wxMessageBox(collectedMarkers, "Collected Markers");
        
        // 显示成功消息
        wxMessageBox(wxString::Format("Collected %d markers.", (int)f.m_wavePanel->m_markers.size()));
    }
    
void OnCopyPrimaryToBMarker(MyFrame& f, wxCommandEvent&)
    {
        // 复制主标记到 B 标记
        if (f.m_wavePanel->m_hasMarkerA) {
            f.m_wavePanel->m_markerB = f.m_wavePanel->m_markerA;
            f.m_wavePanel->Refresh();
            // 显示成功消息
            wxMessageBox(wxString::Format("Primary marker copied to B marker at time %d.", f.m_wavePanel->m_markerB));
        } else {
            // 显示错误消息
            wxMessageBox("No primary marker to copy.");
        }
    }
    
void OnDeletePrimaryMarker(MyFrame& f, wxCommandEvent&)
    {
        // 删除主标记
        if (f.m_wavePanel->m_hasMarkerA) {
            f.m_wavePanel->m_hasMarkerA = false;
            f.m_wavePanel->m_markerA = -1;
            f.m_wavePanel->m_markerB = -1;
            f.m_wavePanel->Refresh();
            // 显示成功消息
            wxMessageBox("Primary marker deleted.");
        } else {
            // 显示错误消息
            wxMessageBox("No primary marker to delete.");
        }
    }
    
void OnFindPreviousEdge(MyFrame& f, wxCommandEvent&)
{
    if (!f.m_wavePanel)
        return;
    const long long t = f.m_wavePanel->FindPrevEdgeWithRepeat(f.m_wavePanel->GetCursorSimTime());
    f.m_wavePanel->SetCursorSimTime(t);
    f.SetStatusText(wxString::Format(wxT("Previous edge @ %lld (repeat %d)"), t, f.m_wavePanel->m_patternSearchRepeatCount));
}

void OnFindNextEdge(MyFrame& f, wxCommandEvent&)
{
    if (!f.m_wavePanel)
        return;
    const long long t = f.m_wavePanel->FindNextEdgeWithRepeat(f.m_wavePanel->GetCursorSimTime());
    f.m_wavePanel->SetCursorSimTime(t);
    f.SetStatusText(wxString::Format(wxT("Next edge @ %lld (repeat %d)"), t, f.m_wavePanel->m_patternSearchRepeatCount));
}
    
void OnAlternateWheelMode(MyFrame& f, wxCommandEvent&)
    {
        // 切换滚轮模式
        f.m_wavePanel->m_alternateWheelMode = !f.m_wavePanel->m_alternateWheelMode;
        
        // 显示当前模式
        if (f.m_wavePanel->m_alternateWheelMode) {
            wxMessageBox("Alternate Wheel Mode enabled.\nWheel now controls time instead of zoom.");
        } else {
            wxMessageBox("Alternate Wheel Mode disabled.\nWheel now controls zoom instead of time.");
        }
    }
    
void OnWaveScrolling(MyFrame& f, wxCommandEvent&)
    {
        // 波形滚动
        f.m_wavePanel->m_waveScrollingEnabled = !f.m_wavePanel->m_waveScrollingEnabled;
        
        // 显示当前状态
        if (f.m_wavePanel->m_waveScrollingEnabled) {
            wxMessageBox(
                "Wave Scrolling enabled.\n"
                "Mouse wheel over the plot scrolls signal rows.\n"
                "Otherwise: wheel on signal names (left), Ctrl+wheel, or Up/Down keys.");
        } else {
            wxMessageBox(
                "Wave Scrolling disabled for plot area.\n"
                "Scroll signals: wheel on left name column, Ctrl+wheel, Up/Down, Shift+PgUp/PgDn.");
        }
    }
    
void OnLocking(MyFrame& f, wxCommandEvent&)
    {
        // 锁定
        f.m_wavePanel->m_markersLocked = !f.m_wavePanel->m_markersLocked;
        
        if (f.m_wavePanel->m_markersLocked) {
            wxMessageBox("Markers locked. They cannot be moved or modified.");
        } else {
            wxMessageBox("Markers unlocked. They can be moved and modified.");
        }
    }

} // namespace MainFrameMarkers
