#!/usr/bin/env python3

import argparse
import struct
import sys
import time

import cv2
import numpy as np
import serial
import serial.tools.list_ports

FRAME_START    = bytes([0xFF, 0xAA])
FRAME_END      = bytes([0xFF, 0xBB])
MAX_FRAME_SIZE = 250_000
READ_TIMEOUT   = 5.0

WINDOW_TITLE = "ESP32-S3 OV2640 VGA 30 FPS  (Q - quit)"


def auto_detect_port() -> str | None:
    candidates = []
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if any(k in desc or k in hwid for k in ("esp32", "cdc", "acm", "usbserial")):
            candidates.append(p.device)
    if candidates:
        return candidates[0]
    ports = serial.tools.list_ports.comports()
    return ports[0].device if ports else None


def read_exact(ser: serial.Serial, n: int) -> bytes:
    buf = bytearray()
    deadline = time.monotonic() + READ_TIMEOUT
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf.extend(chunk)
        elif time.monotonic() > deadline:
            raise IOError(f"Timeout reading {n} bytes (got {len(buf)})")
    return bytes(buf)


def receive_frame(ser: serial.Serial) -> bytes:
    window = bytearray(2)
    while True:
        b = ser.read(1)
        if not b:
            continue
        window[0] = window[1]
        window[1] = b[0]
        if bytes(window) == FRAME_START:
            break

    length_bytes = read_exact(ser, 4)
    length = struct.unpack("<I", length_bytes)[0]

    if length == 0 or length > MAX_FRAME_SIZE:
        raise ValueError(f"Unexpected frame length: {length} bytes")

    jpeg = read_exact(ser, length)

    end = read_exact(ser, 2)
    if end != FRAME_END:
        raise ValueError(f"Bad end marker: {end.hex()}")

    return jpeg


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Receive and display camera stream from ESP32-S3 via USB OTG"
    )
    parser.add_argument("--port", type=str, default=None)
    parser.add_argument("--no-display", action="store_true")
    parser.add_argument("--save", type=str, default=None, metavar="FILE")
    args = parser.parse_args()

    port = args.port or auto_detect_port()
    if not port:
        print("ERROR: No serial port found. Use --port to specify one.")
        sys.exit(1)

    print(f"Connecting to {port} ...")
    try:
        ser = serial.Serial(port, baudrate=115200, timeout=2)
    except serial.SerialException as exc:
        print(f"ERROR: Could not open {port}: {exc}")
        sys.exit(1)
    print("Connected. Waiting for frames ...")

    writer = None
    if args.save:
        fourcc = cv2.VideoWriter_fourcc(*"MJPG")
        writer = cv2.VideoWriter(args.save, fourcc, 30.0, (640, 480))
        print(f"Saving stream to {args.save}")

    if not args.no_display:
        cv2.namedWindow(WINDOW_TITLE, cv2.WINDOW_AUTOSIZE)

    frame_count = 0
    fps_count   = 0
    fps         = 0.0
    fps_ts      = time.monotonic()

    try:
        while True:
            try:
                jpeg = receive_frame(ser)
            except ValueError as exc:
                print(f"[WARN] Frame error: {exc} - skipping")
                continue
            except IOError as exc:
                print(f"[ERROR] {exc}")
                break

            frame_count += 1
            fps_count   += 1

            now = time.monotonic()
            if now - fps_ts >= 1.0:
                fps       = fps_count / (now - fps_ts)
                fps_count = 0
                fps_ts    = now
                print(f"FPS: {fps:.1f}  |  frames received: {frame_count}")

            img_array = np.frombuffer(jpeg, dtype=np.uint8)
            frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
            if frame is None:
                print("[WARN] Failed to decode JPEG frame - skipping")
                continue

            cv2.putText(
                frame, f"FPS: {fps:.1f}", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2, cv2.LINE_AA
            )

            if writer:
                writer.write(frame)

            if not args.no_display:
                cv2.imshow(WINDOW_TITLE, frame)
                if cv2.waitKey(1) & 0xFF in (ord("q"), ord("Q")):
                    break

    except KeyboardInterrupt:
        print("\nInterrupted by user.")

    finally:
        ser.close()
        if writer:
            writer.release()
        if not args.no_display:
            cv2.destroyAllWindows()
        print("Stream closed.")


if __name__ == "__main__":
    main()
