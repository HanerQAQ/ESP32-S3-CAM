"""Preset management for ColorTool HSV configurations.

Allows saving and loading tuned HSV color threshold profiles to/from JSON files
for different illumination environments (e.g. indoor, daylight, direct sunlight).
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from vision_control.models import ColorConfig, HsvThreshold


class PresetError(Exception):
    """Raised when a preset file cannot be parsed or contains invalid values."""
    pass


def export_preset_to_dict(configs: tuple[ColorConfig, ...], description: str = "") -> dict[str, Any]:
    """Serialize a collection of ColorConfig objects to a dictionary."""
    color_entries = []
    for cfg in configs:
        color_entries.append({
            "name": cfg.name,
            "enabled": cfg.enabled,
            "swatch": cfg.swatch,
            "area_min": cfg.area_min,
            "hsv_min": [cfg.threshold.h_min, cfg.threshold.s_min, cfg.threshold.v_min],
            "hsv_max": [cfg.threshold.h_max, cfg.threshold.s_max, cfg.threshold.v_max],
        })
    return {
        "version": "1.0",
        "format": "ColorTool_Preset",
        "description": description,
        "colors": color_entries,
    }


def import_preset_from_dict(data: dict[str, Any]) -> tuple[ColorConfig, ...]:
    """Parse a dictionary into a validated tuple of ColorConfig objects."""
    if not isinstance(data, dict):
        raise PresetError("Preset root must be a JSON object")

    raw_colors = data.get("colors")
    if not isinstance(raw_colors, list) or len(raw_colors) == 0:
        raise PresetError("Preset must contain a non-empty 'colors' list")

    configs = []
    for idx, item in enumerate(raw_colors[:5], start=1):
        if not isinstance(item, dict):
            raise PresetError(f"Color item #{idx} must be a JSON object")

        name = str(item.get("name", f"color{idx}"))
        enabled = bool(item.get("enabled", True))
        swatch = str(item.get("swatch", "#ffffff"))
        area_min = int(item.get("area_min", 120))

        hsv_min = item.get("hsv_min")
        hsv_max = item.get("hsv_max")
        if not (isinstance(hsv_min, list) and len(hsv_min) == 3):
            raise PresetError(f"Color {name} missing valid 3-element 'hsv_min'")
        if not (isinstance(hsv_max, list) and len(hsv_max) == 3):
            raise PresetError(f"Color {name} missing valid 3-element 'hsv_max'")

        threshold = HsvThreshold(
            h_min=max(0, min(180, int(hsv_min[0]))),
            h_max=max(0, min(180, int(hsv_max[0]))),
            s_min=max(0, min(255, int(hsv_min[1]))),
            s_max=max(0, min(255, int(hsv_max[1]))),
            v_min=max(0, min(255, int(hsv_min[2]))),
            v_max=max(0, min(255, int(hsv_max[2]))),
        )
        configs.append(ColorConfig(name, enabled, swatch, threshold, area_min))

    return tuple(configs)


def save_preset(file_path: str | Path, configs: tuple[ColorConfig, ...], description: str = "") -> None:
    """Save ColorConfig tuple as formatted JSON to disk."""
    data = export_preset_to_dict(configs, description)
    p = Path(file_path)
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def load_preset(file_path: str | Path) -> tuple[ColorConfig, ...]:
    """Load and validate ColorConfig tuple from a JSON file."""
    p = Path(file_path)
    if not p.is_file():
        raise PresetError(f"Preset file not found: {p}")
    try:
        with p.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as exc:
        raise PresetError(f"Invalid JSON format in preset: {exc}") from exc
    return import_preset_from_dict(data)
