# Belgian Smart Meter P1 → LoRaWAN

Read your Belgian smart meter (ORES / Fluvius) via the **P1 port** and forward aggregated electrical data over **LoRaWAN** every 15 minutes using a Heltec CubeCell board.

## Quick start

```bash
# 1. Clone the repository
git clone https://github.com/YOUR_USERNAME/p1-meter-lorawan.git
cd p1-meter-lorawan

# 2. Enable the pre-commit hook (blocks accidental credential leaks)
git config core.hooksPath .githooks

# 3. Generate your LoRaWAN credentials from your TTN / Chirpstack device page
python3 credentials.py --deveui AA:BB:CC:DD:EE:FF:00:11 \\
                        --appeui 00:00:00:00:00:00:00:00 \\
                        --appkey AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99 \\
                        --write-h mono-phase-lorawan/credentials.h

# 4. Install required Arduino libraries (Tools → Manage Libraries):
#    - "DHT sensor library" by Adafruit  (also install its dependency: Adafruit Unified Sensor)

# 5. Open the sketch in Arduino IDE, build and flash
```

> **Credentials:** `credentials.h` is git-ignored. A zeroed template is provided as
> `credentials.h.example` in each sketch folder. The pre-commit hook will block any
> commit that contains non-zero keys in `credentials.h`.

---

## Project steps

| Folder | Purpose |
|--------|---------|
| [`serial-config-checker/`](serial-config-checker/) | Dump raw P1 serial output to USB — verify wiring and baud rate |
| [`mono-phase-lorawan/`](mono-phase-lorawan/) | Parse OBIS fields, aggregate statistics, transmit over LoRaWAN (single-phase meter) |
| [`three-phase-lorawan/`](three-phase-lorawan/) | Same as above but with L2/L3 voltage and current stats (three-phase meter) |
| [`mono-phase-overvoltage/`](mono-phase-overvoltage/) | mono-phase-lorawan + Synergrid C10/11 over-voltage detection (253 V / 264.5 V) |

Before flashing any sketch, see **[board-setup.md](board-setup.md)** for driver installation, Arduino IDE board support, and upload instructions.

---

## Hardware

### Why the Heltec CubeCell HTCC-AB02A?

The CubeCell HTCC-AB02A was chosen because it exposes **two independent hardware serial (UART) lines**:

- **Serial** (UART0) — connected to the USB bridge, used for debug output and programming.
- **Serial1** (UART1, pins UART_RX2 / UART_TX2) — dedicated to reading the P1 port of the electrical meter.

Having two separate hardware UARTs is essential: the P1 port streams data continuously at 115200 baud 8N1, so it must be handled on a real hardware peripheral — a software serial implementation would lose bytes at that speed or block the LoRaWAN stack.

As an added bonus, the board integrates a **Semtech SX1262 LoRa transceiver and a 868 MHz antenna**, and is supported by the Heltec Arduino SDK with a ready-made LoRaWAN stack.

Board documentation and pinout are available in [`sources/`](sources/).

### Components

| Component | Reference | Notes |
|-----------|-----------|-------|
| Arduino board | Heltec CubeCell HTCC-AB02A | Two hardware UARTs + integrated LoRa 868 MHz |
| Signal inverter / level shifter | BS170 N-channel MOSFET | Inverts 5 V → 3.3 V (common-drain) |
| Meter connector | RJ11 6P6C | P1 port on Belgian smart meters (pins 1, 2, 3, 5, 6 used) |
| Environmental sensor | DHT22 | Temperature + humidity (all sketches except serial-config-checker) |
| Pull-up resistors | 2 × 10 kΩ (R1, R2) | BS170 inverter biasing (R1 on source/RX2, R2 on gate/Data) |
| DHT pull-up resistor | 1 × 10 kΩ (RPullUpDHT) | DHT22 data line pull-up to VEXT |
| LED current limiter | 1 × 220 Ω (R3) | TX indicator LED |
| TX indicator LED | 5 mm red LED | Driven by GPIO1 via R3 |
| Power jumper | 2-pin header (5V_JP) | Connects P1 +5 V to VIN — **remove when using USB** |

