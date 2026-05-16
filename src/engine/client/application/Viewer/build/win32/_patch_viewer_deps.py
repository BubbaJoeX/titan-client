import re

vcxproj = r"d:\titan\client\src\engine\client\application\Viewer\build\win32\Viewer.vcxproj"
text = open(vcxproj, encoding="utf-8", errors="replace").read()

m = re.search(
    r"<ItemDefinitionGroup Condition=\"'\$\(Configuration\)\|\$\(Platform\)'=='Release\|Win32'\">.*?<Link>\s*<AdditionalDependencies>([^<]+)</AdditionalDependencies>",
    text,
    re.DOTALL,
)
if not m:
    raise SystemExit("Release AdditionalDependencies not found")
dep = m.group(1)


def to_debug(s: str) -> str:
    s = (
        s.replace("gl05_r.lib", "gl05_d.lib")
        .replace("gl06_r.lib", "gl06_d.lib")
        .replace("gl07_r.lib", "gl07_d.lib")
    )
    s = s.replace("libxml2-win32-release.lib", "libxml2-win32-debug.lib")
    s = s.replace("CaptureCommon_release.lib", "CaptureCommon_debug.lib")
    s = s.replace("ImageCapture_release.lib", "ImageCapture_debug.lib")
    # Single occurrence in Viewer Release deps chain
    s = s.replace("picn20m.lib", "picn20md.lib", 1)
    s = s.replace("Smart_release.lib", "Smart_debug.lib").replace(
        "SoeUtil_release.lib", "SoeUtil_debug.lib"
    )
    s = s.replace("VideoCapture_release.lib", "VideoCapture_debug.lib").replace(
        "ZlibUtil_release.lib", "ZlibUtil_debug.lib"
    )
    s = s.replace("vivoxSharedWrapper_Release.lib", "vivoxSharedWrapper_Debug.lib")
    return s


def to_optimized_gl(s: str) -> str:
    return (
        s.replace("gl05_r.lib", "gl05_o.lib")
        .replace("gl06_r.lib", "gl06_o.lib")
        .replace("gl07_r.lib", "gl07_o.lib")
    )


open(
    r"d:\titan\client\src\engine\client\application\Viewer\build\win32\viewer_deps_debug_gen.txt",
    "w",
    encoding="utf-8",
).write(to_debug(dep))
open(
    r"d:\titan\client\src\engine\client\application\Viewer\build\win32\viewer_deps_optimized_gen.txt",
    "w",
    encoding="utf-8",
).write(to_optimized_gl(dep))
print("ok", len(dep))

swg = r"d:\titan\client\src\game\client\application\SwgClient\build\win32\SwgClient.vcxproj"
swg_text = open(swg, encoding="utf-8", errors="replace").read()


def extract_lib_dirs(cfg: str) -> str:
    start = f"<ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='{cfg}|Win32'\">"
    i = swg_text.find(start)
    if i < 0:
        raise SystemExit(f"SwgClient section {cfg} not found")
    j = swg_text.find("</ItemDefinitionGroup>", i)
    block = swg_text[i:j]
    m = re.search(r"<AdditionalLibraryDirectories>([^<]+)</AdditionalLibraryDirectories>", block)
    if not m:
        raise SystemExit(f"SwgClient {cfg} AdditionalLibraryDirectories not found")
    return m.group(1)


for cfg in ("Debug", "Optimized"):
    pth = (
        r"d:\titan\client\src\engine\client\application\Viewer\build\win32"
        rf"\viewer_libdirs_{cfg.lower()}.txt"
    )
    open(pth, "w", encoding="utf-8").write(extract_lib_dirs(cfg))
    print("libdirs", cfg, "ok")

