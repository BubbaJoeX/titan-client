#include "SwgImportTrace.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    std::mutex g_mutex;
    FILE* g_file = nullptr;
    std::string g_path;
    unsigned long long g_sessionStartMs = 0;

#ifdef _WIN32
    unsigned long long nowMs()
    {
        return GetTickCount64();
    }

    std::string defaultLogPath()
    {
        const char* env = getenv("SWG_IMPORT_TRACE_LOG");
        if (env && env[0])
            return std::string(env);

        char tempDir[MAX_PATH] = {};
        const DWORD n = GetTempPathA(MAX_PATH, tempDir);
        if (n == 0 || n >= MAX_PATH)
            return std::string("SwgMayaEditor-import.log");
        std::string path = tempDir;
        if (!path.empty() && path.back() != '\\' && path.back() != '/')
            path += '\\';
        path += "SwgMayaEditor-import.log";
        return path;
    }

    void writeWindowsDebug(const char* msg)
    {
        OutputDebugStringA(msg);
    }
#else
    unsigned long long nowMs()
    {
        return 0;
    }

    std::string defaultLogPath()
    {
        const char* env = getenv("SWG_IMPORT_TRACE_LOG");
        if (env && env[0])
            return std::string(env);
        return std::string("/tmp/SwgMayaEditor-import.log");
    }

    void writeWindowsDebug(const char*) {}
#endif

    bool ensureOpen()
    {
        if (g_file)
            return true;
        g_path = defaultLogPath();
        g_file = fopen(g_path.c_str(), "a");
        if (!g_file)
            return false;
        setvbuf(g_file, nullptr, _IONBF, 0);
        return true;
    }

    void writeLineRaw(const char* category, const char* body)
    {
        if (!ensureOpen())
            return;
        const unsigned long long elapsed = g_sessionStartMs ? (nowMs() - g_sessionStartMs) : 0;
        fprintf(g_file, "[+%7llu ms] [%s] %s\n", elapsed, category ? category : "?", body ? body : "");
        fflush(g_file);
        std::string msg = std::string("[") + (category ? category : "?") + "] " + (body ? body : "") + "\n";
        writeWindowsDebug(msg.c_str());
        std::fputs(msg.c_str(), stderr);
    }

    void writeLine(const char* category, const char* fmt, va_list args)
    {
        if (!ensureOpen())
            return;

        char body[2048];
        vsnprintf(body, sizeof(body), fmt, args);

        const unsigned long long elapsed = g_sessionStartMs ? (nowMs() - g_sessionStartMs) : 0;
        fprintf(g_file, "[+%7llu ms] [%s] %s\n", elapsed, category ? category : "?", body);
        fflush(g_file);

        std::string msg = std::string("[") + (category ? category : "?") + "] " + body + "\n";
        writeWindowsDebug(msg.c_str());
        std::fputs(msg.c_str(), stderr);
    }
}

namespace SwgImportTrace
{

void beginSession(const char* operation, const char* targetPath)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sessionStartMs = nowMs();
    if (!ensureOpen())
        return;

#ifdef _WIN32
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    fprintf(g_file,
        "\n======== SwgMayaEditor import trace ========\n"
        " time: %04u-%02u-%02u %02u:%02u:%02u\n"
        " log:  %s\n"
        " op:   %s\n"
        " path: %s\n"
        " tail: Get-Content $env:TEMP\\SwgMayaEditor-import.log -Wait -Tail 40\n"
        "============================================\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        g_path.c_str(),
        operation ? operation : "(null)",
        targetPath ? targetPath : "(null)");
#else
    fprintf(g_file,
        "\n======== SwgMayaEditor import trace ========\n"
        " log:  %s\n"
        " op:   %s\n"
        " path: %s\n"
        "============================================\n",
        g_path.c_str(),
        operation ? operation : "(null)",
        targetPath ? targetPath : "(null)");
#endif
    fflush(g_file);
}

void stage(const char* label)
{
    if (!label || !label[0])
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    writeLineRaw("STAGE", label);
}

void stagef(const char* fmt, ...)
{
    if (!fmt || !fmt[0])
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    va_list args;
    va_start(args, fmt);
    writeLine("STAGE", fmt, args);
    va_end(args);
}

void log(const char* category, const char* fmt, ...)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    va_list args;
    va_start(args, fmt);
    writeLine(category, fmt, args);
    va_end(args);
}

void logV(const char* category, const char* fmt, va_list args)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    writeLine(category, fmt, args);
}

const char* logFilePath()
{
    return g_path.empty() ? "" : g_path.c_str();
}

}
