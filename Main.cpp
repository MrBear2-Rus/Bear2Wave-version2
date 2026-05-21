#include <wx/wx.h>

#include "ProjectStartWindow.h"
#include "ui/MainFrame.hpp"

/* TclScriptEngine.cpp 若已加入 TEST1.vcxproj，请删掉下面这一行，避免重复符号 */
#include "script/TclScriptEngine.cpp"
/* WaveformCompareHub.cpp 若已加入 TEST1.vcxproj，请删掉下面这一行，避免重复符号 */
#include "ui/WaveformCompareHub.cpp"
#include "core/WaveformSession.cpp"

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        wxString projectDir;
        int ret = wxID_CANCEL;

        {
            ProjectStartWindow startWindow;
            ret = startWindow.ShowModal();

            if (ret == wxID_OK)
                projectDir = startWindow.GetProjectDir();
        }

        if (ret == wxID_CANCEL)
            return false;

        MyFrame* frame = new MyFrame();
        frame->Centre(wxBOTH);
        frame->Show(true);

        if (!projectDir.IsEmpty())
            frame->SetProjectDir(projectDir);

        SetTopWindow(frame);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
