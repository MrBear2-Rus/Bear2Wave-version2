#include "core/trace_external_convert.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string path_str(const fs::path& p)
{
    return p.string();
}

static std::string trim_copy(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && isspace(static_cast<unsigned char>(s[b])))
        ++b;
    size_t e = s.size();
    while (e > b && isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return s.substr(b, e - b);
}

static void set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
}

static std::string env_or_empty(const char* name)
{
    if (!name)
        return {};
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0 || !buf)
        return {};
    std::string out(buf);
    free(buf);
    return out;
#else
    const char* v = getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

static std::string default_config_dir()
{
#ifdef _WIN32
    PWSTR wide = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wide)) && wide) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        std::string out;
        if (n > 1) {
            out.resize(static_cast<size_t>(n - 1));
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
        }
        CoTaskMemFree(wide);
        if (!out.empty())
            return out + "\\Bear2Wave";
    }
#endif
    const char* home = getenv("HOME");
    if (home && home[0])
        return std::string(home) + "/.bear2wave";
    return ".bear2wave";
}

std::string trace_external_config_file_path()
{
    return path_str(fs::path(default_config_dir()) / "external_tools.cfg");
}

static void apply_env_overrides(TraceExternalConfig* cfg)
{
    if (!cfg)
        return;
    const std::string vpd = trim_copy(env_or_empty("BEAR2WAVE_VPD2VCD"));
    const std::string wlf = trim_copy(env_or_empty("BEAR2WAVE_WLF2VCD"));
    const std::string fsdb = trim_copy(env_or_empty("BEAR2WAVE_FSDB2VCD"));
    const std::string shm = trim_copy(env_or_empty("BEAR2WAVE_SHM2VCD"));
    const std::string aet = trim_copy(env_or_empty("BEAR2WAVE_AET2VCD"));
    const std::string translate = trim_copy(env_or_empty("BEAR2WAVE_TRANSLATE_PROC"));
    const std::string transaction = trim_copy(env_or_empty("BEAR2WAVE_TRANSACTION_PROC"));
    const std::string filter_timeout = trim_copy(env_or_empty("BEAR2WAVE_FILTER_TIMEOUT_MS"));
    const std::string cache_dir = trim_copy(env_or_empty("BEAR2WAVE_EXT_CACHE_DIR"));
    const std::string cache_en = trim_copy(env_or_empty("BEAR2WAVE_EXT_CACHE"));
    if (!vpd.empty())
        cfg->vpd2vcd_path = vpd;
    if (!wlf.empty())
        cfg->wlf2vcd_path = wlf;
    if (!fsdb.empty())
        cfg->fsdb2vcd_path = fsdb;
    if (!shm.empty())
        cfg->shm2vcd_path = shm;
    if (!aet.empty())
        cfg->aet2vcd_path = aet;
    if (!translate.empty())
        cfg->translate_proc_path = translate;
    if (!transaction.empty())
        cfg->transaction_proc_path = transaction;
    if (!filter_timeout.empty()) {
        try {
            const int ms = std::stoi(filter_timeout);
            if (ms > 0)
                cfg->filter_process_timeout_ms = ms;
        } catch (...) {
        }
    }
    if (!cache_dir.empty())
        cfg->cache_dir = cache_dir;
    if (!cache_en.empty())
        cfg->cache_enabled = (cache_en == "0" || cache_en == "false" || cache_en == "no") ? 0 : 1;
}

