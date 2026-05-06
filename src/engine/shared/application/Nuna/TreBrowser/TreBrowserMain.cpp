// ======================================================================
// TreBrowser — Win32 GUI: merged .tre tree, overlay history, selective extract
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
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
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
    IDC_FOLDER_EDIT = 2001,
    IDC_BROWSE_FOLDER,
    IDC_REFRESH,
    IDC_MERGE_ALL,
    IDC_FILE_LIST,
    IDC_LIST_CONTENTS,
    IDC_INFO,
    IDC_EXTRACT_FILE,
    IDC_EXTRACT_FOLDER,
    IDC_EXTRACT_ARCHIVE,
    IDC_PASSWORD_LABEL,
    IDC_PASSWORD_EDIT,
    IDC_DETAILS,
    IDC_LOG,
    IDC_TREE,
};

HWND g_hwndMain = nullptr;
HWND g_folderEdit = nullptr;
HWND g_fileList = nullptr;
HWND g_mergeAllCheck = nullptr;
HWND g_tree = nullptr;
HWND g_details = nullptr;
HWND g_log = nullptr;
HWND g_passwordEdit = nullptr;

// ----- Overlay model (normalized path -> stack of layers, bottom .. top) -----

struct OverlayLayer
{
    fs::path trePath;
    Nuna::TocEntry entry{};
};

struct PathOverlay
{
    std::string displayPath;
    std::vector<OverlayLayer> layers;
};

std::unordered_map<std::string, PathOverlay> g_overlay;

// ----- Tree item payload -----

enum class TreeKind : int
{
    Folder,
    File
};

struct TreeItemData
{
    TreeKind kind = TreeKind::Folder;
    std::wstring label;
    std::string normFolderPrefix;
    std::string normFileKey;
};

struct TrieNode
{
    std::map<std::string, TrieNode> subdirs;
    std::vector<std::pair<std::string, std::string>> filesHere;
};

std::vector<TreeItemData*> g_treeAlloc;
INITCOMMONCONTROLSEX g_icc = { sizeof(INITCOMMONCONTROLSEX),
                                ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES };

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

void ClearDetails()
{
    if (g_details)
        SetWindowTextW(g_details, L"");
}

