"""Capture a client window's own contents with PrintWindow(PW_RENDERFULLCONTENT), not the screen.

The DevBridge screenshot reads the D3D front buffer, a copy of the adapter's monitor: it comes back black when the
window is minimized or covered by another window (another session's client, the user's apps). PrintWindow with
PW_RENDERFULLCONTENT asks DWM for the window's own redirection surface, so it works while the window sits behind
everything, and it never picks up anything but that window. The window must not be minimized.

    py -3 client/tools/ui_modern/grab_window.py <pid> <out.png> [--background]

--background first restores the window (if minimized), moves it to the primary display's origin and sends it to the
bottom of the z-order without activating it, so it renders but covers nothing the user is looking at. Never park a
test client on another monitor: the user works there.
"""
from __future__ import annotations

import argparse
import ctypes
import time
from ctypes import wintypes

from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

PW_RENDERFULLCONTENT = 0x2
HWND_BOTTOM = 1
SWP_NOSIZE, SWP_NOACTIVATE = 0x0001, 0x0010
SW_SHOWNOACTIVATE = 4


def main_window(pid: int) -> int:
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        p = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.GetWindowTextLengthW(hwnd) > 0:
            buf = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, buf, 256)
            if buf.value.startswith("SWG"):
                found.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    if not found:
        raise SystemExit(f"no SWG window for pid {pid}")
    return found[0]


def to_background(hwnd: int):
    """Primary display, bottom of the z-order, not activated: renders, covers nothing."""
    if user32.IsIconic(hwnd):
        user32.ShowWindow(hwnd, SW_SHOWNOACTIVATE)
    user32.SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE)
    time.sleep(0.5)


def print_window(hwnd: int) -> Image.Image:
    rc = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rc))
    win = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(win))
    w, h = win.right - win.left, win.bottom - win.top
    hdc_win = user32.GetWindowDC(hwnd)
    hdc = gdi32.CreateCompatibleDC(hdc_win)
    bmp = gdi32.CreateCompatibleBitmap(hdc_win, w, h)
    gdi32.SelectObject(hdc, bmp)
    ok = user32.PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG), ("biHeight", wintypes.LONG),
                    ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                    ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                    ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
                    ("biClrImportant", wintypes.DWORD)]

    bih = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc, bmp, 0, h, buf, ctypes.byref(bih), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(hdc)
    user32.ReleaseDC(hwnd, hdc_win)
    if not ok:
        raise SystemExit("PrintWindow failed")
    img = Image.frombuffer("RGB", (w, h), buf, "raw", "BGRX", 0, 1)
    # crop the frame / title bar away: client area offset inside the window rectangle
    pt = wintypes.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    ox, oy = pt.x - win.left, pt.y - win.top
    return img.crop((ox, oy, ox + rc.right, oy + rc.bottom))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pid", type=int)
    ap.add_argument("out")
    ap.add_argument("--background", action="store_true")
    args = ap.parse_args()
    hwnd = main_window(args.pid)
    if args.background:
        to_background(hwnd)
    img = print_window(hwnd)
    img.save(args.out)
    print(args.out, img.size)


if __name__ == "__main__":
    main()
