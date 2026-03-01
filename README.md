# usbOTGStream

Streams live JPEG frames from an **ESP32-S3 Dev Module + OV2640** camera to a
PC Python script over **USB OTG** (USB CDC).
Resolution: **VGA (640 × 480)** · Target frame rate: **~30 FPS**

---

## Repository layout

```
esp32s3_camera_usb_stream/
  esp32s3_camera_usb_stream.ino   ← Arduino sketch (upload to ESP32-S3)
receiver.py                       ← PC Python receiver / viewer
requirements.txt                  ← Python dependencies
```

---

## Hardware required

| Component | Notes |
|---|---|
| ESP32-S3 Dev Module | Board with native USB OTG port and PSRAM |
| OV2640 camera module | Connected via FPC (flat flexible cable) |
| USB cable | Connects ESP32-S3 **USB OTG** port to PC |

---

## FPC connector pinout

The sketch uses the standard 24-pin FPC pinout found on most ESP32-S3 camera
boards. GPIO assignments:

| Signal | GPIO |
|---|---|
| XCLK | 10 |
| SDA | 40 |
| SCL | 39 |
| D7 | 48 |
| D6 | 11 |
| D5 | 12 |
| D4 | 14 |
| D3 | 16 |
| D2 | 18 |
| D1 | 17 |
| D0 | 15 |
| VSYNC | 38 |
| HREF | 47 |
| PCLK | 13 |
| PWDN | — (not connected) |
| RESET | — (not connected) |

---

## Arduino IDE setup

1. Install **Arduino IDE 2.x** and add the ESP32 board package
   (`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`)
2. Install the **esp32-camera** library via Library Manager.
3. Open `esp32s3_camera_usb_stream/esp32s3_camera_usb_stream.ino`.
4. Set the following in **Tools**:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | **Enabled** |
| Flash Size | 4 MB (or 8 MB) |
| PSRAM | OPI PSRAM (or Quad if available) |
| Partition Scheme | Huge APP (3 MB No OTA) |

5. Select the UART COM port and click **Upload**.
6. After upload, reconnect the board via the **USB OTG** port.

---

## Python receiver setup

```bash
pip install -r requirements.txt
```

### Run

```bash
python receiver.py
python receiver.py --port COM3
python receiver.py --port /dev/ttyACM0
python receiver.py --no-display
python receiver.py --save output.avi
```

Press **Q** in the OpenCV window to quit.

---

## Frame protocol

```
[0xFF 0xAA]  2-byte start marker
[uint32 LE]  4-byte JPEG payload length
[... JPEG …] JPEG image bytes
[0xFF 0xBB]  2-byte end marker
```

