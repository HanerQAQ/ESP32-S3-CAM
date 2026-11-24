"""Repository brand hygiene verification tests.

Ensures no vendor-specific identifiers, filenames, compressed web assets,
or schematic PDF metadata remain anywhere in the repository.
"""

from __future__ import annotations

import gzip
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SELF_PATH = Path(__file__).resolve()

SKIP_DIRS = {".git", "__pycache__", ".pytest_cache"}

FORBIDDEN_TERMS = (
    "hiwonder",
    "幻尔",
    "wonderllm",
    "hw_esp32",
    "hiwonder_esp32",
)

TEXT_EXTS = {
    ".c",
    ".cc",
    ".cfg",
    ".cmake",
    ".cpp",
    ".css",
    ".csv",
    ".h",
    ".hpp",
    ".htm",
    ".html",
    ".ini",
    ".ino",
    ".js",
    ".json",
    ".lds",
    ".md",
    ".properties",
    ".py",
    ".sh",
    ".bat",
    ".spec",
    ".toml",
    ".txt",
    ".xml",
    ".yaml",
    ".yml",
}

SPECIAL_FILENAMES = {
    "CMakeLists.txt",
    "Kconfig",
    "Kconfig.projbuild",
    "LICENSE",
    "Makefile",
    "NOTICE",
    "pre-commit",
    "sdkconfig",
    "sdkconfig.defaults",
}


def _iter_repo_files():
    for path in sorted(REPO_ROOT.rglob("*")):
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        yield path


def _is_text_candidate(path: Path) -> bool:
    if path.suffix.lower() in TEXT_EXTS:
        return True
    if path.name in SPECIAL_FILENAMES:
        return True
    if path.name.startswith(("sdkconfig.defaults", "partitions")):
        return True
    return False


def _find_hits(text: str) -> list[str]:
    low = text.lower()
    return [term for term in FORBIDDEN_TERMS if term in low]


def test_no_branded_filenames():
    """Verify no directory or file names contain deprecated brand identifiers."""
    hits = []
    for path in _iter_repo_files():
        rel = path.relative_to(REPO_ROOT).as_posix()
        found = _find_hits(rel)
        if found:
            hits.append(f"{rel}: {found}")
    assert not hits, f"Branded paths found: {hits}"


def test_no_branded_text_content():
    """Verify no source, documentation, or config files contain deprecated brand terms."""
    hits = []
    for path in _iter_repo_files():
        if not path.is_file() or path.resolve() == SELF_PATH:
            continue
        if not _is_text_candidate(path):
            continue
        content = path.read_text(encoding="utf-8", errors="ignore")
        found = _find_hits(content)
        if found:
            hits.append(f"{path.relative_to(REPO_ROOT).as_posix()}: {found}")
    assert not hits, f"Branded text content found: {hits}"


def test_no_branded_gzip_assets():
    """Verify compressed .gz web frontend archives contain zero deprecated brand terms."""
    hits = []
    for path in _iter_repo_files():
        if path.is_file() and path.suffix.lower() == ".gz":
            raw = gzip.decompress(path.read_bytes()).decode("utf-8", errors="ignore")
            found = _find_hits(raw)
            if found:
                hits.append(f"{path.relative_to(REPO_ROOT).as_posix()}: {found}")
    assert not hits, f"Branded content in .gz archives: {hits}"


def test_no_branded_pdf_schematics():
    """Verify schematic PDFs contain zero deprecated brand text or metadata when PyMuPDF is available."""
    try:
        import fitz  # PyMuPDF
    except ImportError:
        return

    hits = []
    hardware_dir = REPO_ROOT / "hardware"
    if not hardware_dir.is_dir():
        return

    for pdf_path in sorted(hardware_dir.glob("*.pdf")):
        doc = fitz.open(pdf_path)
        try:
            meta_text = " ".join(str(v) for v in (doc.metadata or {}).values() if v)
            page_text = "\n".join(page.get_text("text") for page in doc)
        finally:
            doc.close()
        found = _find_hits(meta_text + "\n" + page_text)
        if found:
            hits.append(f"{pdf_path.relative_to(REPO_ROOT).as_posix()}: {found}")
    assert not hits, f"Branded text/metadata found in PDFs: {hits}"
