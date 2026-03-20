# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Belgian Smart Meter P1 → LoRaWAN IoT firmware. Reads electricity meters via the P1 port (DSMR 5.0 / ESMR 5.0 protocol) and transmits aggregated energy data over LoRaWAN every 15 minutes using a Heltec CubeCell HTCC-AB02A microcontroller.

## Build & Flash

**Toolchain:** Arduino IDE 2.x (no Makefile or CLI build system)

1. Install Heltec CubeCell board support via Board Manager URL: `https://resource.heltec.cn/download/package_CubeCell_index.json`
2. Arduino IDE tools settings:
   - Board: `CubeCell-Board (HTCC-AB02A)`
   - LORAWAN_REGION: `REGION_EU868`
   - LORAWAN_CLASS: `CLASS_A`
   - LORAWAN_NETMODE: `OTAA`
3. Generate and deploy credentials: `python credentials.py --write-h mono-phase-lorawan/credentials.h`
4. Upload via Arduino IDE (Ctrl+U / Cmd+U)

**Debug output:** Serial monitor at 115200 baud. Toggle verbose logging with `#define DEBUG 1` in the `.ino` sketch.

## Credentials Security

- `credentials.h` is git-ignored; never commit it
- Install the pre-commit hook to prevent accidental key commits: `git config core.hooksPath .githooks`
- Use `credentials.h.example` as the template

## Architecture

### Sketch Variants (progressive complexity)

| Sketch | Purpose | Payload |
|--------|---------|---------|
| `serial-config-checker/` | Debug/verify P1 serial wiring only | — |
| `mono-phase-lorawan/` | Single-phase energy monitoring | 62 bytes |
| `three-phase-lorawan/` | Three-phase meter variant | 98 bytes |
| `mono-phase-overvoltage/` | Single-phase + voltage spike detection (Synergrid C10/11) | 62–68 bytes |

### Data Flow

```
P1 Port (115200 baud, 8N1, inverted 5V TTL via BS170 MOSFET)
    → Serial1 (UART_RX2)
    → elec_reader (CRC-16 state machine, extracts 10 OBIS fields/telegram)
    → elec_stats (accumulates min/max/mean over ~900 samples per 15 min)
    → LoRaWAN payload encoder (62–98 bytes)
    → TX every 15 minutes (+ random 15–60 s jitter to reduce congestion)
```

### Core Modules (shared across sketches)

- **`elec_reader.cpp/h`** — Character-by-character DSMR parser. State machine: `IDLE → HEAD → BODY → CSUM`. Validates CRC-16 (poly 0xA001). Extracts timestamp, cumulative Wh (4 registers), instantaneous power/voltage/current.
- **`elec_stats.cpp/h`** — Min/max/mean aggregator. 24-bit little-endian output; 16-bit sample counter.
- **`elec_timestamp.cpp/h`** — Converts P1 timestamp strings (e.g., `"240904223307S"`) to Unix UTC; determines quarter-hour boundary.
- **`dht_reader.cpp/h`** — DHT22 sensor on GPIO4; encodes temperature + humidity as 2 bytes.
- **`voltage_stat.cpp/h`** *(overvoltage variant only)* — Tracks Stage 1 (253 V) and Stage 2 (264.5 V) over-voltage events per Synergrid C10/11.

### GPIO Assignments

| GPIO | Mode | Function |
|------|------|----------|
| GPIO10 | OUTPUT | BS170 inverter enable (P1 power) |
| GPIO7 | OD_HI | P1 Data Request line |
| GPIO4 | INPUT | DHT22 |
| GPIO5 | INPUT (floating) | Random seed |
| GPIO1 | OUTPUT | LED |
| UART_RX2/TX2 | — | Hardware Serial1 ↔ P1 meter |
| USER_KEY | INPUT_PULLDOWN | Manual TX trigger |

### LoRaWAN Payload (v2) Layout

- Byte 0: Frame version (4 bits=2) | user button flag | phase count | reserved
- Bytes 1–4: Unix timestamp (UTC, little-endian)
- Bytes 5–20: Four cumulative Wh registers (consumption day/night, production day/night)
- Byte 21: Current tariff (1 or 2)
- Bytes 22–23: Sample count
- Bytes 24+: Min/max/mean blocks for power consumption, production, voltage, current (24-bit LE each)
- Last 2 bytes: DHT22 temperature & humidity

## Key Design Constraints

- **Dual UART:** USB debug (Serial) and P1 meter (Serial1) run independently to avoid data loss at 115200 baud.
- **Never power via USB + P1 simultaneously** — dual power source risk.
- **15-minute aggregation** — required to comply with EU868 LoRaWAN 1% duty-cycle limit.
- Transmit statistics (min/max/mean), not raw snapshots, to detect anomalies within the window.