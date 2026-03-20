# Serial Config Checker

Bridge the meter's P1 serial port to USB so you can inspect the raw P1 telegram in a serial monitor.

Use this step to:
- Verify that your wiring is correct
- Confirm the baud rate (115200 8N1 for Belgian ESMR 5.0 meters)
- See which OBIS fields your specific meter outputs
- Capture sample telegrams for offline testing

## How to use

> **WARNING — do not connect both power sources at the same time.**
> The board is powered from the meter's 5 V line (VIN). If you also plug in USB while the meter is connected, two power sources will be active simultaneously and can damage the board or the meter.
> Disconnect the RJ11 cable before connecting USB, and reconnect it afterwards.

1. Upload `serial-config-checker.ino` to the CubeCell board (USB only, RJ11 disconnected).
2. Disconnect USB, connect the RJ11 cable to the meter.
3. The board is now powered by the meter — open a serial monitor at **115200 baud** via USB if needed, but do not connect USB power at the same time.
3. One telegram per second should appear, starting with `/` and ending with `!XXXX`.

## Expected output

```
/FLU5\253770234_A

0-0:96.1.4(50217)
0-0:1.0.0(240904223307S)
1-0:1.8.1(000328.928*kWh)
1-0:1.8.2(000312.703*kWh)
1-0:2.8.1(000000.035*kWh)
1-0:2.8.2(000000.000*kWh)
0-0:96.14.0(0002)
1-0:1.7.0(00.247*kW)
1-0:2.7.0(00.000*kW)
1-0:32.7.0(238.7*V)
1-0:31.7.0(001.21*A)
!E4C7
```

## Notes

- The sketch bridges `Serial1` (meter) → `Serial` (USB) in the `loop()`.
- `GPIO10` powers the BS170 inverter circuit.
- `GPIO7` (OD_HI) holds the Data Request line HIGH to enable continuous output.
- The `readOneMeterMessage()` helper function is present but not called from `loop()` — it is a starting point for message-based reading.

## Next step

Once you can see clean telegrams → proceed to [mono-phase-lorawan](../mono-phase-lorawan/) (single-phase meter) or [three-phase-lorawan](../three-phase-lorawan/) (three-phase meter).
