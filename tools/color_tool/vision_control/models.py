from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class SerialSettings:
    port: str = ""
    baudrate: str = ""
    data_bits: str = "8"
    parity: str = "None"
    stop_bits: str = "1"
    flow_control: str = "None"


@dataclass(frozen=True)
class HsvThreshold:
    h_min: int
    h_max: int
    s_min: int
    s_max: int
    v_min: int
    v_max: int


@dataclass(frozen=True)
class ColorConfig:
    name: str
    enabled: bool
    swatch: str
    threshold: HsvThreshold
    area_min: int = 120


@dataclass(frozen=True)
class UiState:
    serial: SerialSettings = field(default_factory=SerialSettings)
    connected: bool = False
    active_color: str = "color1"
    colors: tuple[ColorConfig, ...] = (
        ColorConfig("color1", True, "#ef233c", HsvThreshold(170, 10, 80, 255, 80, 255)),
        ColorConfig("color2", True, "#2f80ed", HsvThreshold(95, 125, 70, 255, 70, 255)),
        ColorConfig("color3", True, "#12b76a", HsvThreshold(45, 85, 60, 255, 60, 255)),
        ColorConfig("color4", True, "#f59e0b", HsvThreshold(15, 35, 70, 255, 80, 255)),
        ColorConfig("color5", True, "#8b5cf6", HsvThreshold(125, 155, 60, 255, 60, 255)),
    )