TraceExternalConfig trace_external_load_config()
{
    TraceExternalConfig cfg;
    cfg.cache_dir = path_str(fs::path(default_config_dir()) / "convert_cache");

    const std::string path = trace_external_config_file_path();
    std::ifstream ifs(path);
    if (ifs) {
        std::string line;
        while (std::getline(ifs, line)) {
            line = trim_copy(line);
            if (line.empty() || line[0] == '#')
                continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string key = trim_copy(line.substr(0, eq));
            const std::string val = trim_copy(line.substr(eq + 1));
            if (key == "vpd2vcd")
                cfg.vpd2vcd_path = val;
            else if (key == "wlf2vcd")
                cfg.wlf2vcd_path = val;
            else if (key == "fsdb2vcd")
                cfg.fsdb2vcd_path = val;
            else if (key == "shm2vcd")
                cfg.shm2vcd_path = val;
            else if (key == "aet2vcd")
                cfg.aet2vcd_path = val;
            else if (key == "translate_proc")
                cfg.translate_proc_path = val;
            else if (key == "transaction_proc")
                cfg.transaction_proc_path = val;
            else if (key == "filter_timeout_ms") {
                try {
                    const int ms = std::stoi(val);
                    if (ms > 0)
                        cfg.filter_process_timeout_ms = ms;
                } catch (...) {
                }
            }
            else if (key == "cache_dir")
                cfg.cache_dir = val;
            else if (key == "cache_enabled")
                cfg.cache_enabled = (val == "0" || val == "false" || val == "no") ? 0 : 1;
        }
    }

    apply_env_overrides(&cfg);
    return cfg;
}

int trace_external_save_config(const TraceExternalConfig& cfg)
{
    const fs::path path = fs::path(trace_external_config_file_path());
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ofstream ofs(path);
    if (!ofs)
        return -1;

    ofs << "# Bear2Wave external converter paths (E4)\n";
    ofs << "vpd2vcd=" << cfg.vpd2vcd_path << "\n";
    ofs << "wlf2vcd=" << cfg.wlf2vcd_path << "\n";
    ofs << "fsdb2vcd=" << cfg.fsdb2vcd_path << "\n";
    ofs << "shm2vcd=" << cfg.shm2vcd_path << "\n";
    ofs << "aet2vcd=" << cfg.aet2vcd_path << "\n";
    ofs << "translate_proc=" << cfg.translate_proc_path << "\n";
    ofs << "transaction_proc=" << cfg.transaction_proc_path << "\n";
    ofs << "filter_timeout_ms=" << cfg.filter_process_timeout_ms << "\n";
    ofs << "cache_enabled=" << (cfg.cache_enabled ? "1" : "0") << "\n";
    ofs << "cache_dir=" << cfg.cache_dir << "\n";
    return ofs.good() ? 0 : -1;
}

TraceExternalKind trace_external_kind_for_extension(const char* ext)
{
    if (!ext || !ext[0])
        return TraceExternalKind::None;
    if (strcmp(ext, "vpd") == 0)
        return TraceExternalKind::Vpd;
    if (strcmp(ext, "wlf") == 0)
        return TraceExternalKind::Wlf;
    if (strcmp(ext, "fsdb") == 0)
        return TraceExternalKind::Fsdb;
    if (strcmp(ext, "shm") == 0 || strcmp(ext, "trn") == 0)
        return TraceExternalKind::Shm;
    if (strcmp(ext, "aet") == 0 || strcmp(ext, "aet2") == 0 || strcmp(ext, "ae2") == 0)
        return TraceExternalKind::Aet;
    return TraceExternalKind::None;
}

int trace_external_extension_needs_converter(const char* ext)
{
    return trace_external_kind_for_extension(ext) != TraceExternalKind::None ? 1 : 0;
}

static const char* tool_for_kind(const TraceExternalConfig* cfg, TraceExternalKind kind)
{
    if (!cfg)
        return nullptr;
    switch (kind) {
    case TraceExternalKind::Vpd:
        return cfg->vpd2vcd_path.c_str();
    case TraceExternalKind::Wlf:
        return cfg->wlf2vcd_path.c_str();
    case TraceExternalKind::Fsdb:
        return cfg->fsdb2vcd_path.c_str();
    case TraceExternalKind::Shm:
        return cfg->shm2vcd_path.c_str();
    case TraceExternalKind::Aet:
        return cfg->aet2vcd_path.c_str();
    default:
        return nullptr;
    }
}

