"""
P1 Meter LoRaWAN payload decoder.

Supports three firmware variants (auto-detected from the header byte):
  - Mono-phase          (62 bytes,  nb_phases=1)
  - Mono-phase + Synergrid C10/11 overvoltage detection (68 bytes, nb_phases=1)
  - Three-phase         (98 bytes,  nb_phases=3)

Usage:
    python decoder.py <base64_payload>
    python decoder.py          # runs built-in examples
"""

import base64
import datetime
import sys
from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# Little-endian integer helpers
# ---------------------------------------------------------------------------

def _u16(data: bytes, i: int) -> int:
    return int.from_bytes(data[i : i + 2], "little")

def _u24(data: bytes, i: int) -> int:
    return data[i] | (data[i + 1] << 8) | (data[i + 2] << 16)

def _u32(data: bytes, i: int) -> int:
    return int.from_bytes(data[i : i + 4], "little")


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class Stats:
    """Min / max / mean statistics aggregated over a 15-minute window."""
    min: float
    max: float
    mean: float

    def __repr__(self) -> str:
        return f"(min={self.min}, max={self.max}, mean={self.mean})"


@dataclass
class OvervoltageData:
    """Synergrid C10/11 overvoltage counters (mono-phase-overvoltage variant)."""
    over_253v_count: int           # samples that exceeded Stage1 = 253.0 V (1.10 × 230 V)
    over_264v5_count: int          # samples that exceeded Stage2 = 264.5 V (1.15 × 230 V)
    max_consecutive_over_253v: int # longest consecutive run above Stage1


@dataclass
class DecodedFrame:
    # Header
    frame_version: int
    user_button_pressed: bool
    nb_phases: int                 # 1 = mono-phase, 3 = three-phase

    # Meter timestamp
    timestamp: int                 # Unix UTC seconds
    datetime_utc: datetime.datetime

    # Cumulative energy indexes (Wh)
    index_cons_day_wh: int
    index_cons_night_wh: int
    index_prod_day_wh: int
    index_prod_night_wh: int

    # Tariff: 1 = night, 2 = day
    tariff: int
    nb_samples: int

    # Power / voltage / current statistics
    cons_w: Stats                  # Active power import, W
    prod_w: Stats                  # Active power export, W
    l1_voltage_v: Stats            # L1 voltage, V
    l1_current_a: Stats            # L1 current, A

    # Three-phase extras (None for mono-phase)
    l2_voltage_v: Stats | None
    l3_voltage_v: Stats | None
    l2_current_a: Stats | None
    l3_current_a: Stats | None

    # DHT22 ambient sensor
    temperature_c: float | None    # None if sensor absent / reading invalid
    humidity_pct: int | None       # 0-100 %

    # Overvoltage (mono-phase-overvoltage variant only, else None)
    overvoltage: OvervoltageData | None

    @property
    def index_cons_total_wh(self) -> int:
        return self.index_cons_day_wh + self.index_cons_night_wh

    @property
    def index_prod_total_wh(self) -> int:
        return self.index_prod_day_wh + self.index_prod_night_wh

    @property
    def tariff_name(self) -> str:
        return {1: "NIGHT", 2: "DAY"}.get(self.tariff, f"UNKNOWN({self.tariff})")


# ---------------------------------------------------------------------------
# Decoder
# ---------------------------------------------------------------------------

def _decode_stats(data: bytes, i: int, factor: float = 1.0) -> tuple[Stats, int]:
    min_v = _u24(data, i) * factor
    max_v = _u24(data, i + 3) * factor
    mean_v = _u24(data, i + 6) * factor
    return Stats(min_v, max_v, mean_v), i + 9


