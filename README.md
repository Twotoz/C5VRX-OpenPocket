<div align="center">

# C5VRX-OpenPocket

**Two-ESP32 digital analog-FPV handheld experiment**

An OpenPocket-focused integration of **C5VRX** and **RivetTX**: use an ESP32-C5 as the experimental 5.8 GHz analog FPV receiver/demodulator and the **exact Waveshare ESP32-S3-LCD-Driver-Board with its 40-pin RGB connector** as the display, radio-control and UI processor.

![status](https://img.shields.io/badge/status-architecture%20%2F%20bring--up-blue)
![RF](https://img.shields.io/badge/video-5.8%20GHz%20analog-orange)
![C5](https://img.shields.io/badge/RF-ESP32--C5-6f42c1)
![S3](https://img.shields.io/badge/UI-Waveshare%20ESP32--S3%2040PIN-00b894)
![display](https://img.shields.io/badge/display-RGB%20LCD-111111)

</div>

---

## What is this?

[C5VRX](https://github.com/Twotoz/C5VRX) asks whether the ESP32-C5's 5 GHz Wi-Fi RF chain can be pushed far enough outside its intended job to receive analog 5.8 GHz FPV video.

This repository asks the next question:

> **If C5VRX can continuously recover composite video, can the exact Waveshare ESP32-S3-LCD-Driver-Board shown in the prototype listing turn that stream into a complete OpenPocket transmitter using its onboard 40-pin RGB LCD connector, battery charger, RivetTX and a converted ExpressLRS receiver used as the TX module?**

The target hardware is intentionally tiny:

```text
                 5.8 GHz analog FPV
                         |
                         v
                 +---------------+
                 |   ESP32-C5    |
                 | RF / I,Q      |
                 | WBFM demod    |
                 | CVBS samples  |
                 | gimbal ADC    |
                 +-------+-------+
                         |
                   QSPI / GDMA
                         |
                         v
          +--------------------------------+
          | Waveshare ESP32-S3-LCD-Driver |
          | Board, N8R8, 40-pin RGB        |
          |                                |
          | PAL/NTSC decode                |
          | RGB framebuffer / LCD          |
          | RivetTX                        |
          | ETA6096 1S charge/power        |
          +----------+---------------------+
                     |
                GPIO43/44 CRSF
                     |
                     v
              +---------------------+
              | ELRS RX flashed TX  |
              | e.g. 100 mW PA      |
              +----------+----------+
                         |
                      2.4 GHz
                         |
                      aircraft
```

The **C5 and S3 are the two main processors**. The converted ELRS receiver is a small external RF module.

---

## Exact S3 board target

This repo does **not** target a generic S3 LCD board.

Target: **Waveshare ESP32-S3-LCD-Driver-Board (SKU 27686)** with:

- ESP32-S3-WROOM-1-N8R8
- 8 MB PSRAM
- 8 MB flash
- onboard USB-C
- ETA6096 1-cell lithium charge/discharge manager
- MX1.25 battery connector
- TCA9554 GPIO expander
- onboard **40-pin 3SPI + RGB connector**

Waveshare's official RGB examples currently use 480x480 ST7701 panels. A different 800x480/480x800 panel is possible only if its electrical pinout and timings match the 40-pin interface or an adapter is used.

See [`docs/waveshare-40pin-target.md`](docs/waveshare-40pin-target.md) for the exact schematic pin map.

---

## Why split C5 and S3?

### ESP32-C5 — RF/video front end + input sampler

```text
5.8 GHz RF
   -> complex I/Q
   -> WBFM discriminator
   -> filtering / decimation
   -> sampled composite video
```

The exact Waveshare S3 board spends most of its native GPIO on RGB. To avoid adding another ADC MCU, the prototype also plans to sample the four analog gimbal axes on the C5 and send the latest input snapshot beside the video stream.

### Waveshare ESP32-S3 — OpenPocket main processor

The S3 receives already-demodulated composite samples and handles:

- PAL/NTSC sync detection
- active-line extraction
- luma first, color later
- scaling/cropping into the framebuffer
- 40-pin RGB LCD timing/output
- RivetTX control/mixing/safety
- CRSF on GPIO43/44 to the ELRS RX-as-TX module
- telemetry and warnings
- battery/UI services

---

## Exact-board C5 -> S3 video link

The chips do **not** exchange RGB frames. The preferred contract remains sampled CVBS:

```text
C5                         S3
---                        ---
WBFM                       QSPI RX
 |                          |
filter                      DMA ring
 |                          |
8-bit CVBS  ===============> sync / video decode
                             |
                             RGB framebuffer
                             |
                       40-pin RGB LCD
```

The generic idea of "six arbitrary QSPI pins" does not work on this board because the 40-pin LCD consumes most GPIO. The current **candidate runtime mapping** deliberately reuses pins after LCD initialization:

| Signal | S3 GPIO | Note |
|---|---:|---|
| QSPI SCLK | 6 | free in 40-pin RGB mode |
| QSPI CS | 16 | touch INT; touch held reset |
| QSPI IO0 | 1 | reused LCD init SDA |
| QSPI IO1 | 2 | reused LCD init SCK |
| QSPI IO2 | 19 | reused native USB D- |
| QSPI IO3 | 20 | reused native USB D+ |
| CRSF TX/RX | 43 / 44 | ELRS module |
| battery ADC | 4 | onboard divider |

Runtime order:

```text
boot
 -> init TCA9554
 -> hold touch reset
 -> init ST7701/RGB panel on GPIO1/2/42
 -> hold LCD CS high
 -> start RGB engine
 -> disconnect/avoid native USB
 -> remap GPIO1/2/6/16/19/20 to QSPI
 -> start CRSF on GPIO43/44
```

This is clever but **not yet proven at 40 MHz**. The repo treats signal integrity and LCD/touch isolation as explicit bring-up tests rather than assumptions.

Initial transport target:

| Property | Target |
|---|---:|
| sample format | unsigned 8-bit CVBS |
| sample rate | 10 MS/s first, 13.5 MS/s stretch |
| transport | 4-data-line SPI / QSPI |
| C5 role | slave / producer |
| S3 role | master / consumer |
| buffering | DMA ping-pong/ring |
| framing | fixed blocks + protocol header |

At 13.5 MS/s the raw payload is 108 Mbit/s; a 4-bit 40 MHz bus is 160 Mbit/s raw. Real sustained margin must be benchmarked on this exact pin reuse.

See [`docs/video-link.md`](docs/video-link.md).

---

## RivetTX + ExpressLRS

[RivetTX](https://github.com/Twotoz/RivetTX) already supports ESP32-S3 and a full-duplex CRSF link to ExpressLRS firmware running in **TX mode**. A supported ESP8285/ESP32 receiver can be flashed as RX-as-TX and used as the RF module.

On this board:

```text
Waveshare S3 GPIO43 / CRSF TX  ---> converted ELRS RX
Waveshare S3 GPIO44 / CRSF RX  <--- converted ELRS TX
GND                              --- GND
regulated supply                 --- VCC
```

The exact receiver target and its own `hardware.json` define whether 100 mW is actually valid.

Current upstream RivetTX OpenPocket presentation code targets analog AT7456E OSD, so this fork needs a new **digital RGB-LCD backend** for the 40-pin panel.

---

## What hardware disappears?

If C5VRX succeeds, this route needs no conventional analog display chain:

- no RX5808 / RTC6715 VRX
- no AT7456E / MAX7456 OSD
- no composite LCD controller
- no separate framebuffer IC
- no separate main radio MCU beyond the S3
- no separate charger board for the prototype

Prototype hardware becomes roughly:

```text
ESP32-C5 board
Waveshare ESP32-S3-LCD-Driver-Board
40-pin RGB LCD
ELRS receiver flashed RX-as-TX
2 gimbals + switches
1S battery
2.4 GHz + 5.8 GHz antennas
```

---

## Current status

> **Experimental — not yet a working FPV receiver or flight transmitter.**

The make-or-break research item remains continuous C5 RF sampling. C5VRX has identified a finite vendor complex-I/Q dump path, but continuous wide live I/Q is still unproven.

The exact Waveshare S3 side can be developed independently with generated/recorded PAL/NTSC data.

| Subsystem | Status |
|---|---|
| exact Waveshare N8R8 40-pin target | 🟢 locked/documented |
| exact schematic pin map | 🟢 encoded in firmware header/docs |
| finite C5 complex-I/Q capture path | 🟡 hardware validation required |
| continuous C5 I/Q | 🔴 blocker / unproven |
| C5 WBFM -> sampled CVBS | 🟡 architecture/model work |
| exact-board QSPI pin reuse | 🟡 designed; hardware benchmark required |
| S3 grayscale PAL/NTSC decode | 🟡 implementation target |
| S3 color decode | ⚪ later milestone |
| 40-pin RGB LCD output | 🟡 official board support exists; fork driver pending |
| RivetTX on ESP32-S3 | 🟢 upstream target |
| RivetTX digital LCD backend | 🔴 new backend required |
| ELRS RX-as-TX over CRSF | 🟢 upstream architecture; real-HW validation required |

---

## Development plan

### S3 / exact board

1. Bring up the Waveshare 40-pin ST7701 RGB example in ESP-IDF.
2. Reimplement only the needed initialization/timing in this firmware.
3. Prove framebuffer output while RivetTX-style tasks are running.
4. Hold touch reset and prove GPIO16 is safe to reuse.
5. Initialize panel, hold GPIO42 CS high, then reuse GPIO1/2 without disturbing display.
6. Prove GPIO19/20 QSPI use with USB disconnected.
7. Run PRBS/counter transport from 10 MHz upward toward 40 MHz.
8. Feed synthetic CVBS and render grayscale.
9. Add RivetTX digital presentation + CRSF on GPIO43/44.

### C5

1. Validate real 5.8 GHz finite I/Q.
2. Recover continuous sample production.
3. Benchmark WBFM.
4. Produce stable sampled CVBS.
5. Add gimbal ADC snapshot transport.
6. Connect live C5 output to the exact Waveshare board.

See [`docs/bringup.md`](docs/bringup.md), [`docs/hardware.md`](docs/hardware.md), and [`docs/waveshare-40pin-target.md`](docs/waveshare-40pin-target.md).

---

## Repository layout

```text
C5VRX-OpenPocket/
├── firmware/
│   ├── c5/
│   └── s3/
│       └── main/
│           └── board_waveshare_s3_lcd_driver.h
├── protocol/
├── docs/
│   ├── architecture.md
│   ├── video-link.md
│   ├── hardware.md
│   ├── waveshare-40pin-target.md
│   └── bringup.md
└── README.md
```

Upstream projects:

- **C5VRX:** <https://github.com/Twotoz/C5VRX>
- **RivetTX:** <https://github.com/Twotoz/RivetTX>

Research useful to every C5VRX target belongs upstream in C5VRX. Exact Waveshare 40-pin OpenPocket integration belongs here.

---

## Safety / validation boundary

Do not fly from a development build because the LCD and sticks appear responsive. Real hardware still needs measured control deadlines, failsafe behavior, CRSF integrity, RF verification, video-buffer fault handling, thermals and power testing.
