#include "ProjectStartWindow.h"

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/activityindicator.h>
#include <wx/file.h>
#include <wx/xml/xml.h>
#include <wx/mstream.h>

#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/dirdlg.h>
#include <wx/filefn.h>
#include <wx/dir.h>

// 事件ID
enum
{
    ID_BTN_OPEN_PROJECT = wxID_HIGHEST + 101,
    ID_BTN_EXIT,
    ID_LIST_RECENT_PROJECTS
};

// 事件表
wxBEGIN_EVENT_TABLE(ProjectStartWindow, wxDialog)
EVT_BUTTON(ID_BTN_OPEN_PROJECT, ProjectStartWindow::OnOpenProject)
EVT_BUTTON(ID_BTN_EXIT, ProjectStartWindow::OnExit)
EVT_LIST_ITEM_ACTIVATED(ID_LIST_RECENT_PROJECTS, ProjectStartWindow::OnRecentProjectDblClick)
wxEND_EVENT_TABLE()

// 构造函数：启动时加载配置文件中的历史记录
ProjectStartWindow::ProjectStartWindow(wxWindow* parent, wxWindowID id, const wxString& title,
    const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
{
    // 初始化文件历史记录
    m_fileHistory = new wxFileHistory(9);
    
    // 加载历史记录：从配置文件读取到内存
    try
    {
        wxConfig config("Bear2");
        m_fileHistory->Load(config);
    }
    catch (...)
    {
        // 配置文件加载失败，忽略错误
    }

    // 初始化界面 + 加载历史记录到列表
    InitUI();
    LoadRecentProjects();

    // 窗口居中
    Centre(wxBOTH);
    wxInitAllImageHandlers();
    // 尝试加载图标，如果失败则忽略
    
}

// 析构函数
ProjectStartWindow::~ProjectStartWindow()
{
    // 保存历史记录
    SaveRecentProjects();
    // 释放文件历史记录
    delete m_fileHistory;
}

// 初始化界面：左侧列表 + 右侧垂直按钮
void ProjectStartWindow::InitUI()
{
    // 主容器：水平布局
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // 左侧区域：最近项目列表
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* titleText = new wxStaticText(this, wxID_ANY,
        "Welcom to the CBWave!!!", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    titleText->SetFont(wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    leftSizer->Add(titleText, 0, wxALL | wxEXPAND, 10);

    // 最近项目列表控件
    m_recentProjectsList = new wxListCtrl(this, ID_LIST_RECENT_PROJECTS,
        wxDefaultPosition, wxSize(-1, 400),
        wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    m_recentProjectsList->InsertColumn(0, "Project File: ", wxLIST_FORMAT_LEFT, 500);
    leftSizer->Add(m_recentProjectsList, 1, wxALL | wxEXPAND, 10);
    mainSizer->Add(leftSizer, 4, wxEXPAND | wxALL, 10);

    // 右侧区域：垂直按钮组
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    wxSize btnSize(150, 40); // 统一按钮尺寸

    // 打开项目按钮
    wxButton* btnOpen = new wxButton(this, ID_BTN_OPEN_PROJECT, "Open Project");
    btnOpen->SetMinSize(btnSize);
    rightSizer->Add(btnOpen, 0, wxALL | wxALIGN_CENTER, 10);

    // 退出按钮
    wxButton* btnExit = new wxButton(this, ID_BTN_EXIT, "Exit");
    btnExit->SetMinSize(btnSize);
    rightSizer->Add(btnExit, 0, wxALL | wxALIGN_CENTER, 10);

    mainSizer->Add(rightSizer, 1, wxALIGN_CENTER | wxALL, 20);

    // 设置布局和窗口最小尺寸
    SetSizer(mainSizer);
    SetMinSize(wxSize(800, 500));
}

// 加载历史记录到列表控件
void ProjectStartWindow::LoadRecentProjects()
{
    m_recentProjectsList->DeleteAllItems();

    // 遍历历史记录并添加到列表
    if (m_fileHistory)
    {
        for (size_t i = 0; i < m_fileHistory->GetCount(); ++i)
        {
            wxString path = m_fileHistory->GetHistoryFile(i);
            long itemIdx = m_recentProjectsList->InsertItem(i, path);
            m_recentProjectsList->SetItemData(itemIdx, i); // 绑定索引
        }
    }
}

// 保存历史记录到配置文件
void ProjectStartWindow::SaveRecentProjects()
{
    try
    {
        if (m_fileHistory)
        {
            wxConfig config("Bear2");
            m_fileHistory->Save(config);
            config.Flush();
        }
    }
    catch (...)
    {
        // 配置文件保存失败，忽略错误
    }
}

// 添加路径到历史记录
void ProjectStartWindow::AddProjectToHistory(const wxString& path)
{
    if (m_fileHistory)
    {
        m_fileHistory->AddFileToHistory(path); // 自动去重，最新在前
        LoadRecentProjects(); // 刷新列表
        SaveRecentProjects(); // 立即保存
    }
}

// 打开项目按钮事件
void ProjectStartWindow::OnOpenProject(wxCommandEvent& evt)
{
    wxFileDialog dlg(this,
        _("Open Project File"),
        wxEmptyString,
        wxEmptyString,
        _("Project Files (*.json)|*.json|All Files (*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK)
    {
        OpenProject(dlg.GetPath());
    }
}

// 实际打开项目逻辑
void ProjectStartWindow::OpenProject(const wxString& projectFile)
{
    if (!wxFile::Exists(projectFile))
    {
        wxMessageBox("File does not exist!", "Prompt", wxOK);
        return;
    }

    AddProjectToHistory(projectFile);
    m_selectedProjectDir = projectFile;
    SaveRecentProjects();
    EndModal(wxID_OK);
}

// 双击最近项目列表事件
// 双击最近项目列表事件
void ProjectStartWindow::OnRecentProjectDblClick(wxListEvent& evt)
{
    long itemIdx = evt.GetIndex();
    if (itemIdx == -1) return;

    if (m_fileHistory)
    {
        size_t historyIdx = m_recentProjectsList->GetItemData(itemIdx);
        wxString projectFile = m_fileHistory->GetHistoryFile(historyIdx);
        OpenProject(projectFile);
    }
}

// 退出按钮事件
void ProjectStartWindow::OnExit(wxCommandEvent& evt)
{
    EndModal(wxID_CANCEL);
}

// 获取选中的项目路径
wxString ProjectStartWindow::GetProjectDir()
{
    return m_selectedProjectDir;
}