// ======================================================================
// TreBrowser — Win32 GUI for browsing SWG .tre archives via Nuna library
// Copyright (c) Titan Project
// ======================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "Nuna.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <cwctype>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

namespace
{

constexpr wchar_t kClassName[] = L"NunaTreBrowserMainWnd";
constexpr wchar_t kTitle[] = L"Nuna Tre Browser";

enum ControlIds : int
{
    IDC_FOLDER_EDIT = 1001,
    IDC_BROWSE_FOLDER,
    IDC_REFRESH,
    IDC_FILE_LIST,
    IDC_LIST_CONTENTS,
    IDC_INFO,
    IDC_EXTRACT,
    IDC_PASSWORD_LABEL,
    IDC_PASSWORD_EDIT,
    IDC_LOG,
};

HWND g_hwndMain = nullptr;
HWND g_folderEdit = nullptr;
HWND g_fileList = nullptr;
HWND g_log = nullptr;
HWND g_passwordEdit = nullptr;

INITCOMMONCONTROLSEX g_icc = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };

std::string WideToUtf8(std::wstring_view w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(std::string_view u8)
{
    if (u8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), nullptr, 0);
    if (n <= 0)
        return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), out.data(), n);
    return out;
}

std::wstring GetWindowTextStr(HWND h)
{
    const int len = GetWindowTextLengthW(h);
    if (len <= 0)
        return {};
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize(static_cast<size_t>(len));
    return s;
}

void AppendLogUtf8(std::string_view text)
{
    if (!g_log)
        return;
    std::wstring w = Utf8ToWide(std::string(text));
    const int end = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, static_cast<WPARAM>(end), static_cast<LPARAM>(end));
    SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(w.c_str()));
}

void AppendLogLine(std::wstring_view line)
{
    AppendLogUtf8(WideToUtf8(line) + "\r\n");
}

void ClearLog()
{
    if (g_log)
        SetWindowTextW(g_log, L"");
}

bool EndsWithTreInsensitive(std::wstring_view name)
{
    if (name.size() < 4)
        return false;
    return name[name.size() - 4] == L'.'
        && std::towlower(static_cast<wint_t>(name[name.size() - 3])) == L't'
        && std::towlower(static_cast<wint_t>(name[name.size() - 2])) == L'r'
        && std::towlower(static_cast<wint_t>(name[name.size() - 1])) == L'e';
}

void RefreshTreList(HWND folderEdit, HWND listBox)
{
    SendMessageW(listBox, LB_RESETCONTENT, 0, 0);
    const std::wstring folderWs = GetWindowTextStr(folderEdit);
    if (folderWs.empty())
    {
        AppendLogLine(L"Choose a folder first.");
        return;
    }

    std::error_code ec;
    fs::path root(folderWs);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
    {
        AppendLogLine(L"Folder does not exist or is not a directory.");
        return;
    }

    std::vector<std::wstring> names;
    for (const fs::directory_entry& ent : fs::directory_iterator(root, ec))
    {
        if (ec)
            break;
        if (!ent.is_regular_file(ec))
            continue;
        std::wstring fname = ent.path().filename().wstring();
        if (EndsWithTreInsensitive(fname))
            names.push_back(std::move(fname));
    }
    std::sort(names.begin(), names.end());

    for (const auto& n : names)
        SendMessageW(listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(n.c_str()));

    std::wstring msg = L"Found ";
    msg += std::to_wstring(names.size());
    msg += L" .tre file(s).";
    AppendLogLine(msg);
}

fs::path GetFolderPathFromEdit(HWND folderEdit)
{
    std::wstring ws = GetWindowTextStr(folderEdit);
    if (ws.empty())
        return {};
    return fs::path(ws);
}

std::vector<fs::path> GetSelectedTrePaths(HWND folderEdit, HWND listBox)
{
    std::vector<fs::path> paths;
    fs::path root = GetFolderPathFromEdit(folderEdit);
    if (root.empty())
        return paths;

    const int n = static_cast<int>(SendMessageW(listBox, LB_GETCOUNT, 0, 0));
    if (n == LB_ERR || n <= 0)
        return paths;

    std::vector<int> sel(static_cast<size_t>(n));
    const int selCount = static_cast<int>(SendMessageW(listBox, LB_GETSELITEMS, static_cast<WPARAM>(n), reinterpret_cast<LPARAM>(sel.data())));
    auto pushPathForIndex = [&](int idx)
    {
        if (idx < 0 || idx >= n)
            return;
        const int len = static_cast<int>(SendMessageW(listBox, LB_GETTEXTLEN, static_cast<WPARAM>(idx), 0));
        if (len <= 0)
            return;
        std::wstring name(static_cast<size_t>(len) + 1, L'\0');
        SendMessageW(listBox, LB_GETTEXT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(name.data()));
        name.resize(static_cast<size_t>(len));
        paths.push_back(root / name);
    };

    if (selCount > 0)
    {
        sel.resize(static_cast<size_t>(selCount));
        for (int i = 0; i < selCount; ++i)
            pushPathForIndex(sel[static_cast<size_t>(i)]);
    }
    else
    {
        const int cur = static_cast<int>(SendMessageW(listBox, LB_GETCARETINDEX, 0, 0));
        if (cur >= 0 && cur < n)
            pushPathForIndex(cur);
        else
        {
            for (int i = 0; i < n; ++i)
                pushPathForIndex(i);
        }
    }
    return paths;
}

