"""Deploy the UI source and the theme textures to the folder the clients actually read.

SwgTitan_d / SwgTitan_r / SwgGodClient run from exe/win64_rel, and TreeFile's SearchAbsolute node (added above every
search path, TreeFile.cpp install) resolves 'ui/...' and 'texture/...' relative to that working directory first. So
the live UI is the loose exe/win64_rel/ui folder - a mirror of client/datasources/ui that UI work copies across -
and anything in exe/win64_rel/texture beats the .tre / titanpak copies. Nothing else needs to change.

    py -3 client/tools/ui_modern/publish_ui.py [--dry-run]     # mirror changed files
    py -3 client/tools/ui_modern/publish_ui.py --revert        # put back everything this script replaced

Copies
  client/datasources/ui/**              -> exe/win64_rel/ui/**        (changed files only)
  client/tools/ui_modern/out/texture/*  -> exe/win64_rel/texture/*    (make_theme.py output)

The first time a file is replaced its old copy goes to exe/win64_rel/ui.pre-uimodern/ (texture.pre-uimodern/), and a
file that didn't exist is recorded there as <name>.absent, so --revert restores the exact previous state. The copy
runs under the client-build lock (D:/titan/.locks/client-build), like every other deploy into exe/win64_rel.
Running clients keep their loaded UI; relaunch, or '/ui reset' with titanAdmin, to see changes.
"""
from __future__ import annotations

import argparse
import datetime
import filecmp
import shutil
import sys
from pathlib import Path

TITAN = Path("D:/titan")
EXE = TITAN / "exe/win64_rel"
LOCK = TITAN / ".locks/client-build"
HERE = Path(__file__).resolve().parent
PAIRS = [
    (TITAN / "client/datasources/ui", EXE / "ui", "**/*"),
    (HERE / "out/texture", EXE / "texture", "*.dds"),
]
SKIP = {"ui.log"}


def backup_dir(dst_root: Path) -> Path:
    return dst_root.with_name(dst_root.name + ".pre-uimodern")


SESSION = "0226ff88"


def take_lock() -> bool:
    """Take the lock. False when this session already holds it (a build of ours is running): the caller then leaves
    it in place."""
    try:
        LOCK.mkdir(parents=True)
    except FileExistsError:
        owner = (LOCK / "owner.txt").read_text(encoding="utf-8", errors="replace") if (LOCK / "owner.txt").exists() else "?"
        if SESSION in owner:
            return False
        sys.exit(f"client-build lock is held:\n{owner}\nwait for it, or ask its owner")
    (LOCK / "owner.txt").write_text(
        f"session: UI modernization ({SESSION})\ntask: publish_ui.py deploy of ui/ + theme textures\n"
        f"taken: {datetime.datetime.now().isoformat(timespec='seconds')}\n", encoding="utf-8")
    return True


def release_lock():
    shutil.rmtree(LOCK, ignore_errors=True)


def publish(dry: bool):
    copied = same = 0
    for src_root, dst_root, pattern in PAIRS:
        if not src_root.is_dir():
            continue
        bak_root = backup_dir(dst_root)
        for src in sorted(src_root.glob(pattern)):
            if not src.is_file() or src.name in SKIP:
                continue
            rel = src.relative_to(src_root)
            dst = dst_root / rel
            if dst.exists() and filecmp.cmp(src, dst, shallow=False):
                same += 1
                continue
            print(("would copy " if dry else "copy ") + str(dst.relative_to(EXE)))
            if dry:
                continue
            bak = bak_root / rel
            absent = bak.with_name(bak.name + ".absent")
            if not bak.exists() and not absent.exists():
                bak.parent.mkdir(parents=True, exist_ok=True)
                if dst.exists():
                    shutil.copy2(dst, bak)
                else:
                    absent.write_text("", encoding="utf-8")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            copied += 1
    print(f"{copied} copied, {same} unchanged")


def revert(dry: bool):
    restored = 0
    for _, dst_root, _ in PAIRS:
        bak_root = backup_dir(dst_root)
        if not bak_root.is_dir():
            continue
        for bak in sorted(bak_root.rglob("*")):
            if not bak.is_file():
                continue
            rel = bak.relative_to(bak_root)
            if bak.name.endswith(".absent"):
                dst = dst_root / rel.with_name(bak.name[:-len(".absent")])
                print(("would remove " if dry else "remove ") + str(dst.relative_to(EXE)))
                if not dry and dst.exists():
                    dst.unlink()
            else:
                dst = dst_root / rel
                print(("would restore " if dry else "restore ") + str(dst.relative_to(EXE)))
                if not dry:
                    shutil.copy2(bak, dst)
            restored += 1
        if not dry:
            shutil.rmtree(bak_root)
    print(f"{restored} files reverted")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--revert", action="store_true", help="restore the files this script replaced")
    args = ap.parse_args()
    took = take_lock() if not args.dry_run else False
    try:
        (revert if args.revert else publish)(args.dry_run)
    finally:
        if took:
            release_lock()
    return 0


if __name__ == "__main__":
    sys.exit(main())
