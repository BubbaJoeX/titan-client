"""Titan loading screens: write the three SwgCuiLoadingSpace pages with one modern layout.

    py -3 client/tools/ui_modern/loading_pages.py

SpaceLoading is the first screen after sign-in (message of the day) and every space zone; MustaLoading and HothLoading
are the Mustafar and Hoth variants. All three keep the CodeData objects SwgCuiLoadingSpace reads (pie, textProgress,
screenshot, screenshotname, Text, backbutton, buttonEsc, default), so the C++ side is unchanged.

Layout, in 1024x768 design units on a page that fills the screen at any resolution and UI scale:
  - a centred card: the picture on the left, title and message on the right (Hoth has no picture: the text card
    is centred alone);
  - a thin accent progress ring with the status line under it, centred at the bottom;
  - Exit at the bottom left;
  - the backdrop: flat slate for SpaceLoading, the planet art dimmed for Mustafar and Hoth.
Re-running rewrites the pages from scratch (idempotent).
"""
from __future__ import annotations

from pathlib import Path

UI = Path(__file__).resolve().parents[2] / "datasources" / "ui"

ACCENT = "#5AB0F5"      # palette contrast1
TRACK = "#22303E"
BACKDROP = "#0B1016"
TEXT_DIM = "#9FB2C6"

PIE_STYLE = "/Styles.space.Pie.Res.style"     # retargeted to a solid patch in theme_spec (only loading pages use it)
PANEL = "/Styles.New.thin_in.rs_default"
BUTTON = "/Styles.New.buttons.hud.style"

ART_TILES = (("nw", 0, 0, 512, 512), ("ne", 512, 0, 512, 512), ("sw", 0, 512, 512, 256), ("se", 512, 512, 512, 256))

PAGES = [
    # file, page name, background page name (CodeData 'default'), art prefix or None, has a picture
    ("ui_spaceloading.inc", "SpaceLoading", "loading_screen", None, True),
    ("ui_load_mustafar.inc", "MustaLoading", "toow_loading_screen", "loading/toow/ui_toow_loading_screen", True),
    ("ui_load_hoth.inc", "HothLoading", "hoth_loading_screen", "loading/hoth/ui_hoth_loading_screen", False),
]


def element(tag: str, attrs: dict, indent: int, children: list[str] | None = None, text: str | None = None) -> list[str]:
    tab = "\t" * indent
    lines = [f"{tab}<{tag}"]
    for k in sorted(attrs, key=str.lower):
        lines.append(f"{tab}\t{k}='{attrs[k]}'")
    if children:
        lines.append(f"{tab}>")
        for c in children:
            lines.append(c)
        lines.append(f"{tab}</{tag}>")
    elif text is not None:
        lines.append(f"{tab}>{text}</{tag}>")
    else:
        lines.append(f"{tab}/>")
    return ["\n".join(lines)]


def rect(x: int, y: int, w: int, h: int) -> dict:
    return {"Location": f"{x},{y}", "Size": f"{w},{h}", "ScrollExtent": f"{w},{h}"}


