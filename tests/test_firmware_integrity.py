"""Automated tests to verify firmware build configuration and optional release binary checksums."""

import hashlib
import os

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_PREBUILT_DIR = os.path.join(_REPO_ROOT, "prebuilt_firmware")
_CHECKSUMS_FILE = os.path.join(_PREBUILT_DIR, "SHA256SUMS.txt")
_FIRMWARE_DIR = os.path.join(_REPO_ROOT, "firmware")

EXPECTED_PROJECTS = [
    "cat_detect",
    "color_detect",
    "human_face_detect",
    "human_face_recognition",
    "Image_transmission_function_AP",
    "Image_transmission_function_STA",
    "qrcode_recognition",
]


def test_firmware_source_projects_present():
    """Verify all 7 ESP-IDF v6.1 firmware source projects and build script exist cleanly."""
    assert os.path.isfile(os.path.join(_PREBUILT_DIR, "README.md"))
    build_script = os.path.join(_FIRMWARE_DIR, "build_and_verify_all.py")
    assert os.path.isfile(build_script)

    for proj in EXPECTED_PROJECTS:
        proj_dir = os.path.join(_FIRMWARE_DIR, proj)
        assert os.path.isdir(proj_dir), f"Missing firmware project: {proj}"
        assert os.path.isfile(os.path.join(proj_dir, "CMakeLists.txt")), f"Missing CMakeLists.txt in {proj}"
        assert os.path.isfile(os.path.join(proj_dir, "sdkconfig.defaults")), f"Missing sdkconfig.defaults in {proj}"


def test_prebuilt_firmware_sha256_checksums_when_present():
    """Verify SHA256SUMS.txt matches all binaries when release binaries are staged locally."""
    if not os.path.isfile(_CHECKSUMS_FILE):
        # Source-only Git repository without staged release binaries; verify manifest README covers all projects.
        readme_text = open(os.path.join(_PREBUILT_DIR, "README.md"), encoding="utf-8").read()
        for proj in EXPECTED_PROJECTS:
            assert f"{proj}.bin" in readme_text, f"{proj}.bin missing from prebuilt_firmware/README.md"
        return

    with open(_CHECKSUMS_FILE, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip() and not line.startswith("#")]

    assert len(lines) == 7, f"Expected 7 firmware entries in SHA256SUMS.txt, found {len(lines)}"
    for line in lines:
        expected_hash, fname = line.split(None, 1)
        bin_path = os.path.join(_PREBUILT_DIR, fname)
        assert os.path.isfile(bin_path), f"Firmware binary {fname} missing in {_PREBUILT_DIR}"

        hasher = hashlib.sha256()
        with open(bin_path, "rb") as bf:
            while chunk := bf.read(65536):
                hasher.update(chunk)
        assert hasher.hexdigest() == expected_hash, f"SHA256 mismatch for {fname}"
