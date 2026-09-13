# Changelog

On-device string: `TAWNI_GYM_VERSION` in `platformio.ini`.  
Release binaries: `gym-timer-t-display-c5-vX.Y.Z.bin`

Newest first.

## 0.5.4

- TIMER, COUNTDOWN, and WORKOUT on the same two-button map
- Top short: countdown +30 s; top long: mode (TIMER → COUNTDOWN → WORKOUT)
- Bottom short: start/pause; bottom long (~1.5 s): reset; keep holding (~3 s): setup
- Both buttons ~3 s: soft-off; hold bottom ~1.5 s after wake to stay on
- Setup hotspot **GymTimer** (open) → `http://192.168.4.1`
- SoftAP is workout-only: sets × work, rest between sets; **Save and Exit** drops the hotspot
- SETUP MODE cabin: join copy in the middle, bottom hint **EXIT**
- Unconfigured WORKOUT face explains how to open setup
- Countdown length stays on-device (+30 / reset); no countdown or flip controls on the phone page
