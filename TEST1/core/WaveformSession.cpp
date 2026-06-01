#include "core/WaveformSession.h"

#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/textfile.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::string Trim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a]))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

void StripTrailingDot(std::string& s)
{
    while (!s.empty() && s.back() == '.')
        s.pop_back();
}

bool StartsWith(const std::string& s, const char* p)
{
    return s.size() >= strlen(p) && s.compare(0, strlen(p), p) == 0;
}

void AppendTreeOpenPath(WaveformSessionData& out, const std::string& raw)
{
    std::string p = Trim(raw);
    StripTrailingDot(p);
    if (p.empty())
        return;
    for (const std::string& e : out.treeOpenPaths) {
        if (e == p)
            return;
    }
    out.treeOpenPaths.push_back(p);
}

wxString ResolveTracePath(const wxString& sessionPath, const std::string& stored)
{
    if (stored.empty())
        return wxString();
    wxString p = wxString::FromUTF8(stored.c_str());
    if (wxFileName(p).IsAbsolute())
        return p;
    wxFileName base(sessionPath);
    base.SetFullName(wxEmptyString);
    base.SetExt(wxEmptyString);
    wxFileName rel(base.GetPath(), p);
    return rel.GetFullPath();
}

wxString RelativizeTracePath(const wxString& sessionPath, const wxString& tracePath)
{
    if (tracePath.empty())
        return wxString();
    wxFileName sess(sessionPath);
    wxFileName tr(tracePath);
    if (!sess.IsOk() || !tr.IsOk())
        return tracePath;
    wxString rel;
    if (tr.MakeRelativeTo(sess.GetPath()))
        rel = tr.GetFullPath();
    else
        rel = tr.GetFullPath();
    return rel;
}

bool LoadBear2Wave(const wxString& path, WaveformSessionData& out, wxString& err)
{
    wxTextFile tf;
    if (!tf.Open(path))
    {
        err = "cannot open session file";
        return false;
    }

    enum class Sec { None, Signals };
    Sec sec = Sec::None;
    int pendingRadix = -1;

    for (size_t i = 0; i < tf.GetLineCount(); ++i) {
        std::string line = Trim(tf.GetLine(i).ToUTF8().data());
        if (line.empty() || line[0] == '#')
            continue;

        if (line == "[signals]") {
            sec = Sec::Signals;
            continue;
        }
        if (line[0] == '[') {
            sec = Sec::None;
            continue;
        }

        if (sec == Sec::Signals) {
            if (line[0] == '@') {
                try {
                    pendingRadix = std::stoi(line.substr(1)) / 100;
                } catch (...) {
                    pendingRadix = -1;
                }
                continue;
            }
            if (line[0] == '+')
                line.erase(0, 1);
            SessionTrace tr;
            tr.name = Trim(line);
            tr.gtkwaveRadixCode = pendingRadix;
            pendingRadix = -1;
            if (!tr.name.empty())
                out.traces.push_back(std::move(tr));
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));
        if (key == "trace")
            out.tracePath = val;
        else if (key == "time_offset")
            out.timeOffset = std::stoll(val);
        else if (key == "display_range")
            out.displayRange = std::stoll(val);
        else if (key == "playhead")
            out.playhead = std::stoll(val);
        else if (key == "show_cursor_value")
            out.showCursorValue = (val == "1" || val == "true" || val == "yes");
        else if (key == "window_x")
            out.windowX = std::stoi(val);
        else if (key == "window_y")
            out.windowY = std::stoi(val);
        else if (key == "window_w")
            out.windowW = std::stoi(val);
        else if (key == "window_h")
            out.windowH = std::stoi(val);
        else if (key == "split_tree_wave")
            out.splitterTreeWave = std::stoi(val);
        else if (key == "split_main_ai")
            out.splitterMainAi = std::stoi(val);
        else if (key == "split_tree_list")
            out.splitterTreeList = std::stoi(val);
        else if (key == "tree_open") {
            std::istringstream iss(val);
            std::string part;
            while (std::getline(iss, part, ',')) {
                AppendTreeOpenPath(out, part);
            }
        }
    }
    tf.Close();

    out.tracePath = ResolveTracePath(path, out.tracePath).ToUTF8().data();
    return true;
}