Nuna::EncryptionOptions PasswordOptionsFromUi()
{
    Nuna::EncryptionOptions enc;
    if (g_passwordEdit)
    {
        std::wstring pw = GetWindowTextStr(g_passwordEdit);
        enc.password = WideToUtf8(pw);
    }
    return enc;
}

std::string CaptureStdout(const std::function<void()>& fn)
{
    std::ostringstream oss;
    std::streambuf* const old = std::cout.rdbuf(oss.rdbuf());
    fn();
    std::cout.rdbuf(old);
    return oss.str();
}

void RunListOnPaths(const std::vector<fs::path>& paths)
{
    if (paths.empty())
    {
        AppendLogLine(L"No .tre files selected.");
        return;
    }

    Nuna::ListOptions opts;
    opts.showSize = true;
    opts.showCompressed = true;
    opts.showOffset = false;
    opts.encryption = PasswordOptionsFromUi();

    for (const fs::path& p : paths)
    {
        AppendLogLine(L"——— List ———");
        AppendLogUtf8(std::string("File: ") + p.u8string() + "\r\n");

        std::vector<std::pair<std::string, Nuna::TocEntry>> entries;
        const std::string body = CaptureStdout([&]()
            {
                const Nuna::Result r = Nuna::list(p.u8string(), opts, &entries);
                if (!r.ok())
                    std::cout << "Error: " << r.message << "\n";
            });

        AppendLogUtf8(body);

        if (!entries.empty())
        {
            AppendLogUtf8(std::string("— Parsed ") + std::to_string(entries.size()) + " entr(y/ies) in memory.\r\n");
        }
    }
}

void RunInfoOnPaths(const std::vector<fs::path>& paths)
{
    if (paths.empty())
    {
        AppendLogLine(L"No .tre files selected.");
        return;
    }

    for (const fs::path& p : paths)
    {
        AppendLogLine(L"——— Information ———");

        Nuna::ArchiveStats stats{};
        const Nuna::Result sr = Nuna::getStats(p.u8string(), stats, PasswordOptionsFromUi());
        if (!sr.ok())
        {
            AppendLogUtf8("getStats: " + sr.message + "\r\n");
            continue;
        }

        std::ostringstream hdr;
        hdr << "Path: " << p.u8string() << "\n";
        hdr << "Files (header): " << stats.fileCount << "\n";
        hdr << "Encrypted: " << (stats.encrypted ? "yes" : "no") << "\n";
        hdr << "Version tag: 0x" << std::hex << stats.version << std::dec << "\n";
        AppendLogUtf8(hdr.str());

        const Nuna::Result vr = Nuna::validate(p.u8string(), PasswordOptionsFromUi());
        AppendLogUtf8(std::string("validate: ") + vr.message + "\r\n");

        AppendLogUtf8("—— analyze ——\r\n");
        const std::string az = CaptureStdout([&]()
            {
                const Nuna::Result ar = Nuna::analyze(p.u8string(), PasswordOptionsFromUi());
                if (!ar.ok())
                    std::cout << "analyze result: " << ar.message << "\n";
            });
        AppendLogUtf8(az);
    }
}

void RunExtractOnPaths(const std::vector<fs::path>& paths)
{
    if (paths.empty())
    {
        AppendLogLine(L"No .tre files selected.");
        return;
    }

    BROWSEINFOW bi{};
    wchar_t display[MAX_PATH]{};
    bi.hwndOwner = g_hwndMain;
    bi.pszDisplayName = display;
    bi.lpszTitle = L"Select folder to extract into";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
        return;

    wchar_t outDir[MAX_PATH]{};
    bool okPath = SHGetPathFromIDListW(pidl, outDir);
    CoTaskMemFree(pidl);
    if (!okPath)
        return;

    Nuna::UnpackOptions uo;
    uo.overwrite = true;
    uo.quiet = true;
    uo.verbose = false;
    uo.encryption = PasswordOptionsFromUi();

    for (const fs::path& trePath : paths)
    {
        const std::wstring stem = trePath.stem().wstring();
        fs::path dest = fs::path(outDir) / stem;

        std::error_code ec;
        fs::create_directories(dest, ec);
        if (ec)
        {
            AppendLogLine(L"Could not create folder:");
            AppendLogLine(dest.wstring());
            continue;
        }

        AppendLogUtf8(std::string("Extracting ") + trePath.u8string() + "\r\n");
        AppendLogUtf8(std::string("        → ") + dest.u8string() + "\r\n");

        const Nuna::Result ur = Nuna::unpack(trePath.u8string(), dest.u8string(), uo);
        if (!ur.ok())
            AppendLogUtf8(std::string("Error: ") + ur.message + "\r\n");
        else
            AppendLogUtf8(ur.message + "\r\n");
    }
}

