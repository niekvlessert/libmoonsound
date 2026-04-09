# libmoonsound

libmoonsound is a small playback/renderer for MSX MoonSound (OPL4) MWM songs.
It can be used both as a CLI tool (`mwm2wav`) and as a library (for native or
Emscripten builds) so you can integrate MWM playback in your own audio player.

Thanks to the **roboplay** and **libvgm** projects for the core format and
emulation work that makes this possible.

## Project Layout

- `src/` — core sources and public library header
- `src_libvgm/` — minimal libvgm subset required by this project
- `modules/` — external sources (`openmsx`, `roboplay`)
- `music/` — MWM/MWK content
- `build/` — build output
- `mwm2wav` — CLI binary (after build)
- `waves.dat`, `yrw801.rom` — OPL4 ROM/WAVES data used by the tool and library

## Build (CLI)

From the project root:

```bash
cmake -S . -B build
cmake --build build
```

This produces `mwm2wav` in the project root.

### CLI Usage

```bash
./mwm2wav [--seconds N] [--loop[=N]] [--dump] [--noteon] [--debug] [--solo CH] input.mwm output.wav
```

Options:
- `--seconds N` : render exactly N seconds (overrides loop length)
- `--loop[=N]` : when `--seconds` is not used, render intro + N loops (default 1)
- `--dump` : print decoded step data
- `--noteon` : debug PCM output
- `--debug` : extra debug output
- `--solo CH` : render only one channel (0-23)

The CLI also reports:
- Whether the song supports looping
- Whether an MWK file is required
- The generated duration

## Build (Emscripten)

Example build (adjust your Emscripten path as needed):

```bash
emcmake cmake -S . -B build-emscripten \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-emscripten
```

By default, the CLI target is disabled under Emscripten. To force it on:

```bash
emcmake cmake -S . -B build-emscripten \
  -DCMAKE_BUILD_TYPE=Release \
  -DMOONSOUND_BUILD_CLI=ON
```

## Using External libvgm

If your project already uses libvgm, you can avoid duplicate symbols by
building libmoonsound against the external libvgm instead of the bundled
subset:

```bash
cmake -S . -B build -DMOONSOUND_USE_SYSTEM_LIBVGM=ON
```

This switches the include path and sources back to `modules/libvgm`.

## Library API

Public header:

- `src/libmoonsound.h`

### Lifecycle

- `MSContext *ms_create(void);`
- `void ms_destroy(MSContext *ctx);`
- `void ms_stop(MSContext *ctx);`

### Loading Assets

- `int ms_load_mwm_file(MSContext *ctx, const char *mwm_path);`
- `int ms_load_mwk_file(MSContext *ctx, const char *mwk_path);`
- `int ms_load_rom_file(MSContext *ctx, const char *rom_path);`
- `int ms_load_waves_file(MSContext *ctx, const char *waves_path);`

### Playback Configuration

- `void ms_set_seconds_limit(MSContext *ctx, int seconds);`
- `void ms_clear_seconds_limit(MSContext *ctx);`
- `void ms_set_loop_count(MSContext *ctx, int loops);`
- `bool ms_supports_loop(MSContext *ctx);`
- `bool ms_requires_mwk(MSContext *ctx);`

### Length / Rendering

- `uint32_t ms_calculate_length_samples(MSContext *ctx, int loops);`
- `uint32_t ms_get_total_samples(MSContext *ctx);`
- `int ms_prepare(MSContext *ctx);`
- `uint32_t ms_render(MSContext *ctx, int16_t *out_interleaved, uint32_t frames);`

`ms_render` outputs interleaved stereo 16-bit PCM frames. Call it repeatedly
until it returns 0.

## Notes

- The current core uses global playback state internally, so only one
  `MSContext` should be active at a time.
- `ms_prepare` must be called after loading the MWM/MWK and ROM/WAVES files.

