#!/usr/bin/env python3
"""
clean_repo.py - Full repository hygiene and build artifact cleanup tool.

Scans the repository and safely removes:
  - Python byte code & cache: __pycache__/, *.py[cod], .pytest_cache/
  - Packaging artifacts: *.egg-info/, build/, dist/
  - Embedded temporary build dirs: .pio/
  - OS metadata: .DS_Store, Thumbs.db

Usage:
  python tools/clean_repo.py           # Clean temporary and test caches
  python tools/clean_repo.py --dry-run # Preview files/directories to be deleted
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

DISALLOWED_DIRS = {
    "__pycache__",
    ".pytest_cache",
    ".pio",
    "dist",
}

DISALLOWED_PATTERNS = {
    ".pyc",
    ".pyo",
    ".pyd",
    ".DS_Store",
    "Thumbs.db",
}


def clean(dry_run: bool = False) -> int:
    removed_files = 0
    removed_dirs = 0
    total_bytes = 0

    print(f"[*] Scanning repository: {ROOT}")
    if dry_run:
        print("[*] Running in DRY-RUN mode (no files will be deleted).")

    # Pass 1: Walk files
    for root, dirs, files in os.walk(ROOT, topdown=False):
        if ".git" in root.split(os.sep):
            continue

        rel_root = Path(root).relative_to(ROOT)

        for f in files:
            p = Path(root) / f
            suffix = p.suffix.lower()
            name = p.name

            # Skip prebuilt firmware binaries and models
            if "prebuilt_firmware" in p.parts:
                continue

            should_remove = False
            if suffix in DISALLOWED_PATTERNS or name in DISALLOWED_PATTERNS:
                should_remove = True
            elif name.endswith(".egg-info"):
                should_remove = True

            if should_remove:
                try:
                    sz = p.stat().st_size
                except OSError:
                    sz = 0
                if not dry_run:
                    p.unlink(missing_ok=True)
                removed_files += 1
                total_bytes += sz
                action = "[DRY-RUN] Would remove" if dry_run else "[REMOVED]"
                print(f"  {action} file: {rel_root / f} ({sz / 1024:.1f} KB)")

        for d in list(dirs):
            dp = Path(root) / d
            if d in DISALLOWED_DIRS or d.endswith(".egg-info"):
                # Safeguard: do not delete firmware source components
                if "firmware" in dp.parts and "components" in dp.parts and d != "__pycache__":
                    continue

                sz = sum(f.stat().st_size for f in dp.rglob("*") if f.is_file())
                if not dry_run:
                    shutil.rmtree(dp, ignore_errors=True)
                removed_dirs += 1
                total_bytes += sz
                action = "[DRY-RUN] Would remove" if dry_run else "[REMOVED]"
                print(f"  {action} dir : {rel_root / d} ({sz / 1024:.1f} KB)")

    # Clean root .pytest_cache if it exists
    root_pytest = ROOT / ".pytest_cache"
    if root_pytest.is_dir():
        sz = sum(f.stat().st_size for f in root_pytest.rglob("*") if f.is_file())
        if not dry_run:
            shutil.rmtree(root_pytest, ignore_errors=True)
        removed_dirs += 1
        total_bytes += sz
        action = "[DRY-RUN] Would remove" if dry_run else "[REMOVED]"
        print(f"  {action} dir : .pytest_cache ({sz / 1024:.1f} KB)")

    print("=" * 60)
    print(f"Cleanup summary: {removed_files} files, {removed_dirs} directories ({total_bytes / 1024:.1f} KB freed).")
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Clean repository caches and temporary build artifacts.")
    parser.add_argument("--dry-run", action="store_true", help="Preview deletions without modifying disk.")
    args = parser.parse_args()
    sys.exit(clean(dry_run=args.dry_run))
