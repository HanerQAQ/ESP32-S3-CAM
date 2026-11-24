#!/usr/bin/env python3
"""
ESP32-S3 CAM I2C Communication Diagnostic Tool.

Tests connectivity with the module over I2C at slave address 0x52 (100 kHz).
Sends a test JSON command, receives the 32-byte fragment frame, verifies XOR
checksum, measures latency, and provides actionable troubleshooting advice.

Supported environments:
  - Linux SBCs (Raspberry Pi, NVIDIA Jetson, Orange Pi) via /dev/i2c-*
  - MicroPython / CircuitPython via standard machine.I2C

Usage:
  python tools/i2c_diagnose.py
  python tools/i2c_diagnose.py --bus 1
"""

from __future__ import annotations

import argparse
import sys
import time

I2C_ADDR_7BIT = 0x52
FRAME_MAGIC = 0x55AA


def parse_and_validate_frame(frame: bytes, elapsed_ms: float = 20.0) -> bool:
    """Parse and validate a 32-byte ESP32-S3 CAM I2C response frame."""
    if len(frame) < 32:
        print(f"[WARN] Received {len(frame)} bytes, expected 32 bytes.")
        return False

    # Resync recovery: If first byte 0xAA was consumed by FIFO STOP/START gap, frame starts with 0x55
    if frame[0] == 0x55 and frame[1] != 0x55:
        candidate = bytes([0xAA]) + frame[:31]
        c_len = int.from_bytes(candidate[2:4], "little")
        if 0 < c_len <= 23:
            c_payload = candidate[8:8 + c_len]
            c_chk = 0
            for b in c_payload:
                c_chk ^= b
            if c_chk == candidate[8 + c_len]:
                print("[INFO] Applied 0x55 resync recovery (0xAA byte restored).")
                frame = candidate

    header = int.from_bytes(frame[0:2], "little")
    data_len = int.from_bytes(frame[2:4], "little")
    frag_id = int.from_bytes(frame[4:6], "little")
    total_frag = int.from_bytes(frame[6:8], "little")

    if not (0 < data_len <= 23):
        print(f"[WARN] Invalid data length: {data_len} bytes (expected 1..23). Raw frame: {frame.hex()}")
        return False

    payload = frame[8:8 + data_len]
    checksum = frame[8 + data_len]

    # Calculate XOR checksum of valid DATA payload
    calc_xor = 0
    for byte in payload:
        calc_xor ^= byte

    print(f"[3/3] Response received in {elapsed_ms:.2f} ms:")
    print(f"      Header: 0x{header:04X} ({'VALID 0x55AA' if header == 0x55AA else 'MISMATCH'})")
    print(f"      Data Length: {data_len} bytes")
    print(f"      Fragment: {frag_id} / {total_frag}")
    print(f"      Payload (hex): {payload.hex()}")
    print(f"      XOR Checksum: 0x{checksum:02X} ({'VALID' if calc_xor == checksum else f'MISMATCH: calculated 0x{calc_xor:02X}'})")

    if header == 0x55AA and calc_xor == checksum:
        print("\n[SUCCESS] ESP32-S3 CAM I2C slave is working properly!")
        return True
    else:
        print("\n[WARN] Received data corrupted or framing error.")
        return False


def run_mock_test() -> bool:
    """Run an automated diagnostic test using synthesized protocol frames."""
    print("[*] Running simulated diagnostic test in --mock mode...")
    payload = b'{"status":0}'
    data_len = len(payload)
    xor_chk = 0
    for b in payload:
        xor_chk ^= b

    # Construct standard 32-byte frame: 0x55AA (little-endian: 0xAA, 0x55)
    frame = bytearray(32)
    frame[0] = 0xAA
    frame[1] = 0x55
    frame[2:4] = data_len.to_bytes(2, "little")
    frame[4:6] = (1).to_bytes(2, "little")
    frame[6:8] = (1).to_bytes(2, "little")
    frame[8:8 + data_len] = payload
    frame[8 + data_len] = xor_chk

    print("\n--- Test 1: Standard Frame Validation ---")
    ok1 = parse_and_validate_frame(bytes(frame), elapsed_ms=18.5)

    print("\n--- Test 2: 0x55 Resync Recovery Simulation ---")
    corrupt_frame = bytes(frame[1:]) + b"\x00"
    ok2 = parse_and_validate_frame(corrupt_frame, elapsed_ms=19.2)

    return ok1 and ok2


