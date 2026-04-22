# MWM Debug Notes (RoboPlay Parity)

## Scope

This document records the targeted parity work for MWM playback against RoboPlay semantics and the trace-based verification flow used to confirm behavior.

## Implemented Fixes

### 1) Wave control register (`0x68 + ch`) semantics aligned to RoboPlay

In `src/mwm2wav.c`, wave control writes are now read-modify-write based, matching RoboPlay event behavior:

- `note_on`: writes `0x80 | (current_stereo & 0x0F)`
- `note_off`: clears key-on bit using current register value (`value & 0x7F`)
- `stereo_change`: preserves upper nibble from current register and applies stereo mask
- `damp`: sets bit `0x40` on current value
- `lfo`: toggles bit `0x20` on current value
- `extra_lfo`: temporary `| 0x20`, write `0x80/0xE0`, restore with `& 0xDF`

A local register shadow (`g_wave_regs[256]`) is updated by `write_ymf278b()` and used for these operations.

### 2) Trace instrumentation in `mwm2wav`

Added optional CSV trace output in `src/mwm2wav.c`:

- `--trace <file>`: enable trace logging
- `--trace-ticks <N>`: stop trace after N ticks (audio rendering continues)

Trace includes:

- `TRACE_VERSION,1`
- `SONG,<path>`
- `TICK,<n>`
- `STEP,<tick>,<position>,<step>`
- `EV,<tick>,<channel>,<event>`
- `REG,<tick>,<seq>,<reg>,<val>`

### 3) RoboPlay-semantics reference tracer

Added `src/mwm_ref_trace.c` and CMake target `mwm_ref_trace` to generate a reference trace without using openMSX.

Usage:

```bash
./build/mwm_ref_trace --ticks N --trace out.csv input.mwm [--waves waves.dat] [--mwk file]
```

### 4) Trace normalization comparator

Added `scripts/compare_trace_normalized.sh` to compare traces robustly:

- `L` mode: last value per `(tick, reg)`
- `M` mode: multiset per `(tick, reg, val)`

This avoids false mismatches from benign write ordering differences where end-of-tick state is equivalent.

## Verification Performed

### Build

```bash
make -j4 -C build
```

### Generate current trace

```bash
./build/mwm2wav --seconds 20 --trace RESIST_trace_current_120.csv --trace-ticks 120 \
  /Volumes/EXT_SSD/AI/DMV/DMV1/RESIST.MWM /tmp/RESIST_20s_check.wav
```

### Generate RoboPlay-reference trace

```bash
./build/mwm_ref_trace --ticks 120 --trace RESIST_trace_roboplay_ref.csv \
  /Volumes/EXT_SSD/AI/DMV/DMV1/RESIST.MWM --waves waves.dat \
  --mwk /Volumes/EXT_SSD/AI/DMV/DMV1/OPL4MUS.MWK
```

### Compare normalized traces

```bash
awk -F',' '($1!="REG" || ($2+0)>0){print}' RESIST_trace_roboplay_ref.csv \
  > /tmp/RESIST_trace_roboplay_ref_noinit.csv

./scripts/compare_trace_normalized.sh \
  /tmp/RESIST_trace_roboplay_ref_noinit.csv \
  RESIST_trace_current_120.csv 120
```

Result:

- `FIRST_L_DIFF=NONE`
- `FIRST_M_DIFF=NONE`

Meaning: no normalized register divergence up to tick 120 after excluding tick-0 reference init writes.

## Output Audio Produced

- `/Volumes/EXT_SSD/AI/libmoonsound/RESIST_20s_roboplay_trace_matched.wav`

## Notes

- The reference tracer compares playback event/register semantics, not full analog output characteristics.
- Small residual loudness differences can still exist from emulator/headroom/mix details even when register traces match.
