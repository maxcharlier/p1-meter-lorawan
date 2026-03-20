# Board setup — Heltec CubeCell HTCC-AB02A

This guide covers driver installation, Arduino IDE board support, and how to flash a sketch.

Official product page: [heltec.org/project/htcc-ab02a](https://heltec.org/project/htcc-ab02a/)
Official documentation: [docs.heltec.org — CubeCell HTCC-AB02A](https://docs.heltec.org/en/node/asr650x/htcc-ab02a/index.html)

---

## 1 — Install the USB driver

The HTCC-AB02A uses a **CP2102** USB-to-UART bridge (Silicon Labs).

| OS | Action |
|----|--------|
| Windows 10/11 | Usually installed automatically via Windows Update. If the port does not appear in Device Manager, download the driver from [silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) and run the installer. |
| macOS | Download and install the CP210x driver from the same Silicon Labs page above. Reboot after installation. |
| Linux | The `cp210x` kernel module is included in most distributions. Plug in the board — a `/dev/ttyUSB0` (or similar) device should appear immediately. |

After installation, connect the board via USB. A serial port should appear:
- Windows: `COMx` in Device Manager → Ports (COM & LPT)
- macOS: `/dev/cu.SLAB_USBtoUART` or `/dev/cu.usbserial-*`
- Linux: `/dev/ttyUSB0`

---

## 2 — Install Arduino IDE

Download Arduino IDE 2.x from [arduino.cc/en/software](https://www.arduino.cc/en/software).

---

## 3 — Add Heltec CubeCell board support

1. Open **File → Preferences** (macOS: **Arduino IDE → Settings**).
2. In the **Additional boards manager URLs** field, add:
   ```
   https://resource.heltec.cn/download/package_CubeCell_index.json
   ```
3. Click **OK**.
4. Open **Tools → Board → Boards Manager**.
5. Search for `CubeCell` and install **Heltec CubeCell Series (ASR650x)**.

---

## 4 — Configure the board

Go to **Tools** and set:

| Setting | Value |
|---------|-------|
| Board | `CubeCell-Board (HTCC-AB02A)` |
| LORAWAN_REGION | `REGION_EU868` (Belgium) |
| LORAWAN_CLASS | `CLASS_A` |
| LORAWAN_NETMODE | `OTAA` |
| LORAWAN_ADR | `ON` |
| LORAWAN_UPLINKMODE | `UNCONFIRMED` |
| LORAWAN_Net_Reserve | `OFF` |
| LORAWAN_AT_SUPPORT | `OFF` |
| LORAWAN_DEVEUI | `CUSTOM` |
| Port | select the CP2102 serial port |

---

## 5 — Generate credentials

Use the [`credentials.py`](credentials.py) script to generate OTAA credentials and deploy them. Requires Python 3.7+.

### Option A — write into credentials.h (recommended)

This embeds the keys directly in the firmware at compile time.

```bash
# Generate random credentials and write to a sketch folder
python credentials.py --write-h mono-phase-lorawan/credentials.h

# Write the same credentials to all sketch folders at once
python credentials.py \
  --write-h mono-phase-lorawan/credentials.h \
  --write-h three-phase-lorawan/credentials.h \
  --write-h mono-phase-overvoltage/credentials.h

# Use your own keys (colons optional)
python credentials.py \
  --deveui A8:17:58:FF:FE:0C:A6:4F \
  --appeui 0000000000000000 \
  --appkey AABBCCDDEEFF00112233445566778899 \
  --write-h mono-phase-lorawan/credentials.h
```

The script prints the credentials (colon format for the network server) and the C array declarations, then writes `credentials.h`. Register the DevEUI / AppEUI / AppKey on your LoRaWAN network server (TTN, Chirpstack, etc.) before flashing.

### Option B — send via AT commands

If the firmware is already flashed and was compiled with **LORAWAN_AT_SUPPORT ON**, credentials can be pushed over serial without reflashing. Requires pyserial:

```bash
pip install pyserial
```

```bash
# macOS / Linux
python credentials.py --at-send /dev/cu.usbserial-0001

# Windows
python credentials.py --at-send COM3
```

The script sends `AT+LPM=0`, `AT+DevEui=`, `AT+AppEui=`, `AT+AppKey=` in sequence and prints each response.

---

## 6 — Flash a sketch

> **WARNING — do not connect both power sources at the same time.**
> The board can be powered from the meter's 5 V line (VIN) or from USB — never both simultaneously.
> Disconnect the P1 RJ11 cable before connecting USB for programming, then reconnect it afterwards.

1. Generate `credentials.h` with `credentials.py` (see section 5).
2. Open the sketch folder (e.g. `mono-phase-lorawan/`) in Arduino IDE.
3. Click **Upload** (→ arrow button) or press `Ctrl+U` / `Cmd+U`.
4. The IDE compiles and flashes. The board resets automatically when done.
5. Open **Tools → Serial Monitor** at **115200 baud** to see debug output (only while USB is connected).

---

## Troubleshooting

**Port not found / upload fails**
- Check the USB cable supports data (some cables are power-only).
- Reinstall the CP2102 driver and reboot.
- On macOS, check **System Settings → Privacy & Security** — the Silicon Labs kernel extension may need explicit approval.

**Board resets during upload**
- This is normal — the bootloader resets the chip to enter flash mode.

**LoRaWAN join fails**
- Double-check DevEUI, AppEUI, and AppKey in `credentials.h`.
- Confirm your LoRaWAN network server has the device registered with matching keys.
- Make sure you are within range of a gateway on EU868.
