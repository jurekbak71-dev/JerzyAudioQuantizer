# FL Studio integration — development branch v0.1
## Tempo-grid workflow
- FL Studio supplies transport, BPM and PPQ to this audio effect.
- Use Piano Roll Jerzy Groove Triplets to create a visual/click timing reference for 1/8, 1/16, 1/16T or shuffle.
- The Audio Quantizer must itself detect guitar attacks and stretch/repair audio; external MIDI cannot directly correct a recorded guitar.
- Do not misrepresent FLP markers as sample-accurate onset detection.
## Verification matrix
Compare RMS/peak, latency, attack preservation and click/echo artefacts after correcting live guitar take: 44.1/48/96 kHz, 80/120/160 BPM, 1/16 and 1/16T, +/− 30..90ms errors. Verify the timeline reset on Stop, project seeking and host tempo automation.
## Current branch
Documentation and test plan only. No untested change to audio DSP or parameter IDs.