void AppendDetailsUtf8(std::string_view text)
{
    if (!g_details)
        return;
    std::wstring w = Utf8ToWide(std::string(text));
    const int end = GetWindowTextLengthW(g_details);
    SendMessageW(g_details, EM_SETSEL, static_cast<WPARAM>(end), static_cast<LPARAM>(end));
    SendMessageW(g_details, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(w.c_str()));
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

// Match Nuna::normalizePath (pack) so keys align with extractOne / TOC paths.
std::string NormalizeTrePath(const std::string& path)
{
    std::string result;
    result.reserve(path.size());
    const char* f = path.c_str();
    while (*f == '\\' || *f == '/')
        ++f;
    while (f[0] == '.' && (f[1] == '\\' || f[1] == '/'))
        f += 2;
    while (f[0] == '.' && f[1] == '.' && (f[2] == '\\' || f[2] == '/'))
        f += 3;
    bool previousIsSlash = false;
    for (; *f; ++f)
    {
        const char c = *f;
        const bool currentIsSlash = (c == '\\' || c == '/');
        if (currentIsSlash)
        {
            if (!previousIsSlash)
            {
                result += '/';
                previousIsSlash = true;
            }
        }
        else
        {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            previousIsSlash = false;
        }
    }
    return result;
}

std::vector<std::string> SplitPathSegments(const std::string& normPath)
{
    std::vector<std::string> parts;
    size_t i = 0;
    while (i < normPath.size())
    {
        while (i < normPath.size() && normPath[i] == '/')
            ++i;
        if (i >= normPath.size())
            break;
        const size_t j = normPath.find('/', i);
        if (j == std::string::npos)
        {
            parts.push_back(normPath.substr(i));
            break;
        }
        parts.push_back(normPath.substr(i, j - i));
        i = j + 1;
    }
    return parts;
}

void TrieInsert(TrieNode& root, const std::string& normPath)
{
    const std::vector<std::string> parts = SplitPathSegments(normPath);
    if (parts.empty())
        return;
    TrieNode* cur = &root;
    for (size_t i = 0; i + 1 < parts.size(); ++i)
        cur = &cur->subdirs[parts[i]];
    cur->filesHere.emplace_back(parts.back(), normPath);
}

void FreeTreeItems()
{
    for (TreeItemData* p : g_treeAlloc)
        delete p;
    g_treeAlloc.clear();
    if (g_tree)
        TreeView_DeleteAllItems(g_tree);
}

std::string CaptureStdout(const std::function<void()>& fn)
{
    std::ostringstream oss;
    std::streambuf* const old = std::cout.rdbuf(oss.rdbuf());
    fn();
    std::cout.rdbuf(old);
    return oss.str();
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

fs::path GetFolderPathFromEdit(HWND folderEdit)
{
    std::wstring ws = GetWindowTextStr(folderEdit);
    if (ws.empty())
        return {};
    return fs::path(ws);
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

std::vector<fs::path> GetAllTreSortedInFolder(HWND folderEdit)
{
    std::vector<fs::path> paths;
    fs::path root = GetFolderPathFromEdit(folderEdit);
    if (root.empty())
        return paths;

    std::error_code ec;
    for (const fs::directory_entry& ent : fs::directory_iterator(root, ec))
    {
        if (!ent.is_regular_file(ec))
            continue;
        const std::wstring fname = ent.path().filename().wstring();
        if (EndsWithTreInsensitive(fname))
            paths.push_back(ent.path());
    }
    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b)
              { return a.filename().wstring() < b.filename().wstring(); });
    return paths;
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
    }

    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b)
              { return a.filename().wstring() < b.filename().wstring(); });
    return paths;
}

std::vector<fs::path> TrePathsForListOperation()
{
    if (g_mergeAllCheck && SendMessageW(g_mergeAllCheck, BM_GETCHECK, 0, 0) == BST_CHECKED)
        return GetAllTreSortedInFolder(g_folderEdit);
    std::vector<fs::path> sel = GetSelectedTrePaths(g_folderEdit, g_fileList);
    if (sel.empty())
        sel = GetAllTreSortedInFolder(g_folderEdit);
    return sel;
}

bool PathIsUnderFolderPrefix(const std::string& normPath, const std::string& normFolderPrefix)
{
    if (normFolderPrefix.empty())
        return true;
    if (normPath.size() < normFolderPrefix.size())
        return false;
    if (normPath.compare(0, normFolderPrefix.size(), normFolderPrefix) != 0)
        return false;
    if (normPath.size() == normFolderPrefix.size())
        return true;
    return normPath[normFolderPrefix.size()] == '/';
}

void BuildOverlayFromTres(const std::vector<fs::path>& treOrder)
{
    g_overlay.clear();

    Nuna::ListOptions opts;
    opts.showSize = false;
    opts.showCompressed = false;
    opts.showOffset = false;
    opts.encryption = PasswordOptionsFromUi();

    for (const fs::path& tre : treOrder)
    {
        std::vector<std::pair<std::string, Nuna::TocEntry>> entries;
        CaptureStdout([&]()
                      {
                          const Nuna::Result r = Nuna::list(tre.u8string(), opts, &entries);
                          if (!r.ok())
                              std::cout << r.message << "\n";
                      });

        for (const auto& pr : entries)
        {
            const std::string nk = NormalizeTrePath(pr.first);
            if (nk.empty())
                continue;
            PathOverlay& po = g_overlay[nk];
            po.displayPath = pr.first;
            OverlayLayer layer;
            layer.trePath = tre;
            layer.entry = pr.second;
            po.layers.push_back(std::move(layer));
        }
    }
}

