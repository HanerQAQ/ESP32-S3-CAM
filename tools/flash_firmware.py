#!/usr/bin/env python3
"""
Interactive ESP32-S3 CAM Firmware Flashing Tool.

Automatically scans available COM ports, lets the user pick from the 7
prebuilt merged binaries, and invokes esptool to write the image at 0x0.

Usage:
    python tools/flash_firmware.py
    python tools/flash_firmware.py --port COMx --bin cat_detect.bin --erase
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREBUILT_DIR = ROOT / "prebuilt_firmware"

FIRMWARE_OPTIONS = [
    ("cat_detect.bin", "Cat & Dog Detection / 猫狗识别检测"),
    ("color_detect.bin", "Color Tracking & Line Following / 颜色识别与巡线 (UART 921600)"),
    ("human_face_detect.bin", "Human Face Detection / 人脸检测"),
    ("human_face_recognition.bin", "Human Face Recognition / 人脸识别 (Flash fr 分区掉电保存)"),
    ("Image_transmission_function_AP.bin", "AP Video Streaming / AP 模式热点图传"),
    ("Image_transmission_function_STA.bin", "STA Provisioning Video Streaming / STA 网页配网图传"),
    ("qrcode_recognition.bin", "QR Code Decoding / 二维码扫描识别"),
]


def ensure_esptool() -> bool:
    """Check if esptool is available; if not, prompt user and offer auto-install via pip."""
    try:
        res = subprocess.run(
            [sys.executable, "-m", "esptool", "version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if res.returncode == 0:
            return True
    except Exception:
        pass

    print("\n[!] 'esptool' is not installed in the current Python environment.")
    print("    esptool is required to flash firmware to ESP32-S3.")
    try:
        ans = input("    Would you like to install esptool automatically now? [Y/n]: ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        return False

    if ans in ("", "y", "yes"):
        print(f"\n[*] Running: {sys.executable} -m pip install esptool pyserial...")
        ret = subprocess.run([sys.executable, "-m", "pip", "install", "esptool", "pyserial"]).returncode
        if ret == 0:
            print("[OK] esptool successfully installed!\n")
            return True
        else:
            print("[ERROR] Failed to install esptool. Please run: pip install esptool")
            return False
    else:
        print("[!] Cannot proceed without esptool. Please install it manually: pip install esptool")
        return False


def list_serial_ports() -> list[str]:
    """Scan and return available serial port identifiers."""
    ports = []
    try:
        from serial.tools import list_ports
        for p in list_ports.comports():
            desc = f"{p.device} ({p.description})" if p.description else p.device
            ports.append((p.device, desc))
    except ImportError:
        # Fallback if pyserial is not installed
        if sys.platform.startswith("win"):
            for i in range(1, 30):
                ports.append((f"COM{i}", f"COM{i}"))
        else:
            for cand in ["/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0", "/dev/ttyACM1"]:
                if os.path.exists(cand):
                    ports.append((cand, cand))
    return ports


def run_esptool(port: str, bin_path: Path, erase: bool = True, baud: int = 921600, dry_run: bool = False) -> int:
    """Invoke esptool to erase flash (optional) and write merged binary at 0x0."""
    if not dry_run and not ensure_esptool():
        return 1
    py_exe = sys.executable

    if erase:
        cmd_erase = [
            py_exe, "-m", "esptool",
            "--chip", "esp32s3",
            "-p", port,
            "-b", "460800",
            "erase_flash",
        ]
        print(f"\n[1/2] Erasing flash on {port}:")
        print("      " + " ".join(cmd_erase))
        if not dry_run:
            ret = subprocess.run(cmd_erase).returncode
            if ret != 0:
                print("[ERROR] Flash erase failed. Please check connection and BOOT button.")
                return ret

    cmd_write = [
        py_exe, "-m", "esptool",
        "--chip", "esp32s3",
        "-p", port,
        "-b", str(baud),
        "write_flash",
        "0x0", str(bin_path),
    ]
    print(f"\n[{'2/2' if erase else '1/1'}] Writing {bin_path.name} to {port} at 0x0 (baud: {baud}):")
    print("      " + " ".join(cmd_write))

    if dry_run:
        print("\n[DRY-RUN] Command previewed successfully (no flash action taken).")
        return 0

    ret = subprocess.run(cmd_write).returncode
    if ret == 0:
        print(f"\n[SUCCESS] Successfully flashed {bin_path.name} to ESP32-S3 CAM!")
        print("Tip: Reset or power-cycle the module to start running.")
    else:
        print(f"\n[ERROR] Flashing failed with exit code {ret}.")
    return ret


def interactive_menu(dry_run: bool = False) -> None:
    print("=" * 64)
    print("  ESP32-S3 CAM Interactive Firmware Flashing Tool")
    print("=" * 64)

    # 1. Scan Ports
    ports = list_serial_ports()
    if not ports:
        print("\n[!] No serial ports detected! Please connect your ESP32-S3 CAM via USB.")
        sys.exit(1)

    print("\nAvailable Serial Ports:")
    for idx, (dev, desc) in enumerate(ports, 1):
        hint = "  <-- (ESP32-S3 / CH340 Bridge)" if any(k in desc.upper() for k in ["CH340", "CP210", "USB SERIAL", "JTAG", "ESP32"]) else ""
        print(f"  [{idx}] {desc}{hint}")

    try:
        choice_port = input(f"\nSelect port [1-{len(ports)}] (default: 1): ").strip()
    except (EOFError, KeyboardInterrupt):
        print("\n[Exit] User cancelled.")
        return
    port_idx = int(choice_port) - 1 if choice_port.isdigit() and 1 <= int(choice_port) <= len(ports) else 0
    selected_port = ports[port_idx][0]
    print(f"Selected port: {selected_port}")

    # 2. Select Firmware
    print("\nAvailable Prebuilt Firmware (Merged 0x0 Binaries):")
    for idx, (fname, desc) in enumerate(FIRMWARE_OPTIONS, 1):
        bin_file = PREBUILT_DIR / fname
        size_str = f"({bin_file.stat().st_size / 1024 / 1024:.2f} MB)" if bin_file.is_file() else "[MISSING]"
        print(f"  [{idx}] {fname:<36} {size_str:<10} - {desc}")

    try:
        choice_fw = input(f"\nSelect firmware to flash [1-{len(FIRMWARE_OPTIONS)}] (default: 1): ").strip()
    except (EOFError, KeyboardInterrupt):
        print("\n[Exit] User cancelled.")
        return
    fw_idx = int(choice_fw) - 1 if choice_fw.isdigit() and 1 <= int(choice_fw) <= len(FIRMWARE_OPTIONS) else 0
    selected_bin = PREBUILT_DIR / FIRMWARE_OPTIONS[fw_idx][0]

    if not selected_bin.is_file():
        print(f"[!] Binary file not found: {selected_bin}")
        sys.exit(1)

    # 3. Erase confirmation
    is_sta = "STA" in selected_bin.name
    if is_sta:
        print("\n[!] IMPORTANT: STA firmware uses a different NVS partition offset (0x9000 vs 0x3C6000).")
        print("    Full flash erase is STRONGLY RECOMMENDED to avoid NVS partition table conflicts.")

    prompt_erase = "\nErase full flash before writing? [Y/n] (default: Y): "
    try:
        choice_erase = input(prompt_erase).strip().lower()
    except (EOFError, KeyboardInterrupt):
        print("\n[Exit] User cancelled.")
        return
    erase = choice_erase not in ("n", "no")

    # 4. Flash
    run_esptool(selected_port, selected_bin, erase=erase, dry_run=dry_run)


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash prebuilt ESP32-S3 CAM firmware")
    parser.add_argument("-p", "--port", help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--bin", help="Binary filename inside prebuilt_firmware/")
    parser.add_argument("--erase", action="store_true", default=False, help="Erase entire flash before writing")
    parser.add_argument("--baud", type=int, default=921600, help="Flashing baud rate (default: 921600)")
    parser.add_argument("--dry-run", action="store_true", default=False, help="Preview esptool flashing command without writing to hardware")
    args = parser.parse_args()

    if args.port and args.bin:
        bin_path = PREBUILT_DIR / args.bin if not os.path.isabs(args.bin) else Path(args.bin)
        if not bin_path.is_file() and not args.dry_run:
            print(f"[!] Binary file not found: {bin_path}")
            return 1
        if "STA" in str(bin_path) and not args.erase:
            print("[TIP] Flashing STA firmware: Pass '--erase' if switching from another firmware to reset NVS (0x9000).")
        return run_esptool(args.port, bin_path, erase=args.erase, baud=args.baud, dry_run=args.dry_run)

    if args.dry_run:
        ports = list_serial_ports()
        port = args.port or (ports[0][0] if ports else "COM1")
        bin_path = (PREBUILT_DIR / args.bin) if args.bin else (PREBUILT_DIR / FIRMWARE_OPTIONS[0][0])
        return run_esptool(port, bin_path, erase=args.erase, baud=args.baud, dry_run=True)

    interactive_menu(dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
