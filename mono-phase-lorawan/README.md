# Mono-phase — LoRaWAN transmission

Parse the P1 telegram, aggregate per-second measurements over a 15-minute window, and transmit a compact LoRaWAN frame at each quarter-hour boundary.

Features:
- OBIS field parsing with CRC-16 validation
- Min / Max / Mean statistics for power, voltage, and current
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

## LoRaWAN payload format (v2)

Frame size: **62 bytes**.

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
| Temperature | 8 | 60 | `(val / 2) - 40 = °C` |
| Humidity | 8 | 61 | 0–100 % |

Each Min/Max/Mean block: 3 × 3 bytes, 24-bit little-endian unsigned integers.

## Next step

To add Synergrid C10/11 over-voltage detection (253 V / 264.5 V thresholds) → [mono-phase-overvoltage](../mono-phase-overvoltage/).