HTREEITEM InsertTreeItem(HWND tv, HTREEITEM parent, TreeItemData* data)
{
    g_treeAlloc.push_back(data);
    TVINSERTSTRUCTW ins{};
    ins.hParent = parent;
    ins.hInsertAfter = TVI_SORT;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = data->label.data();
    ins.item.lParam = reinterpret_cast<LPARAM>(data);
    return TreeView_InsertItem(tv, &ins);
}

void FillTreeRecursive(HWND tv, HTREEITEM parent, TrieNode& node, const std::string& prefixNorm)
{
    for (auto& p : node.subdirs)
    {
        std::string childPrefix = prefixNorm.empty() ? p.first : prefixNorm + "/" + p.first;
        auto* d = new TreeItemData;
        d->kind = TreeKind::Folder;
        d->label = Utf8ToWide(p.first);
        d->normFolderPrefix = childPrefix;
        const HTREEITEM h = InsertTreeItem(tv, parent, d);
        FillTreeRecursive(tv, h, p.second, childPrefix);
    }
    for (const auto& fe : node.filesHere)
    {
        auto* d = new TreeItemData;
        d->kind = TreeKind::File;
        d->label = Utf8ToWide(fe.first);
        d->normFileKey = fe.second;
        InsertTreeItem(tv, parent, d);
    }
}

void PopulateArchiveTree(const std::vector<fs::path>& treOrder)
{
    FreeTreeItems();
    BuildOverlayFromTres(treOrder);

    TrieNode root;
    std::vector<std::string> sortedKeys;
    sortedKeys.reserve(g_overlay.size());
    for (const auto& kv : g_overlay)
        sortedKeys.push_back(kv.first);
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (const std::string& k : sortedKeys)
        TrieInsert(root, k);

    FillTreeRecursive(g_tree, TVI_ROOT, root, std::string{});

    std::ostringstream oss;
    oss << "Merged paths: " << g_overlay.size() << " (from " << treOrder.size() << " archive(s)).\r\n";
    oss << "Overlay order is ascending by file name; later archives replace earlier ones for the same path.\r\n";
    AppendLogUtf8(oss.str());
}

void ShowSelectionDetails(TreeItemData* d)
{
    ClearDetails();
    if (!d)
    {
        AppendDetailsUtf8("Select a folder or file in the tree.\r\n");
        return;
    }
    if (d->kind == TreeKind::Folder)
    {
        std::ostringstream o;
        o << "[Folder]\r\nNormalized prefix: " << d->normFolderPrefix << "\r\n";
        o << "Use \"Extract folder\" to extract all files under this branch (active layer per file).\r\n";
        AppendDetailsUtf8(o.str());
        return;
    }

    const auto it = g_overlay.find(d->normFileKey);
    if (it == g_overlay.end())
    {
        AppendDetailsUtf8("Internal error: path not in overlay map.\r\n");
        return;
    }

    const PathOverlay& po = it->second;
    std::ostringstream o;
    o << "Path: " << po.displayPath << "\r\n";
    o << "Normalized key: " << d->normFileKey << "\r\n\r\n";

    if (po.layers.empty())
    {
        AppendDetailsUtf8(o.str());
        return;
    }

    const OverlayLayer& win = po.layers.back();
    o << "Active layer (winner): " << win.trePath.filename().u8string() << "\r\n";
    o << "  length=" << win.entry.length << "  compressed=" << win.entry.compressedLength
      << "  offset=" << win.entry.offset << "\r\n\r\n";

    o << "Version history (bottom = earliest archive, top = winner):\r\n";
    for (size_t i = 0; i < po.layers.size(); ++i)
    {
        const OverlayLayer& L = po.layers[i];
        o << "  [" << (i + 1) << "] " << L.trePath.filename().u8string()
          << "  len=" << L.entry.length << "  off=" << L.entry.offset << "\r\n";
    }
    AppendDetailsUtf8(o.str());
}

