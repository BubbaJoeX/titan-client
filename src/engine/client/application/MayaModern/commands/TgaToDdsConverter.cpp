#include "TgaToDdsConverter.h"
#include "ConfigFile.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    bool isSupportedImagePath(const std::string& path)
    {
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || dot >= path.size() - 1)
            return false;
        std::string ext = path.substr(dot + 1);
        for (size_t i = 0; i < ext.size(); ++i)
            if (ext[i] >= 'A' && ext[i] <= 'Z')
                ext[i] = static_cast<char>(ext[i] + ('a' - 'A'));
        return ext == "tga" || ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tif" || ext == "tiff";
    }

    std::string replaceExtension(const std::string& path, const std::string& newExt)
    {
        std::string result = path;
        size_t dot = result.find_last_of('.');
        if (dot != std::string::npos && dot > result.find_last_of("/\\"))
            result.resize(dot);
        if (!newExt.empty() && newExt[0] != '.')
            result += '.';
        result += newExt;
        return result;
    }
}

std::string TgaToDdsConverter::getNvttExporterPath()
{
    const char* cfg = ConfigFile::getKeyString("SwgMayaEditor", "nvttExporterPath", "");
    if (cfg && cfg[0])
        return cfg;
#ifdef _WIN32
    return "D:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools\\nvtt_export.exe";
#else
    return "nvtt_export";
#endif
}

std::string TgaToDdsConverter::convertToDds(const std::string& tgaPath,
    const std::string& outputPath,
    const std::string& format,
    int quality)
{
    if (!isSupportedImagePath(tgaPath))
    {
        std::cerr << "[TgaToDds] Unsupported image extension (use tga/png/jpg/bmp): " << tgaPath << "\n";
        return std::string();
    }

    std::string outPath = outputPath;
    if (outPath.empty())
        outPath = replaceExtension(tgaPath, "dds");

    std::string nvttPath = getNvttExporterPath();
    if (nvttPath.empty())
    {
        std::cerr << "[TgaToDds] nvtt_export.exe path not configured\n";
        return std::string();
    }

    // nvtt_export -f ids; BC3 -> DDS DXT5 FourCC (client Texture.cpp supports DXT1/DXT3/DXT5 only).
    int formatId = 18;
    if (format == "bc7") formatId = 23;
    else if (format == "bc1") formatId = 15;
    else if (format == "bc2") formatId = 17;
    else if (format == "bc3" || format == "bc3n") formatId = 18;
    else if (format == "bgra8") formatId = 25;

    std::cerr << "[TgaToDds] Converting: " << tgaPath << " -> " << outPath << "\n";

#ifdef _WIN32
    std::string cmd;
    cmd.reserve(512);
    cmd += '"';
    cmd += nvttPath;
    cmd += "\" -o \"";
    cmd += outPath;
    cmd += "\" -f ";
    cmd += std::to_string(formatId);
    cmd += " -q ";
    cmd += std::to_string(quality);
    cmd += " \"";
    cmd += tgaPath;
    cmd += '"';

    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        std::cerr << "[TgaToDds] CreateProcess failed for: " << nvttPath << "\n";
        return std::string();
    }

    WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0)
    {
        std::cerr << "[TgaToDds] nvtt_export exited with code " << exitCode << "\n";
        return std::string();
    }
#else
    (void)formatId;
    (void)quality;
    std::cerr << "[TgaToDds] TGA->DDS conversion only supported on Windows\n";
    return std::string();
#endif

    std::cerr << "[TgaToDds] Converted OK: " << outPath << "\n";
    return outPath;
}