### Wiring — P1 port to CubeCell

> **WARNING — do not connect both power sources at the same time.**
> The CubeCell is powered from the meter's 5 V line (VIN pin).
> If you also plug in a USB cable while the meter is connected, you will feed two power sources into the board simultaneously, which can damage the board or the meter.
> Remove the 5V_JP jumper before connecting USB (for programming or debug).

The P1 port outputs **inverted** 5 V TTL serial (5 V = logic 0, 0 V = logic 1).
The BS170 MOSFET inverts the signal and shifts it to 3.3 V in a single stage using a common-drain configuration:
- **Gate** receives the P1 data signal (with R2 10 kΩ pull-up to GPIO7).
- **Source** outputs the inverted 3.3 V signal to UART_RX2 (with R1 10 kΩ pull-up to GPIO7).
- **Drain** is tied to GND.

GPIO7 is set HIGH to provide 3.3 V through R1 and R2, biasing the inverter. Setting GPIO7 LOW disables reception.

```
Belgian Meter P1 (RJ11 6P6C)     CubeCell HTCC-AB02A
─────────────────────────────────────────────────────
Pin 1  +5V  ─────[5V_JP]────────→  VIN  (powers the board)
Pin 2  RTS  ─────────────────────→  +5V  (Data Request, active HIGH — hardwired to 5 V)
Pin 3  GND  ─────────────────────→  GND
Pin 4  NC
Pin 5  Data ─────────────────────→  BS170 gate (via R2 10 kΩ pull-up to GPIO7)
Pin 6  GND  ─────────────────────→  GND

BS170 inverter (common-drain configuration):
       BS170 gate   ← P1 Data + R2 (10 kΩ pull-up to GPIO7)
       BS170 source → UART_RX2 (Serial1) + R1 (10 kΩ pull-up to GPIO7)
       BS170 drain  → GND

GPIO7 (OUTPUT, set HIGH) provides 3.3 V pull-up bias through R1 and R2,
effectively enabling/disabling the inverter.
```

> **Data Request line**: The P1 spec requires the Data Request line (RJ11 pin 2) to be
> pulled HIGH to enable telegram output. On this PCB, it is hardwired to the +5 V rail,
> so the meter streams continuously whenever power is present.

### DHT22 (mono-phase-lorawan, three-phase-lorawan, mono-phase-overvoltage)

```
DHT22               CubeCell
─────────────────────────────
VCC  ─────────────→  VEXT (switchable 3.3 V)
GND  ─────────────→  GND
DATA ─[10 kΩ]─────→  GPIO9 / SDA1  (pull-up to VEXT)
```

### GPIO pin summary

| GPIO | Direction | Function |
|------|-----------|----------|
| GPIO7  | OUTPUT | BS170 inverter enable (set HIGH to provide 3.3 V pull-up bias via R1 + R2) |
| GPIO9 / SDA1 | INPUT | DHT22 data (10 kΩ pull-up to VEXT) |
| GPIO5  | INPUT  | Floating — used for `randomSeed()` |
| GPIO1  | OUTPUT | TX LED indicator (via R3 220 Ω) |
| USER_KEY | INPUT_PULLDOWN | User button (manual LoRa send) |
| UART_RX2 / UART_TX2 | — | Hardware Serial1, 115200 8N1 (RX2 receives inverted P1 data via BS170) |

---

## P1 port & DSMR/ESMR protocol

Belgian smart meters (Fluvius in Flanders, ORES in Wallonia) expose a **P1 port** that outputs an DSMR 5.0 / ESMR 5.0 telegram every second.

### Telegram structure

```
/FLU5\253770234_A              ← header: slash + manufacturer ID

0-0:1.0.0(240904223307S)       ← OBIS object: timestamp
1-0:1.8.1(000328.928*kWh)      ← OBIS object: consumption T1
...
!E4C7                          ← CRC-16 checksum
```