def decode(payload: bytes) -> DecodedFrame:
    """Decode a raw P1 LoRaWAN payload into a DecodedFrame."""
    n = len(payload)
    if n < 62:
        raise ValueError(f"Payload too short: {n} bytes (minimum 62)")

    i = 0

    # Byte 0 – packed header
    header = payload[i]; i += 1
    frame_version = (header >> 4) & 0x0F
    user_button = bool((header >> 3) & 0x01)
    nb_phases = (header >> 1) & 0x03   # 1 = mono, 3 = three-phase

    # Bytes 1-4 – Unix UTC timestamp
    ts = _u32(payload, i); i += 4
    dt = datetime.datetime.fromtimestamp(ts, tz=datetime.timezone.utc)

    # Bytes 5-20 – cumulative Wh indexes (4 × uint32)
    cons_day   = _u32(payload, i); i += 4
    cons_night = _u32(payload, i); i += 4
    prod_day   = _u32(payload, i); i += 4
    prod_night = _u32(payload, i); i += 4

    # Byte 21 – tariff indicator
    tariff = payload[i]; i += 1

    # Bytes 22-23 – sample count
    nb_samples = _u16(payload, i); i += 2

    # Power statistics
    cons_w, i = _decode_stats(payload, i)
    prod_w, i = _decode_stats(payload, i)

    # Frame version 0 had extra per-phase power stats before voltage/current
    if frame_version == 0:
        _, i = _decode_stats(payload, i)  # legacy L1 cons – skip
        _, i = _decode_stats(payload, i)  # legacy L1 prod – skip

    # L1 voltage (stored as decivolts → V) and current (stored as centiamps → A)
    l1_v, i = _decode_stats(payload, i, factor=0.1)
    l1_a, i = _decode_stats(payload, i, factor=0.01)

    # Three-phase: L2 and L3 voltage + current
    l2_v = l3_v = l2_a = l3_a = None
    if nb_phases == 3:
        l2_v, i = _decode_stats(payload, i, factor=0.1)
        l3_v, i = _decode_stats(payload, i, factor=0.1)
        l2_a, i = _decode_stats(payload, i, factor=0.01)
        l3_a, i = _decode_stats(payload, i, factor=0.01)

    # DHT22 – 2 bytes (present in all variants)
    temp_c = humidity = None
    if n >= i + 2:
        raw_temp = payload[i]; i += 1
        humidity = payload[i]; i += 1
        temp_c = (raw_temp / 2) - 40
        # Treat all-zero as absent (sensor not connected / failed read)
        if temp_c == -40.0 and humidity == 0:
            temp_c = humidity = None

    # Overvoltage extension – 6 bytes (mono-phase-overvoltage variant)
    overvoltage = None
    if nb_phases == 1 and n >= i + 6:
        over_253   = _u16(payload, i); i += 2
        over_264v5 = _u16(payload, i); i += 2
        max_consec = _u16(payload, i); i += 2
        overvoltage = OvervoltageData(over_253, over_264v5, max_consec)

    return DecodedFrame(
        frame_version=frame_version,
        user_button_pressed=user_button,
        nb_phases=nb_phases,
        timestamp=ts,
        datetime_utc=dt,
        index_cons_day_wh=cons_day,
        index_cons_night_wh=cons_night,
        index_prod_day_wh=prod_day,
        index_prod_night_wh=prod_night,
        tariff=tariff,
        nb_samples=nb_samples,
        cons_w=cons_w,
        prod_w=prod_w,
        l1_voltage_v=l1_v,
        l1_current_a=l1_a,
        l2_voltage_v=l2_v,
        l3_voltage_v=l3_v,
        l2_current_a=l2_a,
        l3_current_a=l3_a,
        temperature_c=temp_c,
        humidity_pct=humidity,
        overvoltage=overvoltage,
    )


def decode_base64(b64: str) -> DecodedFrame:
    """Decode a base64-encoded payload (as received from a LoRaWAN network server)."""
    return decode(base64.b64decode(b64))


# ---------------------------------------------------------------------------
# Pretty printer
# ---------------------------------------------------------------------------

