#!/usr/bin/env python3
"""
Clean ESP32-S3 firmware build artifacts and repository cache before sharing.

Usage:
  python firmware/clean_for_sharing.py
  python firmware/clean_for_sharing.py --dry-run
  python firmware/clean_for_sharing.py --with-managed
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

FIRMWARE_DIR = Path(__file__).resolve().parent
REPO_ROOT = FIRMWARE_DIR.parent

PROJECTS = [
    "cat_detect",
    "color_detect",
    "human_face_detect",
    "human_face_recognition",
    "Image_transmission_function_AP",
    "Image_transmission_function_STA",
    "qrcode_recognition",
]


def remove_path(p: Path, dry_run: bool = False) -> bool:
    if not p.exists():
        return False
    if dry_run:
        print(f"[DRY-RUN] Would remove: {p}")
        return True
    if p.is_dir():
        shutil.rmtree(p, ignore_errors=True)
        return True
    if p.is_file():
        p.unlink(missing_ok=True)
        return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Clean S3CAM build artifacts and caches for sharing")
    parser.add_argument("--with-managed", action="store_true", help="Also remove managed_components/ and sdkconfig")
    parser.add_argument("--dry-run", action="store_true", help="Preview targets without deleting")
    args = parser.parse_args()

    removed = 0
    print(f"[*] Cleaning firmware projects in {FIRMWARE_DIR}...")
    for name in PROJECTS:
        proj = FIRMWARE_DIR / name
        if not proj.is_dir():
            continue
        targets = ["build", "sdkconfig.old"]
        if args.with_managed:
            targets.extend(["managed_components", "sdkconfig", "dependencies.lock"])
        for rel in targets:
            target_path = proj / rel
            if remove_path(target_path, dry_run=args.dry_run):
                if not args.dry_run:
                    print(f"  Removed {name}/{rel}")
                removed += 1

    # Also invoke repo-wide clean_repo if available
    tools_cleaner = REPO_ROOT / "tools" / "clean_repo.py"
    if tools_cleaner.is_file():
        sys.path.insert(0, str(REPO_ROOT / "tools"))
        try:
            import clean_repo
            print("\n[*] Invoking tools/clean_repo.py for repository hygiene...")
            clean_repo.clean(dry_run=args.dry_run)
        except Exception as e:
            print(f"[WARN] Failed to invoke clean_repo: {e}")

    print(f"\n[DONE] Firmware cleaning complete ({removed} build target(s) processed).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