Each telegram line contains one **OBIS code** followed by its value in parentheses.

### OBIS code format

```
A-B:C.D.E
│ │ │ │ └─ Tariff / sub-index
│ │ │ └─── Quantity (measurement type)
│ │ └───── Value group (channel)
│ └──────── Medium group (0=abstract, 1=electricity, 2=heat, 3=gas, 6=water)
└────────── Energy type
```

### OBIS codes extracted by this project

All sketches (except serial-config-checker) extract these fields:

| OBIS code     | Name          | Unit | Description |
|---------------|---------------|------|-------------|
| `0-0:1.0.0`   | Timestamp     | —    | Date-time stamp (local time, S=Summer/CEST, W=Winter/CET) |
| `1-0:1.8.1`   | Cons-Day-Wh   | Wh   | Total electricity delivered to client — Tariff 1 (day) |
| `1-0:1.8.2`   | Cons-Night-Wh | Wh   | Total electricity delivered to client — Tariff 2 (night) |
| `1-0:2.8.1`   | Prod-Day-Wh   | Wh   | Total electricity delivered by client — Tariff 1 (day) |
| `1-0:2.8.2`   | Prod-Night-Wh | Wh   | Total electricity delivered by client — Tariff 2 (night) |
| `0-0:96.14.0` | Night1-Day2   | —    | Active tariff indicator (1=night/T1, 2=day/T2) |
| `1-0:1.7.0`   | Cons-W        | W    | Instantaneous active power import (+P total) |
| `1-0:2.7.0`   | Prod-W        | W    | Instantaneous active power export (-P total) |
| `1-0:32.7.0`  | Inst-L1-dV    | dV   | Instantaneous voltage L1 (decivolts, divide by 10 for V) |
| `1-0:31.7.0`  | Inst-L1-cA    | cA   | Instantaneous current L1 (centiamps, divide by 100 for A) |

three-phase-lorawan additionally extracts:

| OBIS code    | Name         | Unit | Description |
|--------------|--------------|------|-------------|
| `1-0:52.7.0` | Inst-L2-dV   | dV   | Instantaneous voltage L2 (decivolts) |
| `1-0:72.7.0` | Inst-L3-dV   | dV   | Instantaneous voltage L3 (decivolts) |
| `1-0:51.7.0` | Inst-L2-cA   | cA   | Instantaneous current L2 (centiamps) |
| `1-0:71.7.0` | Inst-L3-cA   | cA   | Instantaneous current L3 (centiamps) |