bool LoadGtkwave(const wxString& path, WaveformSessionData& out, wxString& err)
{
    wxTextFile tf;
    if (!tf.Open(path))
    {
        err = "cannot open gtkw file";
        return false;
    }

    std::string section;
    int pendingRadix = -1;

    for (size_t i = 0; i < tf.GetLineCount(); ++i) {
        std::string line = Trim(tf.GetLine(i).ToUTF8().data());
        if (line.empty())
            continue;
        if (line == "[*]")
            continue;

        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section.find("treeopen") != std::string::npos)
                section = "treeopen";
            continue;
        }

        if (section == "dumpfile") {
            if (!line.empty() && line[0] != '[')
                out.tracePath = line;
            section.clear();
            continue;
        }

        if (StartsWith(line, "[dumpfile]")) {
            std::string rest = Trim(line.substr(10));
            if (!rest.empty())
                out.tracePath = rest;
            continue;
        }

        if (section == "state") {
            std::istringstream iss(line);
            long long a = 0, b = 0;
            if (iss >> a >> b) {
                out.timeOffset = a;
                if (b > a)
                    out.displayRange = b - a;
                else
                    out.displayRange = b;
            }
            section.clear();
            continue;
        }

        if (section == "pos") {
            std::istringstream iss(line);
            iss >> out.windowX >> out.windowY;
            section.clear();
            continue;
        }

        if (section == "size") {
            std::istringstream iss(line);
            iss >> out.windowW >> out.windowH;
            section.clear();
            continue;
        }

        if (section == "treeopen") {
            AppendTreeOpenPath(out, line);
            continue;
        }

        if (section == "sst_width") {
            try {
                out.splitterTreeWave = std::stoi(line);
            } catch (...) {
            }
            section.clear();
            continue;
        }

        if (section == "signals_width") {
            try {
                out.splitterTreeList = std::stoi(line);
            } catch (...) {
            }
            section.clear();
            continue;
        }

        if (line[0] == '@') {
            try {
                pendingRadix = std::stoi(line.substr(1)) / 100;
            } catch (...) {
                pendingRadix = -1;
            }
            continue;
        }

        if (line[0] == '+') {
            SessionTrace tr;
            tr.name = Trim(line.substr(1));
            tr.gtkwaveRadixCode = pendingRadix;
            pendingRadix = -1;
            if (!tr.name.empty())
                out.traces.push_back(std::move(tr));
            continue;
        }

        if (StartsWith(line, "+")) {
            SessionTrace tr;
            tr.name = Trim(line.substr(1));
            tr.gtkwaveRadixCode = pendingRadix;
            pendingRadix = -1;
            if (!tr.name.empty())
                out.traces.push_back(std::move(tr));
        }
    }
    tf.Close();

    out.tracePath = ResolveTracePath(path, out.tracePath).ToUTF8().data();
    return true;
}

bool SaveBear2Wave(const wxString& path, const WaveformSessionData& data, wxString& err)
{
    std::ofstream ofs(path.ToUTF8().data(), std::ios::out | std::ios::trunc);
    if (!ofs)
    {
        err = "cannot write session file";
        return false;
    }

    const wxString relTrace = RelativizeTracePath(path, wxString::FromUTF8(data.tracePath.c_str()));

    ofs << "# Bear2Wave session 2\n";
    ofs << "trace=" << relTrace.ToUTF8().data() << "\n";
    ofs << "time_offset=" << data.timeOffset << "\n";
    ofs << "display_range=" << data.displayRange << "\n";
    ofs << "playhead=" << data.playhead << "\n";
    ofs << "show_cursor_value=" << (data.showCursorValue ? "1" : "0") << "\n";
    if (data.windowW > 0 && data.windowH > 0) {
        ofs << "window_x=" << data.windowX << "\n";
        ofs << "window_y=" << data.windowY << "\n";
        ofs << "window_w=" << data.windowW << "\n";
        ofs << "window_h=" << data.windowH << "\n";
    }
    if (data.splitterTreeWave > 0)
        ofs << "split_tree_wave=" << data.splitterTreeWave << "\n";
    if (data.splitterMainAi > 0)
        ofs << "split_main_ai=" << data.splitterMainAi << "\n";
    if (data.splitterTreeList > 0)
        ofs << "split_tree_list=" << data.splitterTreeList << "\n";
    if (!data.treeOpenPaths.empty()) {
        ofs << "tree_open=";
        for (size_t i = 0; i < data.treeOpenPaths.size(); ++i) {
            if (i)
                ofs << ",";
            ofs << data.treeOpenPaths[i];
        }
        ofs << "\n";
    }
    ofs << "[signals]\n";
    for (const SessionTrace& tr : data.traces) {
        if (tr.gtkwaveRadixCode >= 0)
            ofs << "@" << (tr.gtkwaveRadixCode * 100) << "\n";
        ofs << "+" << tr.name << "\n";
    }
    return true;
}