def print_frame(frame: DecodedFrame) -> None:
    phase_label = {1: "Mono-phase", 3: "Three-phase"}.get(frame.nb_phases, f"{frame.nb_phases}-phase")
    print(f"=== P1 LoRaWAN Frame — {phase_label} (v{frame.frame_version}) ===")
    print(f"  Meter timestamp : {frame.datetime_utc.isoformat()}  (Unix {frame.timestamp})")
    print(f"  Tariff          : {frame.tariff_name}")
    print(f"  Samples         : {frame.nb_samples}")
    print(f"  User button     : {frame.user_button_pressed}")
    print()
    print("  Energy indexes")
    print(f"    Consumption day   : {frame.index_cons_day_wh:>10} Wh")
    print(f"    Consumption night : {frame.index_cons_night_wh:>10} Wh")
    print(f"    Consumption total : {frame.index_cons_total_wh:>10} Wh")
    print(f"    Production day    : {frame.index_prod_day_wh:>10} Wh")
    print(f"    Production night  : {frame.index_prod_night_wh:>10} Wh")
    print(f"    Production total  : {frame.index_prod_total_wh:>10} Wh")
    print()
    print("  Power (W)")
    print(f"    Import  {frame.cons_w}")
    print(f"    Export  {frame.prod_w}")
    print()
    print("  L1 Voltage (V)")
    print(f"    {frame.l1_voltage_v}")
    print("  L1 Current (A)")
    print(f"    {frame.l1_current_a}")
    if frame.nb_phases == 3:
        print()
        print("  L2 Voltage (V)")
        print(f"    {frame.l2_voltage_v}")
        print("  L2 Current (A)")
        print(f"    {frame.l2_current_a}")
        print()
        print("  L3 Voltage (V)")
        print(f"    {frame.l3_voltage_v}")
        print("  L3 Current (A)")
        print(f"    {frame.l3_current_a}")
    print()
    if frame.temperature_c is not None:
        print(f"  DHT22  temperature={frame.temperature_c:.1f} °C  humidity={frame.humidity_pct} %")
    else:
        print("  DHT22  not available")
    if frame.overvoltage is not None:
        ov = frame.overvoltage
        print()
        print("  Overvoltage (Synergrid C10/11)")
        print(f"    >253.0 V count       : {ov.over_253v_count}")
        print(f"    >264.5 V count       : {ov.over_264v5_count}")
        print(f"    Max consecutive >253V: {ov.max_consecutive_over_253v}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

EXAMPLES = [
    # Mono-phase (62 bytes) — from convert_base64_to_readable_values.py
    ("Mono-phase",
     "Eoih9GhKIhAAbrMNACMAAAAAAAAAArcCAAAAIgEAqQUADAIAAAAAAAAAAAAAOAkAcgkAUAkAfwAAYAIA4QAA"),
    # Mono-phase (longer frame with padding — demonstrates DHT22 present)
    ("Mono-phase (with samples)",
     "AP///39KIhAAaJ0NACMAAAAAAAAAAoQDAAAAZAAAnQAAfgAAAAAAAAAAAAAAZAAAnQAAfgAAAAAAAAAAAAAAQAkArQkAdAkAMAAASAAAOwAAZAAAnQAAfgAAAAAAAAAAAAAA"),
]

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # Decode a single base64 payload passed on the command line
        b64 = sys.argv[1]
        try:
            frame = decode_base64(b64)
            print_frame(frame)
        except Exception as exc:
            print(f"Error: {exc}", file=sys.stderr)
            sys.exit(1)
    else:
        # Run built-in examples
        for label, b64 in EXAMPLES:
            print(f"\n--- Example: {label} ---")
            print(f"Base64: {b64}")
            raw = base64.b64decode(b64)
            print(f"Length: {len(raw)} bytes")
            try:
                frame = decode_base64(b64)
                print_frame(frame)
            except Exception as exc:
                print(f"  Decode error: {exc}")