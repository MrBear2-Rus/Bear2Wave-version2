#pragma once

#include "vcd.h"

#include <wx/dataview.h>
#include <wx/wx.h>

#include <functional>
#include <vector>

/** P4-5: virtual signal list for module tree (no wxListCtrl row cap). */
class ModuleSignalVirtualModel : public wxDataViewVirtualListModel
{
public:
    void SetRows(std::vector<signal_t*> rows, bool showFullName)
    {
        m_rows = std::move(rows);
        m_showFullName = showFullName;
        Reset(m_rows.size());
    }

    void Clear()
    {
        m_rows.clear();
        Reset(0);
    }

    signal_t* SignalAt(unsigned row) const
    {
        return row < m_rows.size() ? m_rows[row] : nullptr;
    }

    unsigned int GetCount() const override { return (unsigned int)m_rows.size(); }

    void GetValueByRow(wxVariant& variant, unsigned row, unsigned col) const override
    {
        signal_t* sig = SignalAt(row);
        if (!sig) {
            variant = wxEmptyString;
            return;
        }
        if (col == 0) {
            variant = wxString::FromUTF8("wire");
            return;
        }
        if (m_showFullName && sig->full_name[0])
            variant = wxString::FromUTF8(sig->full_name);
        else
            variant = wxString::FromUTF8(sig->name);
    }

    bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const override
    {
        (void)row;
        (void)col;
        (void)attr;
        return false;
    }

    bool SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col) override
    {
        (void)variant;
        (void)row;
        (void)col;
        return false;
    }

private:
    std::vector<signal_t*> m_rows;
    bool m_showFullName = false;
};

class ModuleSignalListView
{
public:
    void Create(wxWindow* parent)
    {
        m_model = new ModuleSignalVirtualModel();
        m_ctrl = new wxDataViewCtrl(
            parent,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_SINGLE);

        m_ctrl->AssociateModel(m_model);
        m_model->DecRef();

        m_ctrl->AppendTextColumn("Type", 0, wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
        m_ctrl->AppendTextColumn("Signals", 1, wxDATAVIEW_CELL_INERT, 160, wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    }

    wxWindow* GetWindow() const { return m_ctrl; }

    void SetSignals(const std::vector<signal_t*>& rows, bool showFullName)
    {
        if (!m_model)
            return;
        std::vector<signal_t*> copy = rows;
        m_model->SetRows(std::move(copy), showFullName);
        if (m_ctrl)
            m_ctrl->Refresh();
    }

    void Clear()
    {
        if (m_model)
            m_model->Clear();
    }

    signal_t* SelectedSignal() const
    {
        if (!m_ctrl || !m_model)
            return nullptr;
        const wxDataViewItem item = m_ctrl->GetSelection();
        if (!item.IsOk())
            return nullptr;
        const int row = (int)(intptr_t)item.GetID() - 1;
        if (row < 0)
            return nullptr;
        return m_model->SignalAt((unsigned)row);
    }

    void BindActivated(const std::function<void(signal_t*)>& handler)
    {
        if (!m_ctrl)
            return;
        m_ctrl->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this, handler](wxDataViewEvent& ev) {
            if (!m_model)
                return;
            const wxDataViewItem item = ev.GetItem();
            if (!item.IsOk())
                return;
            const int row = (int)(intptr_t)item.GetID() - 1;
            if (row < 0)
                return;
            signal_t* sig = m_model->SignalAt((unsigned)row);
            if (sig && handler)
                handler(sig);
        });
    }

private:
    wxDataViewCtrl* m_ctrl = nullptr;
    ModuleSignalVirtualModel* m_model = nullptr;
};