bool SaveGtkwave(const wxString& path, const WaveformSessionData& data, wxString& err)
{
    std::ofstream ofs(path.ToUTF8().data(), std::ios::out | std::ios::trunc);
    if (!ofs)
    {
        err = "cannot write gtkw file";
        return false;
    }

    const wxString relTrace = RelativizeTracePath(path, wxString::FromUTF8(data.tracePath.c_str()));
    wxFileName fn(data.tracePath.empty() ? wxString() : wxString::FromUTF8(data.tracePath.c_str()));
    wxDateTime mtime = wxDateTime::Now();
    if (fn.FileExists())
        mtime = fn.GetModificationTime();

    const int wx = data.windowX >= 0 ? data.windowX : -1;
    const int wy = data.windowY >= 0 ? data.windowY : -1;
    const int ww = data.windowW > 0 ? data.windowW : 1400;
    const int wh = data.windowH > 0 ? data.windowH : 800;

    ofs << "[*]\n";
    ofs << "[*] GTKWave save file (written by Bear2Wave)\n";
    ofs << "[*]\n";
    ofs << "[dumpfile]\n";
    ofs << relTrace.ToUTF8().data() << "\n";
    ofs << "[dumpfile_mtime] " << mtime.GetTicks() << "\n";
    ofs << "[savefile] " << wxFileName(path).GetFullName().ToUTF8().data() << "\n";
    ofs << "[size]\n";
    ofs << ww << " " << wh << "\n";
    ofs << "[pos]\n";
    ofs << wx << " " << wy << "\n";
    if (data.splitterTreeWave > 0)
        ofs << "[sst_width] " << data.splitterTreeWave << "\n";
    if (data.splitterTreeList > 0)
        ofs << "[signals_width] " << data.splitterTreeList << "\n";
    ofs << "[state]\n";
    const long long end = data.timeOffset + std::max(1LL, data.displayRange);
    ofs << data.timeOffset << " " << end << "\n";
    for (const std::string& mod : data.treeOpenPaths) {
        ofs << "[treeopen]\n";
        ofs << mod << ".\n";
    }
    for (const SessionTrace& tr : data.traces) {
        if (tr.gtkwaveRadixCode >= 0)
            ofs << "@" << (tr.gtkwaveRadixCode * 100) << "\n";
        ofs << "+" << tr.name << "\n";
    }
    return true;
}

} // namespace

namespace WaveformSession {

bool IsSupportedExtension(const wxString& path)
{
    wxString ext = wxFileName(path).GetExt().Lower();
    return ext == "gtkw" || ext == "sav" || ext == "b2w" || ext == "save";
}

bool Load(const wxString& path, WaveformSessionData& out, wxString& err)
{
    out = WaveformSessionData{};
    if (!wxFileName::FileExists(path))
    {
        err = "file not found";
        return false;
    }

    wxString ext = wxFileName(path).GetExt().Lower();
    if (ext == "b2w")
        return LoadBear2Wave(path, out, err);
    return LoadGtkwave(path, out, err);
}

bool Save(const wxString& path, const WaveformSessionData& data, wxString& err)
{
    wxString ext = wxFileName(path).GetExt().Lower();
    if (ext == "b2w")
        return SaveBear2Wave(path, data, err);
    return SaveGtkwave(path, data, err);
}

} // namespace WaveformSession
