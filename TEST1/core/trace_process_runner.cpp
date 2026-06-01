#include "core/trace_process_runner.h"

#include "core/trace_translate_debug.h"

#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

static std::string shell_quote(const std::string& s)
{
    return std::string("\"") + s + "\"";
}

static std::string build_process_cmdline(const std::string& executable)
{
    namespace fs = std::filesystem;
    const std::string ext = fs::path(executable).extension().string();
    if (ext == ".cmd" || ext == ".bat") {
        const char* comspec = std::getenv("COMSPEC");
        const std::string cmd = (comspec && comspec[0]) ? comspec : "cmd.exe";
        return shell_quote(cmd) + " /c " + shell_quote(executable);
    }
    return shell_quote(executable);
}

#ifdef _WIN32
TraceProcessResult trace_process_run(
    const std::string& executable,
    const std::string& stdin_data,
    int timeout_ms)
{
    TraceProcessResult result;
    if (executable.empty()) {
        result.stderr_text = "empty executable";
        return result;
    }

    SECURITY_ATTRIBUTES sa {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE out_rd = nullptr;
    HANDLE out_wr = nullptr;
    HANDLE err_rd = nullptr;
    HANDLE err_wr = nullptr;
    HANDLE in_rd = nullptr;
    HANDLE in_wr = nullptr;

    auto close_all = [&]() {
        if (out_rd)
            CloseHandle(out_rd);
        if (out_wr)
            CloseHandle(out_wr);
        if (err_rd)
            CloseHandle(err_rd);
        if (err_wr)
            CloseHandle(err_wr);
        if (in_rd)
            CloseHandle(in_rd);
        if (in_wr)
            CloseHandle(in_wr);
    };

    if (!CreatePipe(&out_rd, &out_wr, &sa, 0)
        || !CreatePipe(&err_rd, &err_wr, &sa, 0)
        || !CreatePipe(&in_rd, &in_wr, &sa, 0)) {
        result.stderr_text = "CreatePipe failed";
        close_all();
        return result;
    }

    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_rd;
    si.hStdOutput = out_wr;
    si.hStdError = err_wr;

    PROCESS_INFORMATION pi {};
    const std::string cmdline = build_process_cmdline(executable);
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
        result.stderr_text = "CreateProcess failed";
        trace_translate_error_log("process_run CreateProcess FAILED cmdline=%s", cmdline.c_str());
        close_all();
        return result;
    }

    CloseHandle(in_rd);
    CloseHandle(out_wr);
    CloseHandle(err_wr);

    if (!stdin_data.empty() && in_wr) {
        DWORD written = 0;
        WriteFile(in_wr, stdin_data.data(), (DWORD)stdin_data.size(), &written, nullptr);
    }
    CloseHandle(in_wr);
    in_wr = nullptr;

    auto drain_pipe = [](HANDLE rd, std::string* dst) {
        if (!rd || !dst)
            return;
        char chunk[512];
        DWORD nread = 0;
        while (ReadFile(rd, chunk, sizeof(chunk), &nread, nullptr) && nread > 0)
            dst->append(chunk, chunk + nread);
    };

    const DWORD wait_ms = timeout_ms > 0 ? (DWORD)timeout_ms : INFINITE;
    const DWORD wait_rc = WaitForSingleObject(pi.hProcess, wait_ms);
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timed_out = true;
        result.exit_code = -2;
        result.stderr_text = "process timed out";
    } else {
        DWORD ec = 1;
        GetExitCodeProcess(pi.hProcess, &ec);
        result.exit_code = (int)ec;
    }

    drain_pipe(out_rd, &result.stdout_text);
    drain_pipe(err_rd, &result.stderr_text);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    close_all();
    return result;
}
#else
TraceProcessResult trace_process_run(
    const std::string& executable,
    const std::string& stdin_data,
    int timeout_ms)
{
    (void)timeout_ms;
    TraceProcessResult result;
    if (executable.empty()) {
        result.stderr_text = "empty executable";
        return result;
    }

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        result.stderr_text = "pipe failed";
        return result;
    }

    const pid_t pid = fork();
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl(executable.c_str(), executable.c_str(), (char*)nullptr);
        _exit(127);
    }
    if (pid < 0) {
        result.stderr_text = "fork failed";
        return result;
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    if (!stdin_data.empty())
        (void)write(in_pipe[1], stdin_data.data(), stdin_data.size());
    close(in_pipe[1]);

    char chunk[512];
    ssize_t n = 0;
    while ((n = read(out_pipe[0], chunk, sizeof(chunk))) > 0)
        result.stdout_text.append(chunk, chunk + n);
    close(out_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}
#endif