static const char* default_tool_name(TraceExternalKind kind)
{
    switch (kind) {
    case TraceExternalKind::Vpd:
        return "vpd2vcd";
    case TraceExternalKind::Wlf:
        return "wlf2vcd";
    case TraceExternalKind::Fsdb:
        return "fsdb2vcd";
    case TraceExternalKind::Shm:
        return "shm2vcd";
    case TraceExternalKind::Aet:
        return "aet2vcd";
    default:
        return nullptr;
    }
}

static bool file_exists_utf8(const std::string& path)
{
    if (path.empty())
        return false;
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

static std::string exe_directory()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    return path_str(fs::path(buf).parent_path());
#else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    return path_str(fs::path(buf).parent_path());
#endif
}

static std::vector<std::string> split_search_path(const char* env_name)
{
    std::vector<std::string> out;
    const std::string raw = env_or_empty(env_name);
    if (raw.empty())
        return out;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    size_t b = 0;
    while (b <= raw.size()) {
        const size_t e = raw.find(sep, b);
        const std::string part = trim_copy(raw.substr(b, e == std::string::npos ? std::string::npos : e - b));
        if (!part.empty())
            out.push_back(part);
        if (e == std::string::npos)
            break;
        b = e + 1;
    }
    return out;
}

static void push_if_exists(std::vector<std::string>* out, const fs::path& p)
{
    if (!out || p.empty())
        return;
    const std::string s = path_str(p);
    if (!file_exists_utf8(s))
        return;
    if (std::find(out->begin(), out->end(), s) == out->end())
        out->push_back(s);
}

static void push_tool_in_dir(std::vector<std::string>* out, const std::string& dir, const char* name)
{
    if (!out || !name || !name[0] || dir.empty())
        return;
    push_if_exists(out, fs::path(dir) / name);
#ifdef _WIN32
    push_if_exists(out, fs::path(dir) / (std::string(name) + ".exe"));
    push_if_exists(out, fs::path(dir) / (std::string(name) + ".cmd"));
    push_if_exists(out, fs::path(dir) / (std::string(name) + ".bat"));
    push_if_exists(out, fs::path(dir) / "win64" / name);
    push_if_exists(out, fs::path(dir) / "win64" / (std::string(name) + ".exe"));
    push_if_exists(out, fs::path(dir) / "win64pe" / name);
    push_if_exists(out, fs::path(dir) / "win64pe" / (std::string(name) + ".exe"));
#endif
}

static void push_from_env_home(std::vector<std::string>* out, const char* env_name, const char* tool_name)
{
    const std::string home = trim_copy(env_or_empty(env_name));
    if (home.empty())
        return;
    push_tool_in_dir(out, home + "/bin", tool_name);
    push_tool_in_dir(out, home, tool_name);
#ifdef _WIN32
    push_tool_in_dir(out, home + "\\bin", tool_name);
    push_tool_in_dir(out, home + "\\win64", tool_name);
#endif
}

static void push_aet_tool_names(std::vector<std::string>* out, const std::string& dir)
{
    if (!out || dir.empty())
        return;
    static const char* kNames[] = {"aet2vcd", "ae2vcd", "ae2export", "aetexport", "mock_aet2vcd"};
    for (const char* n : kNames)
        push_tool_in_dir(out, dir, n);
}

/** Walk up from start (exe dir / cwd) to find repo tools/ with dev mocks. */
static void push_repo_tools_dir(std::vector<std::string>* out, TraceExternalKind kind, const fs::path& start)
{
    if (!out || start.empty())
        return;
    const char* standard = default_tool_name(kind);
    const char* mock = nullptr;
    switch (kind) {
    case TraceExternalKind::Vpd:
        mock = "mock_vpd2vcd";
        break;
    case TraceExternalKind::Shm:
        mock = "mock_shm2vcd";
        break;
    case TraceExternalKind::Aet:
        mock = "mock_aet2vcd";
        break;
    default:
        break;
    }

    fs::path p = start;
    std::error_code ec;
    p = fs::absolute(p, ec);
    for (int depth = 0; depth < 8; ++depth) {
        const std::string tools = path_str(p / "tools");
        if (standard)
            push_tool_in_dir(out, tools, standard);
        if (mock)
            push_tool_in_dir(out, tools, mock);
        if (kind == TraceExternalKind::Aet)
            push_aet_tool_names(out, tools);
        if (kind == TraceExternalKind::Shm)
            push_tool_in_dir(out, tools, "simvisdbutil");
        if (!p.has_parent_path() || p == p.parent_path())
            break;
        p = p.parent_path();
    }
}