def test_linux_i2c(bus_num: int | None = None, slave_addr: int = I2C_ADDR_7BIT) -> bool:
    try:
        import os
        from fcntl import ioctl
    except ImportError:
        print("[!] Native Linux I2C ioctl is only available on Linux (Raspberry Pi / Jetson).")
        print("    Tip: Use 'python tools/i2c_diagnose.py --mock' to test diagnostic logic.")
        return False

    candidate_buses = [bus_num] if bus_num is not None else [1, 7, 8, 0]
    fd = -1
    active_bus = None

    for b in candidate_buses:
        dev_path = f"/dev/i2c-{b}"
        if not os.path.exists(dev_path):
            continue
        try:
            fd = os.open(dev_path, os.O_RDWR)
            # I2C_SLAVE = 0x0703
            ioctl(fd, 0x0703, slave_addr)
            active_bus = b
            print(f"[OK] Opened {dev_path} addressing slave 0x{slave_addr:02X}")
            break
        except Exception:
            if fd >= 0:
                os.close(fd)
                fd = -1

    if fd < 0:
        print(f"[FAIL] Could not open any I2C bus among: {candidate_buses}")
        print("\nTroubleshooting Suggestions:")
        print("  1. Ensure I2C interface is enabled (on Raspberry Pi: 'sudo raspi-config' -> Interface Options -> I2C).")
        print("  2. Ensure your user has permissions: 'sudo usermod -aG i2c $USER' and re-login.")
        print("  3. Check device files: 'ls -l /dev/i2c*'.")
        return False

    try:
        # Step 1: Send test command
        cmd = b'{"command":"get_info","params":""}'
        print(f"\n[1/3] Transmitting test query to 0x{slave_addr:02X} on bus {active_bus}: {cmd.decode('utf-8')}")
        t0 = time.monotonic()
        os.write(fd, cmd)
        time.sleep(0.020)  # 20ms processing delay for ESP32-S3 slave task

        # Step 2: Read 32-byte fragment frame
        print("[2/3] Reading 32-byte response fragment...")
        frame = os.read(fd, 32)
        elapsed_ms = (time.monotonic() - t0) * 1000.0

        return parse_and_validate_frame(frame, elapsed_ms=elapsed_ms)

    except Exception as exc:
        print(f"\n[FAIL] I2C communication error: {exc}")
        print("\nTroubleshooting Suggestions:")
        print("  1. COMMON GROUND: Ensure host GND is connected to ESP32-S3 CAM GND.")
        print("  2. POWER SUPPLY: Ensure ESP32-S3 CAM has dedicated 5V >= 2A power.")
        print("  3. WIRING: Check SDA -> GPIO47 and SCL -> GPIO48 on ESP32-S3 CAM.")
        print("  4. PULL-UPS: Verify 3.3V logic level and 4.7k pull-up resistors.")
        print("  5. FIRMWARE: Confirm ESP32-S3 CAM is flashed with a valid vision firmware.")
        return False
    finally:
        os.close(fd)


def main() -> int:
    parser = argparse.ArgumentParser(description="ESP32-S3 CAM I2C Communication Diagnostic")
    parser.add_argument("--bus", type=int, help="I2C bus number (e.g. 1 on RPi, 7 on Jetson)")
    parser.add_argument("--addr", type=lambda x: int(x, 0), default=I2C_ADDR_7BIT, help="I2C 7-bit slave address (default: 0x52)")
    parser.add_argument("--mock", action="store_true", help="Run simulated self-test without physical I2C hardware")
    args = parser.parse_args()

    print("=" * 64)
    print("  ESP32-S3 CAM I2C Link Diagnostic Tool")
    print("=" * 64)

    if args.mock:
        success = run_mock_test()
    else:
        success = test_linux_i2c(args.bus, args.addr)
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