TreeItemData* GetSelectedTreeData()
{
    if (!g_tree)
        return nullptr;
    HTREEITEM h = TreeView_GetSelection(g_tree);
    if (!h)
        return nullptr;
    TVITEMW ti{};
    ti.hItem = h;
    ti.mask = TVIF_PARAM;
    if (!TreeView_GetItem(g_tree, &ti))
        return nullptr;
    return reinterpret_cast<TreeItemData*>(ti.lParam);
}

bool PickFolder(HWND owner, const wchar_t* title, std::wstring& outPath)
{
    BROWSEINFOW bi{};
    wchar_t display[MAX_PATH]{};
    bi.hwndOwner = owner;
    bi.pszDisplayName = display;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
        return false;
    wchar_t buf[MAX_PATH]{};
    const bool ok = SHGetPathFromIDListW(pidl, buf);
    CoTaskMemFree(pidl);
    if (!ok)
        return false;
    outPath = buf;
    return true;
}

bool BrowseFolderIntoEdit(HWND owner, HWND targetEdit)
{
    std::wstring p;
    if (!PickFolder(owner, L"Select folder containing .tre files", p))
        return false;
    SetWindowTextW(targetEdit, p.c_str());
    return true;
}

void RunListTree()
{
    ClearLog();
    const std::vector<fs::path> tres = TrePathsForListOperation();
    if (tres.empty())
    {
        AppendLogLine(L"No .tre archives in scope. Select archives in the list or enable \"Merge all\".");
        FreeTreeItems();
        return;
    }

    AppendLogUtf8("Building merged tree...\r\n");
    PopulateArchiveTree(tres);
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

void RunExtractArchive()
{
    ClearLog();
    std::vector<fs::path> paths = GetSelectedTrePaths(g_folderEdit, g_fileList);
    if (paths.empty())
        paths = GetAllTreSortedInFolder(g_folderEdit);
    if (paths.empty())
    {
        AppendLogLine(L"No .tre files to extract.");
        return;
    }

    std::wstring outRoot;
    if (!PickFolder(g_hwndMain, L"Select folder to extract full archive(s) into", outRoot))
        return;

    Nuna::UnpackOptions uo;
    uo.overwrite = true;
    uo.quiet = true;
    uo.encryption = PasswordOptionsFromUi();

    for (const fs::path& trePath : paths)
    {
        const std::wstring stem = trePath.stem().wstring();
        fs::path dest = fs::path(outRoot) / stem;

        std::error_code ec;
        fs::create_directories(dest, ec);
        if (ec)
        {
            AppendLogLine(L"Could not create folder:");
            AppendLogLine(dest.wstring());
            continue;
        }

        AppendLogUtf8(std::string("Extract archive ") + trePath.u8string() + "\r\n");
        const Nuna::Result ur = Nuna::unpack(trePath.u8string(), dest.u8string(), uo);
        if (!ur.ok())
            AppendLogUtf8(std::string("Error: ") + ur.message + "\r\n");
        else
            AppendLogUtf8(ur.message + "\r\n");
    }
}

void RunExtractSingleFile()
{
    TreeItemData* d = GetSelectedTreeData();
    if (!d || d->kind != TreeKind::File)
    {
        AppendLogLine(L"Select a file in the tree (leaf node).");
        return;
    }

    const auto it = g_overlay.find(d->normFileKey);
    if (it == g_overlay.end() || it->second.layers.empty())
    {
        AppendLogLine(L"Nothing to extract for this path.");
        return;
    }

    const fs::path winnerTre = it->second.layers.back().trePath;
    const std::string internalPath = it->second.displayPath;

    std::wstring outDir;
    if (!PickFolder(g_hwndMain, L"Select folder to place the extracted file", outDir))
        return;

    fs::path rel(internalPath);
    fs::path baseName = rel.filename();
    if (baseName.empty())
    {
        AppendLogLine(L"Invalid internal path.");
        return;
    }

    const fs::path outFile = fs::path(outDir) / baseName;

    std::error_code ec;
    fs::create_directories(outFile.parent_path(), ec);

    Nuna::UnpackOptions uo;
    uo.overwrite = true;
    uo.quiet = true;
    uo.encryption = PasswordOptionsFromUi();

    const Nuna::Result r =
        Nuna::extractOne(winnerTre.u8string(), internalPath, outFile.u8string(), uo);
    if (!r.ok())
        AppendLogUtf8(std::string("Extract file: ") + r.message + "\r\n");
    else
        AppendLogUtf8(std::string("Wrote: ") + outFile.u8string() + "\r\n");
}

void CollectFilesUnderFolderPrefix(const std::string& normFolderPrefix, std::vector<std::string>& outKeys)
{
    for (const auto& kv : g_overlay)
    {
        if (kv.second.layers.empty())
            continue;
        if (!PathIsUnderFolderPrefix(kv.first, normFolderPrefix))
            continue;
        outKeys.push_back(kv.first);
    }
}

void RunExtractFolder()
{
    TreeItemData* d = GetSelectedTreeData();
    if (!d)
    {
        AppendLogLine(L"Select a folder or file in the tree.");
        return;
    }

    std::string normPrefix;
    if (d->kind == TreeKind::Folder)
        normPrefix = d->normFolderPrefix;
    else
    {
        const std::string k = d->normFileKey;
        const size_t slash = k.find_last_of('/');
        normPrefix = (slash == std::string::npos) ? std::string{} : k.substr(0, slash);
    }

    std::vector<std::string> keys;
    CollectFilesUnderFolderPrefix(normPrefix, keys);
    if (keys.empty())
    {
        AppendLogLine(L"No files under this selection.");
        return;
    }

    std::wstring outRoot;
    if (!PickFolder(g_hwndMain, L"Select output folder (archive paths recreated beneath it)", outRoot))
        return;

    Nuna::UnpackOptions uo;
    uo.overwrite = true;
    uo.quiet = true;
    uo.encryption = PasswordOptionsFromUi();

    uint32_t okCount = 0;
    for (const std::string& key : keys)
    {
        const auto it = g_overlay.find(key);
        if (it == g_overlay.end() || it->second.layers.empty())
            continue;
        const fs::path winnerTre = it->second.layers.back().trePath;
        const std::string internalPath = it->second.displayPath;
        const fs::path outPath = fs::path(outRoot) / fs::path(internalPath);

        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);

        const Nuna::Result r = Nuna::extractOne(winnerTre.u8string(), internalPath, outPath.u8string(), uo);
        if (r.ok())
            ++okCount;
        else
            AppendLogUtf8(std::string("FAIL ") + internalPath + " : " + r.message + "\r\n");
    }

    AppendLogUtf8(std::string("Folder extract finished. Files written: ") + std::to_string(okCount) + "\r\n");
}