static std::vector<std::string> tool_search_candidates(TraceExternalKind kind, const TraceExternalConfig* cfg)
{
    std::vector<std::string> out;
    const char* name = default_tool_name(kind);
    if (!name)
        return out;

    const char* configured = tool_for_kind(cfg, kind);
    if (configured && configured[0])
        push_if_exists(&out, fs::path(configured));

    switch (kind) {
    case TraceExternalKind::Vpd:
        push_from_env_home(&out, "VCS_HOME", name);
        push_from_env_home(&out, "DVE_HOME", name);
        push_from_env_home(&out, "SYNOPSYS", name);
        break;
    case TraceExternalKind::Wlf:
        push_from_env_home(&out, "MODELTECH", name);
        push_from_env_home(&out, "MTI_HOME", name);
        push_from_env_home(&out, "QUESTA_HOME", name);
        push_from_env_home(&out, "MODELSIM_ROOT", name);
        break;
    case TraceExternalKind::Fsdb:
        push_from_env_home(&out, "VERDI_HOME", name);
        push_from_env_home(&out, "NOVAS_HOME", name);
        push_from_env_home(&out, "FSDB_HOME", name);
        break;
    case TraceExternalKind::Shm:
        push_from_env_home(&out, "CDS_INST_DIR", "simvisdbutil");
        push_from_env_home(&out, "XCELIUM_HOME", "simvisdbutil");
        push_from_env_home(&out, "CADENCE_HOME", "simvisdbutil");
        push_from_env_home(&out, "INCA_HOME", "simvisdbutil");
        push_tool_in_dir(&out, trim_copy(env_or_empty("CDS_INST_DIR")) + "/tools/bin", "simvisdbutil");
        push_tool_in_dir(&out, trim_copy(env_or_empty("CDS_INST_DIR")) + "/tools/bin", name);
        break;
    case TraceExternalKind::Aet: {
        push_from_env_home(&out, "SIMARAMA_BASE", name);
        const std::string sim = trim_copy(env_or_empty("SIMARAMA_BASE"));
        if (!sim.empty()) {
            push_aet_tool_names(&out, sim);
            push_aet_tool_names(&out, sim + "/tools/bin");
            push_aet_tool_names(&out, sim + "/bin");
            push_aet_tool_names(&out, sim + "/tools/aet/bin");
        }
        push_from_env_home(&out, "IBM_SIMRAMA_HOME", name);
        const std::string ibm = trim_copy(env_or_empty("IBM_SIMRAMA_HOME"));
        if (!ibm.empty()) {
            push_aet_tool_names(&out, ibm);
            push_aet_tool_names(&out, ibm + "/tools/bin");
        }
        break;
    }
    default:
        break;
    }

    const std::string extra = trim_copy(env_or_empty("BEAR2WAVE_EXT_SEARCH_DIRS"));
    if (!extra.empty()) {
#ifdef _WIN32
        const char sep = ';';
#else
        const char sep = ':';
#endif
        size_t b = 0;
        while (b <= extra.size()) {
            const size_t e = extra.find(sep, b);
            const std::string dir = trim_copy(extra.substr(b, e == std::string::npos ? std::string::npos : e - b));
            push_tool_in_dir(&out, dir, name);
            if (e == std::string::npos)
                break;
            b = e + 1;
        }
    }

    const std::string exe_dir = exe_directory();
    if (!exe_dir.empty())
        push_repo_tools_dir(&out, kind, fs::path(exe_dir));

    {
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (!ec)
            push_repo_tools_dir(&out, kind, cwd);
    }

#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = SearchPathA(nullptr, name, ".exe", MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH)
        push_if_exists(&out, fs::path(buf));
    n = SearchPathA(nullptr, (std::string(name) + ".exe").c_str(), nullptr, MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH)
        push_if_exists(&out, fs::path(buf));
    n = SearchPathA(nullptr, (std::string(name) + ".cmd").c_str(), nullptr, MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH)
        push_if_exists(&out, fs::path(buf));
    n = SearchPathA(nullptr, (std::string(name) + ".bat").c_str(), nullptr, MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH)
        push_if_exists(&out, fs::path(buf));
#else
    for (const std::string& dir : split_search_path("PATH"))
        push_tool_in_dir(&out, dir, name);
#endif

    return out;
}

