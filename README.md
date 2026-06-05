# AV-Plaits

**Mutable Instruments Plaits** macro oscillator for Akashic Vault.

24 synthesis engines — virtual analog, FM, waveshaping, grain, wavetable,
chord, string, modal, drums, noise, speech, and more.

## Structure

```
core/plaits/   Vendored Plaits DSP (MIT, unchanged)
core/stmlib/   Vendored stmlib utilities (MIT, unchanged)
src/           ISoundGenerator wrapper (GPL-3.0)
menu.json      Parameter & menu definition
```

## Integration

Added as a submodule under `src/modules/generators/plaits/` in the
Akashic Vault firmware. Build with `-DTEST` to use portable C++ code
paths (no ARMv7 Thumb-2 assembly).

## Credits

DSP by Émilie Gillet / Mutable Instruments. See `CREDITS.md`.