void LayoutChildren(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    const int margin = 8;
    const int btnW = 112;
    const int btnH = 26;
    const int editH = 22;
    const int treListH = 84;

    int y = margin;

    const int folderEditW = std::max(120, cw - margin * 4 - btnW * 2);
    MoveWindow(g_folderEdit, margin, y, folderEditW, editH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BROWSE_FOLDER), margin + folderEditW + margin, y, btnW, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_REFRESH), margin + folderEditW + margin + btnW + margin, y, btnW, btnH, TRUE);
    y += btnH + margin;

    MoveWindow(g_mergeAllCheck, margin, y + 2, 340, editH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PASSWORD_LABEL), margin + 360, y + 3, 130, editH, TRUE);
    MoveWindow(g_passwordEdit, margin + 490, y, std::max(160, cw - margin - 490), editH, TRUE);
    y += editH + margin;

    MoveWindow(g_fileList, margin, y, cw - margin * 2, treListH, TRUE);
    y += treListH + margin;

    const int btnRowW = btnW + margin;
    MoveWindow(GetDlgItem(hwnd, IDC_LIST_CONTENTS), margin, y, btnW, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_INFO), margin + btnRowW, y, btnW, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXTRACT_FILE), margin + btnRowW * 2, y, btnW + 18, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXTRACT_FOLDER), margin + btnRowW * 3 + 18, y, btnW + 24, btnH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXTRACT_ARCHIVE), margin + btnRowW * 4 + 42, y, btnW + 24, btnH, TRUE);
    y += btnH + margin;

    const int splitLeft = std::max(200, cw * 38 / 100);
    const int midH = std::max(120, ch - y - margin);
    const int rightW = cw - margin * 3 - splitLeft;
    const int detailsH = std::max(80, midH * 52 / 100);

    MoveWindow(g_tree, margin, y, splitLeft, midH, TRUE);
    MoveWindow(g_details, margin + splitLeft + margin, y, rightW, detailsH, TRUE);
    MoveWindow(g_log, margin + splitLeft + margin, y + detailsH + margin, rightW,
               std::max(60, midH - detailsH - margin), TRUE);
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

        g_mergeAllCheck = CreateWindowW(L"BUTTON", L"Merge all .tre in folder",
                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MERGE_ALL)), nullptr, nullptr);

        g_fileList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                       WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_EXTENDEDSEL | LBS_NOTIFY,
                                       0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILE_LIST)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"List contents",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST_CONTENTS)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Information",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INFO)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Extract file",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXTRACT_FILE)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Extract folder",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXTRACT_FOLDER)), nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Extract archive",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXTRACT_ARCHIVE)), nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Password (NUNA):",
                      WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PASSWORD_LABEL)), nullptr, nullptr);

        g_passwordEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                         0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PASSWORD_EDIT)), nullptr, nullptr);

        g_tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW,
                                 L"",
                                 WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_TABSTOP,
                                 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TREE)), nullptr, nullptr);

        g_details = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
                                      0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DETAILS)), nullptr, nullptr);

        g_log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
                                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG)), nullptr, nullptr);

        SendMessageW(g_tree, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_log, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_details, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_folderEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_fileList, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(g_passwordEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

        LayoutChildren(hwnd);
        AppendLogLine(kTitle);
        AppendLogLine(L"Refresh loads .tre names. Choose merge mode, List contents for unified tree, select nodes for details.");
        AppendDetailsUtf8("Build the tree with \"List contents\", then click items for overlay history.\r\n");
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            LayoutChildren(hwnd);
        return 0;

    case WM_NOTIFY:
        if (reinterpret_cast<LPNMHDR>(lParam)->hwndFrom == g_tree)
        {
            if (reinterpret_cast<LPNMHDR>(lParam)->code == TVN_SELCHANGED)
            {
                const auto* nmt = reinterpret_cast<LPNMTREEVIEW>(lParam);
                auto* d = reinterpret_cast<TreeItemData*>(nmt->itemNew.lParam);
                ShowSelectionDetails(d);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BROWSE_FOLDER:
            BrowseFolderIntoEdit(hwnd, g_folderEdit);
            return 0;

        case IDC_REFRESH:
            ClearLog();
            RefreshTreList(g_folderEdit, g_fileList);
            return 0;

        case IDC_LIST_CONTENTS:
            RunListTree();
            return 0;

        case IDC_INFO:
            ClearLog();
            {
                std::vector<fs::path> ip = GetSelectedTrePaths(g_folderEdit, g_fileList);
                if (ip.empty())
                    ip = GetAllTreSortedInFolder(g_folderEdit);
                RunInfoOnPaths(ip);
            }
            return 0;

        case IDC_EXTRACT_FILE:
            ClearLog();
            RunExtractSingleFile();
            return 0;

        case IDC_EXTRACT_FOLDER:
            ClearLog();
            RunExtractFolder();
            return 0;

        case IDC_EXTRACT_ARCHIVE:
            RunExtractArchive();
            return 0;

        default:
            break;
        }
        return 0;

    case WM_DESTROY:
        FreeTreeItems();
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

    RECT want{ 0, 0, 1100, 720 };
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