bool BrowseForFolder(HWND owner, HWND targetEdit)
{
    BROWSEINFOW bi{};
    wchar_t display[MAX_PATH]{};
    bi.hwndOwner = owner;
    bi.pszDisplayName = display;
    bi.lpszTitle = L"Select folder containing .tre files";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = nullptr;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
        return false;
    wchar_t path[MAX_PATH]{};
    if (!SHGetPathFromIDListW(pidl, path))
    {
        CoTaskMemFree(pidl);
        return false;
    }
    CoTaskMemFree(pidl);
    SetWindowTextW(targetEdit, path);
    return true;
}

void LayoutChildren(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int margin = 8;
    const int btnW = 110;
    const int btnH = 26;
    const int editH = 22;
    const int row1y = margin;
    const int folderEditW = std::max(100, w - margin * 4 - btnW * 2);

    MoveWindow(g_folderEdit, margin, row1y, folderEditW, editH, TRUE);
    HWND browseFolder = GetDlgItem(hwnd, IDC_BROWSE_FOLDER);
    MoveWindow(browseFolder, margin + folderEditW + margin, row1y, btnW, btnH, TRUE);
    HWND refresh = GetDlgItem(hwnd, IDC_REFRESH);
    MoveWindow(refresh, margin + folderEditW + margin + btnW + margin, row1y, btnW, btnH, TRUE);

    const int row2y = row1y + btnH + margin;
    const int listH = std::max(120, (h - row2y - margin * 3 - editH * 6) / 2);
    MoveWindow(g_fileList, margin, row2y, w - margin * 2, listH, TRUE);

    const int row3y = row2y + listH + margin;
    MoveWindow(GetDlgItem(hwnd, IDC_LIST_CONTENTS), margin, row3y, btnW, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_INFO), margin + btnW + margin, row3y, btnW, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXTRACT), margin + (btnW + margin) * 2, row3y, btnW, btnH, TRUE);

    const int row4y = row3y + btnH + margin;
    MoveWindow(GetDlgItem(hwnd, IDC_PASSWORD_LABEL), margin, row4y + 3, 120, editH, TRUE);
    MoveWindow(g_passwordEdit, margin + 125, row4y, std::max(200, w - margin * 2 - 125), editH, TRUE);

    const int logY = row4y + editH + margin;
    const int logH = std::max(80, h - logY - margin);
    MoveWindow(g_log, margin, logY, w - margin * 2, logH, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hwndMain = hwnd;
        InitCommonControlsEx(&g_icc);

        g_folderEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                       WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                       0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FOLDER_EDIT)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Browse…",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BROWSE_FOLDER)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Refresh",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REFRESH)), nullptr, nullptr);

        g_fileList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_EXTENDEDSEL | LBS_NOTIFY,
                                     0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILE_LIST)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"List contents",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST_CONTENTS)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Information",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INFO)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Extract…",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXTRACT)), nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Password (NUNA):",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PASSWORD_LABEL)), nullptr, nullptr);

        g_passwordEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                         0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PASSWORD_EDIT)), nullptr, nullptr);

        g_log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
                                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG)), nullptr, nullptr);

        SendMessageW(g_log, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_folderEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_fileList, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_passwordEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

        LayoutChildren(hwnd);
        AppendLogLine(kTitle);
        AppendLogLine(L"Select a folder, click Refresh, then choose .tre file(s) (Ctrl+click for multiple).");
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            LayoutChildren(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BROWSE_FOLDER:
            BrowseForFolder(hwnd, g_folderEdit);
            return 0;

        case IDC_REFRESH:
            ClearLog();
            RefreshTreList(g_folderEdit, g_fileList);
            return 0;

        case IDC_LIST_CONTENTS:
            ClearLog();
            RunListOnPaths(GetSelectedTrePaths(g_folderEdit, g_fileList));
            return 0;

        case IDC_INFO:
            ClearLog();
            RunInfoOnPaths(GetSelectedTrePaths(g_folderEdit, g_fileList));
            return 0;

        case IDC_EXTRACT:
            ClearLog();
            RunExtractOnPaths(GetSelectedTrePaths(g_folderEdit, g_fileList));
            return 0;

        default:
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int show)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
        return 1;

    RECT want{ 0, 0, 960, 640 };
    AdjustWindowRect(&want, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        kTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        want.right - want.left,
        want.bottom - want.top,
        nullptr,
        nullptr,
        hInst,
        nullptr);

    if (!hwnd)
        return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
