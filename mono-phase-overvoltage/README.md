# Mono-phase — Over-voltage detection

Everything from [mono-phase-lorawan](../mono-phase-lorawan/), plus Synergrid C10/11 / EN 50549-1 compliant over-voltage monitoring.

This step is designed for households with solar installations where grid over-voltage can cause the inverter to disconnect.

## Over-voltage thresholds (Belgium, 230 V nominal)

| Stage | Threshold | Trip time | Use |
|-------|-----------|-----------|-----|
| Stage 1 (U>) | **253.0 V** (1.10 × 230 V) | 1.5 s sustained | Sustained over-voltage, inverter trips after ~1.5 s |
| Stage 2 (U>>) | **264.5 V** (1.15 × 230 V) | 0.2 s instant | Extreme spike, instant trip |

Per sample (1 sample/s), if `max_consecutive_over_stage1 >= 2` the inverter very likely tripped.

## What is tracked per 15-minute window

- `L1-over-253V` — total samples where voltage ≥ 253.0 V
- `L1-over-264.5V` — total samples where voltage ≥ 264.5 V
- `L1-max-consec-253` — longest uninterrupted streak above 253.0 V

## LoRaWAN payload format (v2)

Frame size: **62 bytes** normally, **68 bytes** when over-voltage occurred.

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
| L1-over-253V | 16 | 62 | Stage 1 count *(only if > 0)* |
| L1-over-264.5V | 16 | 64 | Stage 2 count *(only if Stage 1 > 0)* |
| L1-max-consec-253 | 16 | 66 | Max consecutive streak *(only if Stage 1 > 0)* |

> The 6 over-voltage bytes are only appended when `L1-over-253V > 0`, saving bandwidth in normal operation.

## Setup

> **WARNING — do not connect both power sources at the same time.**
> The board is powered from the meter's 5 V line (VIN). If you also plug in USB while the meter is connected, two power sources will be active simultaneously and can damage the board or the meter.
> Disconnect the RJ11 cable before connecting USB for programming, then reconnect it.

1. Copy `credentials.h` from the provided zeroed template and fill in your LoRaWAN OTAA keys.
2. Set `DEBUG 1` during testing.
3. Upload to the CubeCell HTCC-AB02A board.

## References

- Synergrid C10/11: [synergrid.be](https://www.synergrid.be/)
- EN 50549-1: Requirements for generating plants connected in parallel with distribution networks
