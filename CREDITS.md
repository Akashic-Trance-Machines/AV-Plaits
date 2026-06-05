# AV-Plaits — Credits

## Original DSP Core

- **Project**: Plaits (Eurorack macro oscillator)
- **Author**: Émilie Gillet / Mutable Instruments
- **Repository**: https://github.com/pichenettes/eurorack
- **License**: MIT
- **Files**: `core/plaits/` — vendored unchanged from upstream `master`

## Support Library

- **Project**: stmlib (STM32 utility library)
- **Author**: Émilie Gillet / Mutable Instruments
- **Repository**: https://github.com/pichenettes/stmlib
- **License**: MIT
- **Files**: `core/stmlib/` — portable subset (dsp, utils, fft)

## Port

- **Wrapper**: `src/plaits_generator.cpp/.h` — ISoundGenerator adapter
- **Author**: Akashic Trance Machines Team
- **License**: GPL-3.0

## Changes from upstream

- Compiled with `-DTEST` to use portable C++ paths instead of ARMv7 Thumb-2
  inline assembly (`ssat`/`usat`/`vsqrt.f32` → standard C++ equivalents).
- No modifications to the DSP source files in `core/`.
