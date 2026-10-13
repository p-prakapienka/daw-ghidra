# Differential checks

These run a function from an original `libcaustic.so` and the
reconstruction's version of it on the same inputs, and compare the results
bit for bit. The original runs under the unicorn ARM emulator; the
reconstruction runs natively, through a small driver program.

The library is not in the repository. Point `CAUSTIC_SO` at your own copy.
Only the 3.2.0 (beta 7) armv7 build is supported so far: it is the one with
the engine's C++ names, which is how functions are found.

## Running

```
pip install unicorn pyelftools
g++ -std=c++20 -O2 -ffp-contract=off -I. components/*.cpp tools/differential/driver.cpp -o build/differential-driver
cd tools/differential
CAUSTIC_SO=/path/to/libcaustic.so python3 check_filter.py ../../build/differential-driver
```

The command exits non-zero and prints each mismatch, if there are any.

## Checks

`check_filter.py` covers `FixedPointSVFilter`:

- **Constructor.** Both tables and the initial state, for both flag values.
- **`ProcessCV`.** 81 scenarios over 128 samples:
  - every mode, plus mode 0 and an unknown mode;
  - cutoff and resonance across their ranges and past them;
  - envelope inverted or not, gate open and closed;
  - with and without keyboard tracking, mono and stereo, with and without
    modulation.

  Each scenario compares the output buffer, the input gain and both
  resolved coefficients.

## How the original is run (`caustic_emu.py`)

**Loading.**

- The library is loaded at `0x40000000` with its `R_ARM_RELATIVE`
  relocations applied.
- Every imported symbol and every PLT stub gets a name, so a call to an
  import can be recognised.
- The low 2 MB of memory hold the word `0x100` everywhere. A missing object
  or vtable chases back into that page and lands on a stub instead of
  faulting.
- VFP is enabled by hand (`CPACR`, `FPEXC`, Cortex-A15); without it the
  first `vpush` faults.

**Calls.**

- One function runs for real. Its calls into the rest of the library return
  at once with `r0` unchanged, except address ranges passed as `real=`.
  `check_filter.py` passes every `ADSR::` function, so the envelope the
  filter calls through its vtable is the original's.
- The 3.2.0 (beta 7) build is softfp, so floats travel in core registers.
  `f2u` and `u2f` convert.

**Imports.**

- The libm functions are the host's own, called through `ctypes`: the same
  ones the driver links against. Differences found are then the code's, not
  libm's. Bionic's `powf` can differ from glibc's in the last bit; that is a
  separate question.
- `memset`, `memcpy` and `memmove` are provided. `operator new` allocates
  from a bump heap.
- Any other import is stubbed and reported.

**Pitfalls already hit.**

- numpy's float32 `power` is not glibc's `powf`. It produced one-off
  differences in the filter tables.
- The host `ControlVoltage` struct holds a native pointer at `0x1C`, so on a
  64-bit host it is not the engine's 0x60-byte layout. The driver sends
  `controlVoltageLayout::Engine` instead.