std::string trace_external_probe_tool(TraceExternalKind kind, const TraceExternalConfig* cfg)
{
    TraceExternalConfig owned;
    const TraceExternalConfig* use = cfg;
    if (!use) {
        owned = trace_external_load_config();
        use = &owned;
    }
    const std::vector<std::string> candidates = tool_search_candidates(kind, use);
    return candidates.empty() ? std::string() : candidates.front();
}

void trace_external_apply_probed_paths(TraceExternalConfig* cfg)
{
    if (!cfg)
        return;
    if (cfg->vpd2vcd_path.empty())
        cfg->vpd2vcd_path = trace_external_probe_tool(TraceExternalKind::Vpd, cfg);
    if (cfg->wlf2vcd_path.empty())
        cfg->wlf2vcd_path = trace_external_probe_tool(TraceExternalKind::Wlf, cfg);
    if (cfg->fsdb2vcd_path.empty())
        cfg->fsdb2vcd_path = trace_external_probe_tool(TraceExternalKind::Fsdb, cfg);
    if (cfg->shm2vcd_path.empty())
        cfg->shm2vcd_path = trace_external_probe_tool(TraceExternalKind::Shm, cfg);
    if (cfg->aet2vcd_path.empty())
        cfg->aet2vcd_path = trace_external_probe_tool(TraceExternalKind::Aet, cfg);
}

static std::string resolve_tool_path(const TraceExternalConfig* cfg, TraceExternalKind kind)
{
    const std::vector<std::string> candidates = tool_search_candidates(kind, cfg);
    if (!candidates.empty())
        return candidates.front();

    const char* configured = tool_for_kind(cfg, kind);
    return configured && configured[0] ? std::string(configured) : std::string();
}

static std::string shell_quote(const std::string& s)
{
    return std::string("\"") + s + "\"";
}

static std::string build_convert_cmdline(
    TraceExternalKind kind,
    const std::string& tool,
    const fs::path& source,
    const fs::path& tmp_out)
{
    std::error_code ec;
    const std::string in = path_str(fs::absolute(source, ec));
    const std::string out = path_str(fs::absolute(tmp_out, ec));
    switch (kind) {
    case TraceExternalKind::Wlf:
        return shell_quote(tool) + " -o " + shell_quote(out) + " " + shell_quote(in);
    case TraceExternalKind::Fsdb:
        return shell_quote(tool) + " " + shell_quote(in) + " -o " + shell_quote(out);
    case TraceExternalKind::Shm:
    case TraceExternalKind::Aet:
    case TraceExternalKind::Vpd:
    default:
        return shell_quote(tool) + " " + shell_quote(in) + " " + shell_quote(out);
    }
}

