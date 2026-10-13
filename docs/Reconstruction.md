# Reconstruction: goal, builds and plan

## Goal

Recreate the **source code** of Caustic's engine. Compiled for each platform,
it should give the same code as the shipped libraries: the same classes,
signatures and member order, the same control flow, and expressions of the
same shape, order and types. Byte-identical output is not required.

No build is the reference. The source is. A build's floating-point rounding
is the compiler's choice: whether to fuse a multiply and an add, say. Source
with the original's expression shapes leads the same compiler to the same
choices.

## The builds

| Build | ABIs | Compiler | Names |
|---|---|---|---|
| 3.2.0 (beta 7) | armeabi-v7a (VFPv3-D16) | GCC 4.6 / 4.8 | ~7000 exported C++ names |
| 3.2.2 | arm64-v8a, armeabi-v7a, x86, x86_64 | clang 21 (Android r563880c), LTO, PGO, BOLT | Java entry points and libc++ only |

The 3.2.2 APK (`Caustic_3_2_2_64b.apk`, sha256 `72adf9bc…9d5c1`) carries
four libraries, all from the same clang 21 toolchain:

| ABI | Size | sha256 |
|---|---|---|
| arm64-v8a | 4298264 | `2fafebda9b8737fe57ba9d231b8dab4308173d4e8a8e2e5f3c690b47a700a2ac` |
| armeabi-v7a (VFPv3, NEON) | 3484108 | `949a73fcc8c1fb1e79a6878aa606bb104112a740c13c1e14181114f1ed62e0cb` |
| x86 | 4106460 | `78a44d4631e0c72aac481a433d537be93a22d8a2202b7d50f0c5715f0bdcf87e` |
| x86_64 | 4543240 | `1e11d207d8e06d8cc05c9878de8f68b057b89f18f26faed6c2ab390644371b9a` |

The 3.2.0 (beta 7) library is
`dac64ee7e3c0cea6a4059cffd2f8629aa55a6782f92d68ebeb1c5348c8fbd7fb`.

What was checked:

- **Version strings.** All four 3.2.2 libraries call themselves
  "Caustic 3.2.1"; only the APK says 3.2.2.
- **Names.** The 3.2.2 libraries export only the Java entry points and
  libc++, none of the engine's C++ names. The 3.2.0 (beta 7) library is the
  only one with names, so it remains the map: functions are located in the
  3.2.2 libraries by strings, constants and structure.
- **SubSynth looks the same.** Every SubSynth control name and label is
  present in 3.2.2. So are these constants, most visibly in the x86 builds,
  which carry them as instruction immediates:
  - the 0.333/0.666 modulation-mode thresholds;
  - both pitch-sweep decays;
  - the 0.9 and 0.999 level smoothing;
  - the 2.5 modulation scale;
  - the LFO smoothing and cycle.
- **Fused multiply-adds.** The arm64 library uses them:

  | Instruction | Count |
  |---|---|
  | `fmadd` | 1290 |
  | `fmla` | 391 |
  | `fnmsub` | 95 |
  | `fmsub` | 59 |
  | `fmls` | 46 |
  | `fnmadd` | 4 |

  The armv7 libraries are VFPv3, which has no fused instructions. The x86_64
  library uses none. So the four 3.2.2 libraries can differ in the last bit
  of a float result. The fixed-point audio paths are unaffected.
- **Not found: the filter's 0.36 constant.** `FixedPointSVFilter`'s cutoff
  table uses 0.36 (`0x3EB851EC`). It appears in none of the 3.2.2 libraries,
  so that table was compiled differently. How is not yet established. It is
  the first function to check in 3.2.2.

## Verification

- **Behaviour.** Run the original function and the reconstruction on the
  same inputs and compare. The original runs under an ARM emulator
  (unicorn), with calls outside the function under test stubbed or
  provided. The reconstruction is compiled with clang 21 for the same
  target. Matching outputs on armv7 show the arithmetic is right; matching
  outputs on arm64 also show that the expression shapes lead clang to the
  same fusing.
- **Structure.** Compare the disassembly by hand: calls, constants and
  loops.

## Plan

1. **New source tree.** Original class names, signatures and member order.
   The 32-bit offsets are recorded as comments and checked at compile time
   where the layout allows.
2. **Port the components one class per patch.** Each port follows the
   original's control flow and expression order, with its tests and a
   harness check. `components/` stays until every class is ported.
3. **The UI layer only as far as the engine depends on it.** `UIControl`,
   `UIKnob`, `UISlider`, `UISelector`, `UIValueContainer` and
   `UIControlCollection`, with their value logic (set, get, ranges, the
   parent notification) and no drawing. `SubSynth::OnChildModified` cannot be
   written as the original has it without them.
4. **Versions.** Names and reading come from 3.2.0 (beta 7). Each function is
   then checked against the 3.2.2 armv7 library, and any difference is
   recorded.

The first steps are the harness itself and a port of `FixedPointSVFilter`.
