<div align="center">

# C5VRX-OpenPocket

**Two-ESP32 digital analog-FPV handheld experiment**

An OpenPocket-focused integration of **C5VRX** and **RivetTX**: use an ESP32-C5 as the experimental 5.8 GHz analog FPV receiver/demodulator and an ESP32-S3 as the display, radio-control and UI processor.

![status](https://img.shields.io/badge/status-architecture%20%2F%20bring--up-blue)
![RF](https://img.shields.io/badge/video-5.8%20GHz%20analog-orange)
![C5](https://img.shields.io/badge/RF-ESP32--C5-6f42c1)
![S3](https://img.shields.io/badge/UI-ESP32--S3-00b894)
![display](https://img.shields.io/badge/display-RGB%20LCD-111111)

</div>

---

## What is this?

[C5VRX](https://github.com/Twotoz/C5VRX) asks whether the ESP32-C5's 5 GHz Wi-Fi RF chain can be pushed far enough outside its intended job to receive analog 5.8 GHz FPV video.

This repository asks the next question:

> **If C5VRX can continuously recover composite video, can an ESP32-S3 turn that stream into a complete OpenPocket transmitter with a digital LCD, RivetTX controls and a converted ExpressLRS receiver used as the TX module?**

The target is intentionally tiny in hardware count.

```text
                 5.8 GHz analog FPV
                         |
                         v
                 +---------------+
                 |   ESP32-C5    |
                 | RF / I,Q      |
                 | WBFM demod    |
                 | CVBS samples  |
                 +-------+-------+
                         |
                  digital video link
                    QSPI / GDMA
                         |
                         v
                 +---------------+
  gimbals ------>|               |------> RGB LCD
  switches ----->|   ESP32-S3    |
  battery ------>|               |------> buzzer / UI
                 | CVBS decode   |
                 | RivetTX       |
                 +-------+-------+
                         |
                    CRSF UART
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

The **C5 and S3 are the two main processors**. The converted ELRS receiver is treated as a small external RF module, not as the handset's main controller.

---

## Why split C5 and S3?

The two chips have very different jobs.

### ESP32-C5 — RF/video front end

The C5 side stays close to the original C5VRX architecture:

```text
5.8 GHz RF
   -> complex I/Q
   -> WBFM discriminator
   -> filtering / decimation
   -> sampled composite video
```

The C5 should **not** spend its remaining compute on a framebuffer, menu system or RGB LCD.

### ESP32-S3 — OpenPocket main processor

The S3 receives already-demodulated composite samples and handles the product-level work:

- PAL/NTSC sync detection
- active-line extraction
- luma decode first, color later
- scaling/cropping into the LCD framebuffer
- RGB LCD timing/output
- RivetTX control loop
- gimbal and switch inputs
- CRSF UART to an ELRS RX-as-TX module
- telemetry and warnings
- battery/UI services

A Waveshare-style **ESP32-S3 LCD Driver Board with PSRAM and 1S battery charging** is the initial development target because it removes a large amount of support hardware during bring-up.

---

## Digital video link

The chips do **not** exchange RGB frames. That would waste bandwidth and duplicate work.

The preferred contract is a continuous 8-bit composite sample stream:

```text
C5                         S3
---                        ---
WBFM                       QSPI RX
 |                          |
filter                      ring buffer
 |                          |
8-bit CVBS  ===============> sync / video decode
                             |
                             RGB framebuffer
                             |
                             LCD
```

Initial target:

| Property | Target |
|---|---:|
| sample format | unsigned 8-bit CVBS |
| sample rate | 10–13.5 MS/s, benchmark first |
| transport | 4-data-line SPI / QSPI |
| C5 role | slave / producer |
| S3 role | master / consumer |
| buffering | DMA-backed ring / ping-pong buffers |
| framing | continuous stream + small block header/control channel |

At 13.5 MS/s the payload is **108 Mbit/s**. A 4-bit 40 MHz link has **160 Mbit/s raw line capacity**, leaving a useful but not enormous implementation margin. The first hardware test therefore benchmarks sustained DMA throughput before locking the final sample rate.

See [`docs/video-link.md`](docs/video-link.md).

---

## RivetTX + ExpressLRS

[RivetTX](https://github.com/Twotoz/RivetTX) already supports ESP32-S3 and a full-duplex CRSF link to ExpressLRS firmware running in **TX mode**. A supported ESP8285/ESP32 receiver can be deliberately flashed as an RX-as-TX module and used as the radio module.

```text
ESP32-S3 / RivetTX       converted ELRS module
CRSF TX  -----------------> RX
CRSF RX  <----------------- TX
GND      ------------------ GND
supply   ------------------ VCC
```

A receiver with a genuine PA may expose 50 mW, 100 mW or another power level through ExpressLRS. The exact board's firmware target and `hardware.json` remain authoritative; this project must not assume that every receiver can safely produce 100 mW.

**Important:** current RivetTX OpenPocket presentation code targets an analog AT7456E OSD. This fork therefore needs a new **digital RGB-LCD presentation backend** instead of pretending the existing analog OSD backend already drives this screen.

---

## What hardware disappears?

If the C5 experiment succeeds, the digital OpenPocket path no longer needs the conventional analog video chain:

- no RX5808 / RTC6715 VRX module
- no AT7456E / MAX7456 OSD chip
- no composite LCD controller board
- no separate framebuffer IC
- no separate main radio MCU beyond the S3
- no separate charger board when using the development S3-LCD board's battery-management hardware

The prototype still needs the LCD, controls, battery, antennas, the converted ELRS RF module and normal power/RF support components.

---

## Current status

> **This is experimental. It is not yet a working FPV receiver or flight transmitter.**

The architecture after a valid digital CVBS stream is conventional embedded engineering. The make-or-break research item remains upstream in C5VRX:

**Can the ESP32-C5 produce a continuous, sufficiently wide live I/Q stream from a 5.8 GHz analog FPV transmission?**

C5VRX has identified a finite vendor complex-I/Q dump path, but continuous RF sample production is not yet proven. Until that is solved, the S3 side can be developed independently with synthetic and recorded PAL/NTSC composite samples.

### Status matrix

| Subsystem | Status |
|---|---|
| finite C5 complex-I/Q capture path | 🟡 statically identified; hardware validation required |
| continuous C5 I/Q | 🔴 blocker / unproven |
| C5 WBFM -> sampled CVBS | 🟡 host-modelled architecture |
| C5 -> S3 digital transport | 🟡 architecture defined; benchmark required |
| S3 grayscale PAL/NTSC decode | 🟡 implementation target |
| S3 color decode | ⚪ later milestone |
| S3 RGB LCD output | 🟢 supported hardware class; board-specific bring-up required |
| RivetTX on ESP32-S3 | 🟢 existing upstream target |
| RivetTX digital LCD presentation | 🔴 new backend required |
| ELRS RX-as-TX over CRSF | 🟢 supported upstream architecture; real-HW validation required |

---

## Development plan

### Phase 0 — develop both halves independently

**C5 side**
1. Validate real 5.8 GHz finite I/Q capture.
2. Recover a continuous producer/ring path.
3. Benchmark hardware-assisted WBFM.
4. Produce stable 8-bit CVBS samples.

**S3 side**
1. Bring up the target RGB LCD.
2. Feed synthetic/recorded CVBS samples into the decoder.
3. Lock PAL/NTSC sync and render grayscale.
4. Add a DMA QSPI receiver.
5. Add a digital-LCD RivetTX presentation backend.
6. Run the RivetTX control loop and CRSF simultaneously with video.

### Phase 1 — connect the processors

1. Benchmark sustained QSPI/DMA throughput.
2. Add FIFO level / overrun / underrun counters.
3. Stream a known generated composite signal from C5 to S3.
4. Replace it with real C5VRX output.
5. Measure glass-to-glass latency and frame stability.

### Phase 2 — complete OpenPocket

1. Integrate gimbals and switches.
2. Connect the converted ELRS RX-as-TX module.
3. Add telemetry and safety warnings over the digital video UI.
4. Add color decode if the CPU/DMA budget allows it cleanly.
5. Characterize battery life, thermals and 2.4/5.8 GHz RF coexistence.

See [`docs/bringup.md`](docs/bringup.md) and [`docs/architecture.md`](docs/architecture.md).

---

## Repository layout

```text
C5VRX-OpenPocket/
├── firmware/
│   ├── c5/                 # C5 transport/integration work
│   └── s3/                 # S3 video/display/RivetTX integration work
├── docs/
│   ├── architecture.md
│   ├── video-link.md
│   ├── hardware.md
│   └── bringup.md
└── README.md
```

This repository intentionally references rather than silently replacing the upstream projects:

- **C5VRX:** <https://github.com/Twotoz/C5VRX>
- **RivetTX:** <https://github.com/Twotoz/RivetTX>

Research discoveries that are useful to every C5VRX target belong upstream in C5VRX. Product-specific C5↔S3 transport, LCD decoding and OpenPocket integration belong here.

---

## Safety / validation boundary

Do not fly from a development build simply because the UI appears responsive. A valid OpenPocket transmitter requires measured control-loop deadlines, failsafe behavior, CRSF integrity, RF output verification, video-buffer fault handling, thermal testing and power-supply testing on the real hardware.

Keep propellers removed during bench bring-up and start converted ELRS modules at their lowest useful RF power.
