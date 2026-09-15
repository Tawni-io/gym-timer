<div align="center">
  <img src="docs/images/tawni-logo.png" alt="Tawni" width="140">

  <p><strong>Gym Timer</strong></p>

  <p>
    Stopwatch, countdown, and gym intervals on the same Tawni box.<br>
    Same pocket cabin, this firmware.
  </p>

  <p>
    <a href="https://github.com/Tawni-io/gym-timer/releases"><strong>Releases</strong></a>
    ·
    <a href="https://tawni.io/gym.html"><strong>Product page</strong></a>
    ·
    <a href="https://tawni.io"><strong>tawni.io</strong></a>
  </p>

  <p>
    Current version: <strong>v0.1.0</strong>
    ·
    <a href="CHANGELOG.md">Changelog</a>
  </p>
</div>

---

## Table of Contents

- [About The Project](#-about-the-project)
- [Getting Started](#-getting-started)
- [Usage](#-usage)
- [Roadmap](#️-roadmap)
- [License](#-license)

---

## 📖 About The Project

<div align="center">
  <img src="docs/images/about.jpg" alt="Tawni cabin (same hardware as Gym Timer)" width="250">
</div>

<br>

Stopwatch, countdown, and workout intervals on the landscape cabin. Flip modes with the two buttons — or a swipe if the touch screen is fitted. Workout settings live on your phone, not in a cabin menu.

| Mode | Shows |
| --- | --- |
| TIMER | Stopwatch, hundredths |
| COUNTDOWN | Remaining time; short-press top for **+30 s** |
| WORKOUT | N sets of work with rest between sets (no rest after the last set) |

Start/pause applies to the whole session. Workout rest starts on its own. If WORKOUT has not been saved yet, that mode tells you how to open setup.

### Built With

Runs on both Tawni hardware SKUs:

- **Tawni** — original / smallest pocket box
- **Rufous** — Tawni core + GPS + external antenna (GPS unused by this firmware)

Board: [LILYGO T-Display C5](https://www.lilygo.cc/products/t-display-c5) (ESP32-C5, 1.9″ 320×170)

---

## 🚀 Getting Started

Buyers: flash from [tawni.io](https://tawni.io) or a GitHub Release `.bin`. No PlatformIO required.

### Install & update

#### USB — first flash, recovery, and updates

Use a USB-C cable and the flasher on [tawni.io](https://tawni.io), or flash a Release `.bin` from [Releases](https://github.com/Tawni-io/gym-timer/releases) with your usual ESP32 tool.

This firmware’s SoftAP is **workout setup only** — it does **not** upload firmware yet. Until SoftAP firmware update ships, every install and later update uses USB / the tawni.io flasher.

Asset name pattern: `gym-timer-t-display-c5-vX.Y.Z.bin`

<!-- website:omit -->

#### Developer USB (PlatformIO)

Python 3.12+, [PlatformIO Core](https://platformio.org/install/cli), [Git](https://git-scm.com/downloads) on PATH, USB-C cable.

```bash
pio run -e bringup -t upload
pio device monitor -e bringup
```

If upload fails: hold **BOOT**, tap **RST**, release **BOOT**, then upload again.

**Windows:** run this before upload so the flash progress bar does not hang the COM port:

```powershell
chcp 65001
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"
pio run -e bringup -t upload
```

<!-- /website:omit -->

### Setup

Hold the bottom button ~3 s until the cabin shows **SETUP MODE**.

This firmware’s hotspot is **GymTimer**. Other Tawni firmwares use their own SSID so two boxes on the bench do not collide.

1. Join **GymTimer** (open network) → `http://192.168.4.1`
2. Set sets, work, and rest → **Save and Exit** (this drops the Access Point)
3. Or hold bottom (**EXIT**) on the cabin to leave without saving

After a save, WORKOUT is ready to start. Countdown length stays on the device with **+30** / **RESET**.

---

## 🧭 Usage

Two buttons. The enclosure marks the **setup** button (bottom). Flip Display 180 does not swap them. Same map on TIMER, COUNTDOWN, and WORKOUT.

| Input | Action |
| --- | --- |
| Top short | Countdown **+30 s** (ignored on TIMER / WORKOUT) |
| Top long (~1.5 s) | **Mode** — TIMER → COUNTDOWN → WORKOUT |
| Bottom short | **Start / Pause** |
| Bottom long (~1.5 s) | **Reset** |
| Bottom keep holding (~3 s) | Setup hotspot on/off (**GymTimer**) |
| Both held (~3 s) | Soft power-off (deep sleep) |
| Bottom after wake (~1.5 s) | Stay on |

Swipe left/right cycles mode if the touch screen is fitted.

---

## 🗺️ Roadmap

- SoftAP firmware update with PIN / auth (not in v0.1.0 — USB / flasher for now)
- Workout and countdown polish from real gym use

---

## 📄 License

Firmware: [MIT](LICENSE).

<!-- website:omit -->

## Build from source

```bash
pio run -e bringup
```

Field binary: `.pio/build/bringup/firmware.bin`  
Rename for a release: `gym-timer-t-display-c5-vX.Y.Z.bin`

Public images (GitHub Releases, SoftAP, USB that leaves the bench) are **`build_type = release` only** — never `-ggdb2`.

Version string: `-DTAWNI_GYM_VERSION` in `platformio.ini` (keep the `#ifndef` fallbacks in `src/main.cpp`, `src/softap/softap.cpp`, and `src/ui/splash.cpp` the same). Every GitHub Release must bump that string — splash, SoftAP, and on-device copy all read it. Add a [CHANGELOG](CHANGELOG.md) entry, then tag `vX.Y.Z`.

Touch-IC gold test: `pio run -e bringup_touch -t upload`

Vendored drivers keep their own licenses: [`lib/esp_lcd_st7789`](lib/esp_lcd_st7789) (LilyGO / João Brilha), [`lib/CST816S`](lib/CST816S) (Felix Biego).

<!-- /website:omit -->