def page_text(page: str, background: str, art: str | None, has_picture: bool) -> str:
    i = 3      # depth of the children of 'all'
    out: list[str] = []

    # card geometry: picture 470 + gap 16 + text 414 = 900 wide, centred; Hoth centres the text card alone
    top, height = 128, 470
    text_x, text_w = (548, 414) if has_picture else (262, 500)

    ring = {"Name": "pie", **rect(492, 652, 40, 40), "PackLocation": "cfc,fff", "PackSize": "f,f",
            "Style": PIE_STYLE, "PieInnerRadiusType": "Fixed", "PieInnerRadius": "17", "PieWinding": "cw",
            "PieColor": ACCENT, "PieOpacity": "1.00", "PieValue": "0.00"}
    out += element("Pie", ring, i)
    out += element("Pie", {**ring, "Name": "ringTrack", "PieColor": TRACK, "PieValue": "1.00"}, i)
    out += element("Text", {"Name": "textprogress", **rect(262, 702, 500, 18), "PackLocation": "cfc,fff",
                            "PackSize": "f,f", "Font": "bold_13", "TextColor": TEXT_DIM, "TextAlignment": "Center",
                            "MaxLines": "1"}, i)
    out += element("Button", {"Name": "buttonPrev", **rect(40, 714, 140, 28), "PackLocation": "nfn,fff",
                              "PackSize": "f,f", "Style": BUTTON, "TextCapital": "true"}, i, text="@exit")

    body = []
    body += element("Text", {"Name": "kicker", **rect(20, 18, text_w - 40, 14), "PackLocation": "nfn,nfn",
                             "PackSize": "a,f", "Font": "bold_11", "TextColor": ACCENT, "PalText": "contrast1",
                             "PreLocalized": "true", "LocalText": "NOW LOADING"}, i + 1)
    body += element("Text", {"Name": "screenshotName", **rect(20, 34, text_w - 40, 30), "PackLocation": "nfn,nfn",
                             "PackSize": "a,f", "Font": "bold_20", "TextColor": "#8CC4F2", "PalText": "header",
                             "MaxLines": "1", "LocalText": "Galaxy news"}, i + 1)
    body += element("Page", {"Name": "rule", **rect(20, 70, text_w - 40, 1), "PackLocation": "nfn,nfn",
                             "PackSize": "a,f", "BackgroundColor": "#40546C", "BackgroundOpacity": "1.00"}, i + 1)
    body += element("Text", {"Name": "text", **rect(20, 86, text_w - 52, height - 106), "PackLocation": "nfn,nfn",
                             "PackSize": "a,a", "Font": "bold_13", "TextColor": "#D3DDE8", "PalText": "text1",
                             "LocalText": ""}, i + 1)
    body += element("Scrollbar", {"Name": "scroll", **rect(text_w - 22, 86, 8, height - 106),
                                  "PackLocation": "fff,nfn", "PackSize": "f,a",
                                  "Style": "/Styles.scrollbar.default.vHud3.style"}, i + 1)
    # A page's Color tints everything inside it, so each panel is its own page under the content it frames.
    def panel(name: str, x: int) -> list[str]:
        return element("Page", {"Name": name, **rect(x, top, text_w if name == "textPanel" else 470, height),
                                "PackLocation": "cfc,cfc", "PackSize": "f,f", "RStyleDefault": PANEL,
                                "Color": "#33465C", "PalColor": "back1"}, i)

    out += element("Page", {"Name": "textbg", **rect(text_x, top, text_w, height), "PackLocation": "cfc,cfc",
                            "PackSize": "f,f"}, i, body)
    out += panel("textPanel", text_x)

    if has_picture:
        shot = element("Image", {"Name": "shot", **rect(1, 1, 468, height - 2), "PackLocation": "nfn,nfn",
                                 "PackSize": "f,f", "SourceRect": "0,0,512,512"}, i + 1)
        out += element("Page", {"Name": "screenShot", **rect(62, top, 470, height), "PackLocation": "cfc,cfc",
                                "PackSize": "f,f"}, i, shot)
        out += panel("shotPanel", 62)

    backdrop = []
    if art:
        # the planet art fills the screen proportionally, under a dark veil so the card stays readable
        backdrop += element("Page", {"Name": "veil", **rect(0, 0, 1024, 768), "PackLocation": "nfn,nfn",
                                     "PackSize": "a,a", "BackgroundColor": "#000000",
                                     "BackgroundOpacity": "0.62"}, i + 1)
        for name, x, y, w, h in ART_TILES:
            backdrop += element("Image", {"Name": name, **rect(x, y, w, h), "PackLocation": "pfp,pfp",
                                          "PackSize": "p,p", "SourceRect": f"0,0,{w},{h}",
                                          "SourceResource": f"{art}_{name}"}, i + 1)
    out += element("Page", {"Name": background, **rect(0, 0, 1024, 768), "PackLocation": "nfn,nfn",
                            "PackSize": "a,a", "BackgroundColor": BACKDROP, "BackgroundOpacity": "1.00",
                            "Visible": "false"}, i, backdrop or None)
    out += element("Button", {"Name": "buttonEsc", **rect(40, 714, 140, 28), "PackLocation": "nfn,fff",
                              "PackSize": "f,f", "Style": BUTTON, "TextCapital": "true", "Opacity": "0.00"}, i,
                   text="@exit")

    code = {"Name": "CodeData", "backbutton": "all.buttonprev", "buttonEsc": "all.buttonEsc",
            "default": f"all.{background}", "Pie": "all.pie", "screenshotname": "all.textbg.screenshotname",
            "Text": "all.textbg.text", "textProgress": "all.textprogress",
            "screenshot": "all.screenshot.shot" if has_picture else "dummyimage"}
    root_children = element("Data", code, 2)
    root_children += element("Page", {"Name": "all", **rect(0, 0, 1024, 768), "PackLocation": "nfn,nfn",
                                      "PackSize": "a,a"}, 2, out)
    root_children += element("Page", {"Name": "back_black", **rect(0, 0, 1024, 768), "PackLocation": "nfn,nfn",
                                      "PackSize": "a,a", "BackgroundColor": "#000000",
                                      "BackgroundOpacity": "1.00"}, 2)
    if not has_picture:
        root_children += element("Image", {"Name": "dummyimage", "Visible": "false"}, 2)
    root = element("Page", {"Name": page, **rect(0, 0, 1024, 768), "PackLocation": "nfn,nfn", "PackSize": "a,a",
                            "Visible": "false"}, 1, root_children)
    return "\n".join(root) + "\n"


def main():
    for file, page, background, art, has_picture in PAGES:
        path = UI / file
        new = page_text(page, background, art, has_picture).replace("\n", "\r\n")
        old = path.read_bytes().decode("latin1") if path.exists() else ""
        if old != new:
            path.write_bytes(new.encode("latin1"))
            print(f"wrote {path.name}")
        else:
            print(f"unchanged {path.name}")


if __name__ == "__main__":
    main()