**Sources:**
- DSMR P1 Companion Standard v5.0.2 — [Netbeheer Nederland](https://www.netbeheernederland.nl/publicatie/dsmr-502-p1-companion-standard)
- Fluvius P1 port specification — [Digitale Meters in Vlaanderen](https://www.fluvius.be/nl/thema/meters-en-meterstanden/digitale-meter)
- ORES S1 port specification (Wallonia)
- IEC 62056-21 / OBIS object identification system

---

## Software architecture

### P1 parser

The telegram parser is adapted from [Maarten Pennings' eMeterP1port project (gen2)](https://github.com/maarten-pennings/eMeterP1port/tree/master/gen2).
It is a character-by-character state machine with CRC-16 validation.

States: `IDLE` → `HEAD` → `BODY` → `CSUM` → `IDLE`

CRC-16 polynomial: `x16 + x15 + x2 + 1` (0xA001 reflected).

### Statistics aggregation

Since LoRaWAN duty-cycle limits transmission to once every 15 minutes, ~900 samples are collected per period. For each measured quantity the code tracks:

- **Minimum** value in the window
- **Maximum** value (critical for voltage spike detection)
- **Mean** (sum / count)
- **Sample count** (data quality)

### LoRaWAN transmission timing

A random delay (15–60 s after a quarter-hour boundary) is added before transmitting to avoid all devices on a network transmitting simultaneously.

### Over-voltage detection (mono-phase-overvoltage only)

Belgian grid protection thresholds per **Synergrid C10/11** and **EN 50549-1** (230 V nominal):

| Stage | Threshold | Trip time | Threshold in decivolts |
|-------|-----------|-----------|------------------------|
| Stage 1 (U>) | 1.10 × 230 V = **253.0 V** | 1.5 s sustained | 2530 dV |
| Stage 2 (U>>) | 1.15 × 230 V = **264.5 V** | 0.2 s instant | 2645 dV |

When a grid-connected solar inverter detects sustained over-voltage it disconnects and waits ~60 s before reconnecting. This module tracks:

- Total samples above Stage 1 threshold
- Total samples above Stage 2 threshold
- Maximum consecutive samples above Stage 1 (if ≥ 2 consecutive at 1 sample/s → inverter likely tripped)

---

## LoRaWAN payload format (v2)

Each sketch uses the same frame version (v2) and a shared header, with fields added per sketch. The **NB Phases** nibble in byte 0 tells the decoder which format to expect.

### mono-phase-lorawan / mono-phase-overvoltage — 62 bytes (68 with over-voltage)

| Field | Bits | Byte index | Detail |
|-------|------|------------|--------|
| Frame version | 4 | 0 | Value: 2 |
| User button | 1 | 0 | Pressed = 1 |
| NB Phases | 2 | 0 | Value: 1 |
| RESERVED | 1 | 0 | — |
| Timestamp | 32 | 1 | Unix timestamp (UTC) |
| Cons-Day-Wh | 32 | 5 | Cumulative Wh |
| Cons-Night-Wh | 32 | 9 | Cumulative Wh |
| Prod-Day-Wh | 32 | 13 | Cumulative Wh |
| Prod-Night-Wh | 32 | 17 | Cumulative Wh |
| Night1-Day2 | 8 | 21 | Current tariff |
| Nb Samples | 16 | 22 | Sample count in period |
| Cons-W stats | 72 | 24 | Min/Max/Mean (W) |
| Prod-W stats | 72 | 33 | Min/Max/Mean (W) |
| L1-dV stats | 72 | 42 | Min/Max/Mean (decivolts) |
| L1-cA stats | 72 | 51 | Min/Max/Mean (centiamps) |
| Temperature | 8 | 60 | DHT22: `(val / 2) - 40 = °C` |
| Humidity | 8 | 61 | DHT22: 0–100 % |
| L1-over-253V | 16 | 62 | Stage 1 count *(mono-phase-overvoltage only, if > 0)* |
| L1-over-264.5V | 16 | 64 | Stage 2 count *(mono-phase-overvoltage only, if Stage 1 > 0)* |
| L1-max-consec-253 | 16 | 66 | Max streak *(mono-phase-overvoltage only, if Stage 1 > 0)* |

> The 6 over-voltage bytes are only appended when `L1-over-253V > 0`, saving bandwidth in normal operation.

### three-phase-lorawan — 98 bytes

Same header and common fields as above (bytes 0–59), then:

| Field | Bits | Byte index | Detail |
|-------|------|------------|--------|
| Frame version | 4 | 0 | Value: 2 |
| User button | 1 | 0 | Pressed = 1 |
| NB Phases | 2 | 0 | Value: 3 |
| RESERVED | 1 | 0 | — |
| Timestamp | 32 | 1 | Unix timestamp (UTC) |
| Cons-Day-Wh | 32 | 5 | Cumulative Wh |
| Cons-Night-Wh | 32 | 9 | Cumulative Wh |
| Prod-Day-Wh | 32 | 13 | Cumulative Wh |
| Prod-Night-Wh | 32 | 17 | Cumulative Wh |
| Night1-Day2 | 8 | 21 | Current tariff |
| Nb Samples | 16 | 22 | Sample count in period |
| Cons-W stats | 72 | 24 | Min/Max/Mean (W) |
| Prod-W stats | 72 | 33 | Min/Max/Mean (W) |
| L1-dV stats | 72 | 42 | Min/Max/Mean (decivolts) |
| L1-cA stats | 72 | 51 | Min/Max/Mean (centiamps) |
| L2-dV stats | 72 | 60 | Min/Max/Mean (decivolts) |
| L3-dV stats | 72 | 69 | Min/Max/Mean (decivolts) |
| L2-cA stats | 72 | 78 | Min/Max/Mean (centiamps) |
| L3-cA stats | 72 | 87 | Min/Max/Mean (centiamps) |
| Temperature | 8 | 96 | DHT22: `(val / 2) - 40 = °C` |
| Humidity | 8 | 97 | DHT22: 0–100 % |

> 98 bytes requires at least DR3 (SF9/125 kHz) in EU868. DR0–DR2 cannot carry this payload.

Each Min/Max/Mean stat block is 3 × 3 bytes (24-bit little-endian unsigned integers).

---

## Sources

Local copies of the key reference documents are in the [`sources/`](sources/) folder.

### P1 port / DSMR protocol

| File | Description |
|------|-------------|
| `P1 Companion Standard.pdf` | DSMR P1 Companion Standard v5.0.2 — Netbeheer Nederland |
| `p1-companion-standard.pdf` | DSMR P1 Companion Standard (alternate edition) |
| `Slimme_meter_ESMR50.pdf` | ESMR 5.0 — Dutch smart meter requirements |

### Belgian meter operator documentation

| File | Description |
|------|-------------|
| `fluvius_serial_format.pdf` | Fluvius P1 port serial format (Flanders) |
| `ORES P1 port standard.pdf` | ORES P1 port standard (Wallonia) |
| `ores-s1-port_format.pdf` | ORES S1 port format detail |
| `dmk-demo-v2.1-rtc.pdf` | Digitale Meters in Vlaanderen — full specification |
| `Digitale Meters in Vlaanderen - dmk-demo-v2.1-rtc-p1-p18.pdf` | DMK spec pages 1–18 |
| `Digitale Meters in Vlaanderen - dmk-demo-v2.1-rtc-p19-p57.pdf` | DMK spec pages 19–57 |
| `Digitale Meters in Vlaanderen - dmk-demo-v2.1-rtc-p57-p75.pdf` | DMK spec pages 57–75 |

### Hardware

| File | Description |
|------|-------------|
| `BS170_FairchildSemiconductor.pdf` | BS170 N-channel MOSFET datasheet (signal inverter) |
| `P1 poort slimme meter uitlezen (hardware).pdf` | P1 hardware wiring guide |
| `AB02A.pdf` | Heltec CubeCell HTCC-AB02A datasheet |
| `HTCC-AB02A_PinoutDiagram.pdf` | CubeCell HTCC-AB02A pinout |
| `HTCC-AB02A_SchematicDiagram.pdf` | CubeCell HTCC-AB02A schematic |
| `CubeCell Series Quick Start — cubecell latest documentation.pdf` | CubeCell getting started guide |

### Articles

| File | Description |
|------|-------------|
| `Read data from the Belgian digital meter through the P1 port Jensd's I_O buffer.pdf` | Jensd blog — reading the Belgian P1 meter (basis for this project) |

## Resources

- [Maarten Pennings — eMeterP1port (parser basis)](https://github.com/maarten-pennings/eMeterP1port/tree/master/gen2)
- [DSMR P1 Companion Standard v5.0.2](https://www.netbeheernederland.nl/publicatie/dsmr-502-p1-companion-standard)
- [Jensd — Read data from the Belgian digital meter through the P1 port](https://jensd.be/1183/linux/read-data-from-the-belgian-digital-meter-through-the-p1-port)
- [domoticx.com — P1 poort slimme meter uitlezen (hardware)](http://domoticx.com/p1-poort-slimme-meter-uitlezen-hardware/)
- [Heltec CubeCell HTCC-AB02A documentation](https://heltec.org/project/htcc-ab02a/)
- [Synergrid C10/11 — Grid connection regulations Belgium](https://www.synergrid.be/)
- [EN 50549-1 — Requirements for generating plants to be connected in parallel with distribution networks](https://www.en-standard.eu/)

---

## License

MIT — see [LICENSE](LICENSE).