#ifdef _WIN32
static int run_process(const std::string& cmdline, std::string* captured_err)
{
    STARTUPINFOA si {};
    PROCESS_INFORMATION pi {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    SECURITY_ATTRIBUTES sa {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr;
    HANDLE wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0))
        return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.hStdOutput = wr;
    si.hStdError = wr;

    std::vector<char> mutable_cmd(cmdline.begin(), cmdline.end());
    mutable_cmd.push_back('\0');

    if (!CreateProcessA(
            nullptr,
            mutable_cmd.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        if (captured_err)
            *captured_err = "CreateProcess failed";
        return -1;
    }

    CloseHandle(wr);
    std::string output;
    char chunk[512];
    DWORD read = 0;
    while (ReadFile(rd, chunk, sizeof(chunk), &read, nullptr) && read > 0)
        output.append(chunk, chunk + read);
    CloseHandle(rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (captured_err && !output.empty())
        *captured_err = output;
    return static_cast<int>(ec);
}
#else
static int run_process(const std::string& cmdline, std::string* captured_err)
{
    const int rc = system(cmdline.c_str());
    if (captured_err)
        *captured_err = (rc != 0) ? "command failed" : std::string();
    return rc;
}
#endif

static uint64_t file_mtime_u64(const fs::path& p)
{
    std::error_code ec;
    const auto ft = fs::last_write_time(p, ec);
    if (ec)
        return 0;
    return static_cast<uint64_t>(ft.time_since_epoch().count());
}

static std::string cache_key_for_source(const fs::path& source)
{
    std::error_code ec;
    const auto sz = fs::file_size(source, ec);
    const uint64_t mt = file_mtime_u64(source);
    std::string abs = path_str(fs::absolute(source, ec));
    std::transform(abs.begin(), abs.end(), abs.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });

    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* data, size_t n) {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    mix(abs.data(), abs.size());
    mix(&sz, sizeof(sz));
    mix(&mt, sizeof(mt));

    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

static std::string sanitize_basename(const std::string& stem)
{
    std::string out;
    out.reserve(stem.size());
    for (char c : stem) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')
            out.push_back(c);
        else
            out.push_back('_');
    }
    if (out.empty())
        out = "trace";
    return out;
}

static fs::path cache_vcd_path(const TraceExternalConfig* cfg, const fs::path& source)
{
    const std::string key = cache_key_for_source(source);
    const std::string stem = sanitize_basename(source.stem().string());
    return fs::path(cfg->cache_dir) / (key + "_" + stem + ".vcd");
}

static fs::path cache_meta_path(const fs::path& vcd_path)
{
    return fs::path(vcd_path.string() + ".meta");
}

static bool cache_meta_matches(const fs::path& meta_path, const fs::path& source)
{
    std::ifstream ifs(meta_path);
    if (!ifs)
        return false;

    std::error_code ec;
    const auto sz = fs::file_size(source, ec);
    const uint64_t mt = file_mtime_u64(source);

    std::string line;
    std::string src_line;
    std::string size_line;
    std::string mtime_line;
    while (std::getline(ifs, line)) {
        line = trim_copy(line);
        if (line.rfind("source=", 0) == 0)
            src_line = line.substr(7);
        else if (line.rfind("size=", 0) == 0)
            size_line = line.substr(5);
        else if (line.rfind("mtime=", 0) == 0)
            mtime_line = line.substr(6);
    }

    if (src_line.empty() || size_line.empty() || mtime_line.empty())
        return false;

    const std::string abs = path_str(fs::absolute(source, ec));
    if (src_line != abs)
        return false;
    if (strtoull(size_line.c_str(), nullptr, 10) != static_cast<unsigned long long>(sz))
        return false;
    if (strtoull(mtime_line.c_str(), nullptr, 10) != mt)
        return false;
    return true;
}

static void write_cache_meta(const fs::path& meta_path, const fs::path& source)
{
    std::error_code ec;
    const auto sz = fs::file_size(source, ec);
    const uint64_t mt = file_mtime_u64(source);
    const std::string abs = path_str(fs::absolute(source, ec));

    std::ofstream ofs(meta_path);
    if (!ofs)
        return;
    ofs << "source=" << abs << "\n";
    ofs << "size=" << sz << "\n";
    ofs << "mtime=" << mt << "\n";
}

void trace_external_clear_cache(const TraceExternalConfig* cfg)
{
    if (!cfg)
        return;
    std::error_code ec;
    const fs::path dir = fs::path(cfg->cache_dir);
    if (!fs::exists(dir, ec))
        return;
    for (const auto& ent : fs::directory_iterator(dir, ec)) {
        if (!ent.is_regular_file())
            continue;
        const std::string ext = ent.path().extension().string();
        if (ext == ".vcd" || ext == ".meta")
            fs::remove(ent.path(), ec);
    }
}

int trace_external_convert_to_vcd(
    const char* source_path,
    const TraceExternalConfig* cfg_in,
    char* out_vcd_path,
    size_t out_vcd_path_len,
    char* err_buf,
    size_t err_buf_len)
{
    if (!source_path || !source_path[0] || !out_vcd_path || out_vcd_path_len == 0) {
        set_err(err_buf, err_buf_len, "invalid arguments");
        return -1;
    }

    const fs::path source = fs::path(source_path);
    std::error_code ec;
    if (!fs::exists(source, ec)) {
        set_err(err_buf, err_buf_len, "source file not found");
        return -1;
    }

    std::string ext = source.extension().string();
    if (!ext.empty() && ext[0] == '.')
        ext.erase(ext.begin());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });

    const TraceExternalKind kind = trace_external_kind_for_extension(ext.c_str());
    if (kind == TraceExternalKind::None) {
        set_err(err_buf, err_buf_len, "not an external-converter extension");
        return -1;
    }

    TraceExternalConfig owned;
    const TraceExternalConfig* cfg = cfg_in;
    if (!cfg) {
        owned = trace_external_load_config();
        cfg = &owned;
    }

    const fs::path out_path = cache_vcd_path(cfg, source);
    const fs::path meta_path = cache_meta_path(out_path);

    if (cfg->cache_enabled) {
        std::error_code ec2;
        fs::create_directories(out_path.parent_path(), ec2);
        if (fs::exists(out_path, ec2) && fs::file_size(out_path, ec2) > 0
            && cache_meta_matches(meta_path, source)) {
            strncpy(out_vcd_path, path_str(out_path).c_str(), out_vcd_path_len - 1);
            out_vcd_path[out_vcd_path_len - 1] = '\0';
            set_err(err_buf, err_buf_len, "INFO: using cached conversion");
            return 0;
        }
    }

    const std::string tool = resolve_tool_path(cfg, kind);
    if (tool.empty()) {
        set_err(err_buf, err_buf_len,
            "external converter not configured (Edit -> External Tool Paths, or set BEAR2WAVE_VPD2VCD / WLF2VCD / FSDB2VCD / SHM2VCD / AET2VCD)");
        return -2;
    }

    fs::create_directories(out_path.parent_path(), ec);
    const fs::path tmp_out = fs::path(path_str(out_path) + ".part");

    std::ostringstream cmd;
    cmd << build_convert_cmdline(kind, tool, source, tmp_out);

    std::string proc_err;
    const int rc = run_process(cmd.str(), &proc_err);
    if (rc != 0 || !fs::exists(tmp_out, ec) || fs::file_size(tmp_out, ec) == 0) {
        fs::remove(tmp_out, ec);
        char msg[768];
        snprintf(msg, sizeof(msg),
            "external converter failed (exit=%d tool=\"%s\")%s%s",
            rc,
            tool.c_str(),
            proc_err.empty() ? "" : ": ",
            proc_err.empty() ? "" : proc_err.c_str());
        set_err(err_buf, err_buf_len, msg);
        return -1;
    }

    fs::rename(tmp_out, out_path, ec);
    if (ec) {
        fs::remove(tmp_out, ec);
        set_err(err_buf, err_buf_len, "failed to finalize cached VCD");
        return -1;
    }

    write_cache_meta(meta_path, source);

    strncpy(out_vcd_path, path_str(out_path).c_str(), out_vcd_path_len - 1);
    out_vcd_path[out_vcd_path_len - 1] = '\0';
    return 0;
}
