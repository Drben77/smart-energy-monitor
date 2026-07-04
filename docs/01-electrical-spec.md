# Electrical Design Constraints — NZ Mains

| Parameter | Value | Design implication |
|---|---|---|
| Nominal voltage | 230 V AC (RMS) | Sensor must span 0–340 V peak |
| Voltage tolerance | ±6% (216–244 V) | Components rated ≥260 V minimum |
| Frequency | 50 Hz (±1.5%) | AC cycle period = 20 ms |
| Max outlet current | 10 A (standard socket) | Current sensor range: 0–10 A min |
| Max power (apparent) | ~2300 W (230 V × 10 A) | Upper limit of power measurement |
| Plug standard | AS/NZS 3112 Type I | 3-pin: Active, Neutral, Earth |
| Waveform | Sinusoidal AC | Must measure true RMS, not average |

> ⚠️ **Safety note:** This device measures live mains voltage. Every design
> decision must prioritise galvanic isolation between the high-voltage side
> and everything a user or microcontroller can touch.