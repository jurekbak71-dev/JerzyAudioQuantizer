# JERZY AUDIO QUANTIZER v0.2

Real-time transient-based rhythmic audio quantizer for guitar and other attack-based audio.

## DSP chain

1. HP-filtered transient detector with fast/slow envelope novelty measure.
2. Host BPM + PPQ sync from JUCE AudioPlayHead.
3. Musical grid quantizer.
4. Strength/window/swing calculation.
5. Lookahead ring buffer.
6. WSOLA-inspired waveform-similarity search around the requested timing offset.
7. Equal-power overlap-add splice between constant-speed read heads.
8. Protected transient region.

## Parameters

- QUANTIZE on/off
- SENSITIVITY
- THRESHOLD
- GRID: 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T
- STRENGTH 0-100%
- WINDOW 2-120 ms
- LOOKAHEAD 10-300 ms
- SMOOTH
- SWING
- TRANSIENT PRESERVE 0-40 ms
- QUALITY: LIVE / STUDIO

## LIVE vs STUDIO

LIVE uses a shorter waveform search and shorter comparison window to reduce CPU.
STUDIO searches a wider region and uses a longer similarity window for cleaner splices.

## Why this does not change pitch

The engine does not continuously accelerate/decelerate the guitar signal. Instead, it changes
where the buffered audio is read and chooses the splice point by waveform similarity. Both
read heads run at normal 1x speed. The transition is overlap-added with equal-power gains.

This is especially appropriate for small/medium guitar timing corrections. It avoids the
continuous pitch modulation that a simple variable-delay approach would create.

## Transient Preserve

The splice is completed before the protected attack region reaches the output. The attack
itself is therefore not used as the overlap zone, which helps retain pick definition.

## Build on Windows

Requirements:
- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git
- Internet access during first configure (JUCE is fetched by CMake)

PowerShell:

```powershell
.\build_windows.ps1
```

or manually:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected output:

```text
build\JerzyAudioQuantizer_artefacts\Release\VST3\JERZY AUDIO QUANTIZER.vst3
```

Copy to:

```text
C:\Program Files\Common Files\VST3\
```

then rescan plugins in FL Studio.

## Guitar starting settings

Rhythm guitar:
- Grid: 1/16
- Sensitivity: 0.65-0.75
- Threshold: -45 to -35 dB
- Strength: 70-90%
- Window: 35-55 ms
- Lookahead: 80-120 ms
- Smooth: 6-10 ms
- Transient Preserve: 15-25 ms
- Quality: STUDIO

Tighter palm-muted playing:
- Window: 20-35 ms
- Preserve: 8-15 ms
- Strength: 80-100%

## Current limitation

v0.2 is a real-time similarity-aligned time-domain splice engine. It is substantially safer
for pitch than the v0.1 variable-delay transition, but it is not an offline multisegment
elastique-style stretcher. Very large corrections or dense polyphonic sustained material can
still produce audible repetition/omission artifacts. The intended range is timing repair of
individual guitar attacks with moderate offsets.
