# Three-phase — LoRaWAN transmission

Parse the P1 telegram from a **three-phase smart meter**, aggregate per-second measurements over a 15-minute window, and transmit a compact LoRaWAN frame at each quarter-hour boundary.

Extends [mono-phase-lorawan](../mono-phase-lorawan/) with per-phase voltage and current statistics for L2 and L3.

Features:
- OBIS field parsing with CRC-16 validation
- Min / Max / Mean statistics for power, voltage, and current on all three phases (L1, L2, L3)
- DHT22 temperature + humidity (2 bytes in payload)
- Random transmission delay (15–60 s after quarter boundary) to reduce network congestion
- User button triggers an immediate manual transmission

## Setup

> **WARNING — do not connect both power sources at the same time.**
> The board is powered from the meter's 5 V line (VIN). If you also plug in USB while the meter is connected, two power sources will be active simultaneously and can damage the board or the meter.
> Disconnect the RJ11 cable before connecting USB for programming, then reconnect it.

1. Copy `credentials.h` from `credentials.h.example` (or use the provided zeroed template) and fill in your LoRaWAN OTAA keys.
2. Set `DEBUG 1` in the sketch to enable serial debug output during development, then set it back to `0` for production.
3. Upload to the CubeCell HTCC-AB02A board.

> **Note on LoRaWAN payload size:** The frame is 98 bytes. In EU868, this requires at least DR3 (SF9/125 kHz, max 115 bytes). DR0–DR2 cannot carry this payload — ensure your network server and device use ADR or a fixed data rate of DR3 or higher.

## Additional OBIS fields (vs mono-phase)

| OBIS code    | Name        | Unit | Description |
|--------------|-------------|------|-------------|
| `1-0:52.7.0` | Inst-L2-dV  | dV   | Instantaneous voltage L2 (decivolts) |
| `1-0:72.7.0` | Inst-L3-dV  | dV   | Instantaneous voltage L3 (decivolts) |
| `1-0:51.7.0` | Inst-L2-cA  | cA   | Instantaneous current L2 (centiamps) |
| `1-0:71.7.0` | Inst-L3-cA  | cA   | Instantaneous current L3 (centiamps) |

## LoRaWAN payload format (v2, three-phase)

Frame size: **98 bytes**.

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
| Temperature | 8 | 96 | `(val / 2) - 40 = °C` |
| Humidity | 8 | 97 | 0–100 % |

Each Min/Max/Mean block: 3 × 3 bytes, 24-bit little-endian unsigned integers.
