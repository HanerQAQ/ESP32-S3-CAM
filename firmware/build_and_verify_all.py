#!/usr/bin/env python3
"""
Batch-build and merge-bin all ESP32-S3 S3CAM projects using ESP-IDF 6.1.

Usage:
    python build_and_verify_all.py              # incremental build if sdkconfig exists
    python build_and_verify_all.py --clean      # clean build & set-target esp32s3
    python build_and_verify_all.py cat_detect   # build specific project(s)
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FIRMWARE_DIR = ROOT.parent / "prebuilt_firmware"

PROJECTS = [
    "cat_detect",
    "color_detect",
    "human_face_detect",
    "human_face_recognition",
    "Image_transmission_function_AP",
    "Image_transmission_function_STA",
    "qrcode_recognition",
]


@dataclass
class BuildResult:
    name: str
    ok: bool
    elapsed: float
    bin_size: int = 0
    error: str = ""


def _resolve_idf_python(env: dict[str, str]) -> str:
    idf_py_env = env.get("IDF_PYTHON_ENV_PATH")
    if idf_py_env:
        cand = Path(idf_py_env) / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        if cand.is_file():
            return str(cand)
    return sys.executable


def build_idf_env() -> dict[str, str]:
    env = os.environ.copy()
    if env.get("IDF_PATH") and (Path(env["IDF_PATH"]) / "tools" / "idf.py").is_file():
        return env

    # Auto-detect common ESP-IDF v6.1 PowerShell profile locations on Windows when launched from a plain shell
    profile_candidates = [
        Path(r"C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1"),
        Path(os.environ.get("IDF_TOOLS_PATH", r"C:\Espressif")) / "Microsoft.v6.1.PowerShell_profile.ps1",
        Path.home() / ".espressif" / "Microsoft.v6.1.PowerShell_profile.ps1",
    ]
    for profile in profile_candidates:
        if profile.is_file():
            ps_cmd = (
                f". '{profile}' > $null; "
                "Get-ChildItem Env: | ForEach-Object { \"$($_.Name)=$($_.Value)\" }"
            )
            proc = subprocess.run(
                ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps_cmd],
                capture_output=True,
                text=True,
                check=True,
            )
            for line in proc.stdout.splitlines():
                if "=" in line:
                    k, v = line.split("=", 1)
                    env[k] = v
            if env.get("IDF_PATH") and (Path(env["IDF_PATH"]) / "tools" / "idf.py").is_file():
                return env

    raise RuntimeError(
        "ESP-IDF environment is not active (IDF_PATH not set). "
        "Please run this script from an ESP-IDF v6.1 PowerShell/Command Prompt."
    )


def run_idf(project_dir: Path, args: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    idf_py = Path(env["IDF_PATH"]) / "tools" / "idf.py"
    py_exe = _resolve_idf_python(env)
    cmd = [py_exe, str(idf_py), *args]
    return subprocess.run(
        cmd,
        cwd=str(project_dir),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def build_one(name: str, clean: bool, env: dict[str, str]) -> BuildResult:
    proj_dir = ROOT / name
    start = time.monotonic()

    if not proj_dir.is_dir():
        return BuildResult(name=name, ok=False, elapsed=0.0, error=f"Missing dir: {proj_dir}")

    if clean:
        for stale in ("build", "sdkconfig", "sdkconfig.old"):
            target = proj_dir / stale
            if target.is_dir():
                shutil.rmtree(target)
            elif target.exists():
                target.unlink()

    if not (proj_dir / "sdkconfig").exists():
        res = run_idf(proj_dir, ["set-target", "esp32s3"], env)
        if res.returncode != 0:
            tail = "\n".join((res.stdout + "\n" + res.stderr).splitlines()[-30:])
            return BuildResult(name=name, ok=False, elapsed=time.monotonic() - start, error=f"set-target failed:\n{tail}")

    res = run_idf(proj_dir, ["build"], env)
    if res.returncode != 0:
        combined = (res.stdout + "\n" + res.stderr).splitlines()
        err_lines = [ln for ln in combined if "FAILED:" in ln or "error:" in ln or "undefined reference" in ln]
        snippet = err_lines[-20:] if err_lines else combined[-40:]
        return BuildResult(name=name, ok=False, elapsed=time.monotonic() - start, error="build failed:\n" + "\n".join(snippet))

    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)
    out_bin = FIRMWARE_DIR / f"{name}.bin"
    res = run_idf(proj_dir, ["merge-bin", "-o", str(out_bin)], env)
    if res.returncode != 0 or not out_bin.exists():
        tail = "\n".join((res.stdout + "\n" + res.stderr).splitlines()[-30:])
        return BuildResult(name=name, ok=False, elapsed=time.monotonic() - start, error=f"merge-bin failed:\n{tail}")

    return BuildResult(
        name=name,
        ok=True,
        elapsed=time.monotonic() - start,
        bin_size=out_bin.stat().st_size,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build & merge-bin all S3CAM ESP-IDF 6.1 projects")
    parser.add_argument("projects", nargs="*", help="Subset of project folder names to build")
    parser.add_argument("--clean", action="store_true", help="Remove build/ & sdkconfig before building")
    args = parser.parse_args()

    targets = args.projects if args.projects else PROJECTS
    env = build_idf_env()

    results: list[BuildResult] = []
    for name in targets:
        print(f"\n=== [{len(results) + 1}/{len(targets)}] Building {name} ===", flush=True)
        r = build_one(name, clean=args.clean, env=env)
        results.append(r)
        if r.ok:
            print(f"[PASS] {r.name} ({r.elapsed:.1f}s) -> prebuilt_firmware/{r.name}.bin ({r.bin_size:,} bytes)", flush=True)
        else:
            print(f"[FAIL] {r.name} ({r.elapsed:.1f}s)\n{r.error}", flush=True)

    passed = sum(1 for r in results if r.ok)
    print(f"\nSummary: {passed}/{len(results)} succeeded.")
    if passed == len(results):
        import hashlib
        lines = []
        for bin_file in sorted(FIRMWARE_DIR.glob("*.bin")):
            h = hashlib.sha256(bin_file.read_bytes()).hexdigest()
            lines.append(f"{h}  {bin_file.name}\n")
        if lines:
            (FIRMWARE_DIR / "SHA256SUMS.txt").write_text("".join(lines), encoding="utf-8")
            print(f"[INFO] Updated {FIRMWARE_DIR / 'SHA256SUMS.txt'}", flush=True)
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
