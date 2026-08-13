# Architecture

C5VRX-OpenPocket deliberately separates the RF/video problem from the handheld/product problem.

## System boundary

```text
                                     OPENPOCKET HANDSET

 analog FPV VTX
      5.8 GHz
         |
         v
 +-------------------+
 |     ESP32-C5      |
 |                   |
 | 5 GHz RF front end|
 | complex I/Q       |
 | WBFM demod        |
 | filtering         |
 | 8-bit CVBS stream |
 | optional raw ADC  |
 +---------+---------+
           |
           | high-speed digital sampled video
           | + optional timestamped input snapshots
           | QSPI / DMA
           v
 +------------------------------------+
 | Waveshare ESP32-S3-LCD-Driver     |
 | Board / exact 40-pin RGB target   |
 |                                    |
 | transport RX                       |
 | PAL/NTSC recovery                  |
 | RGB framebuffer                    |
 | LCD engine                         |
 |                                    |
 | RivetTX = main radio firmware      |
 | control / mixer / safety           |
 | CRSF / telemetry / UI              |
 +---+----------------------------+---+
     |                            |
     |                            +------------------> 40-pin RGB LCD
     |
     +------ CRSF UART ------+
                             v
                     +---------------+
                     | ELRS RX-as-TX |
                     | 2.4 GHz RF    |
                     +-------+-------+
                             |
                             v
                          aircraft
```

## Firmware ownership

The S3 does **not** run a second C5VRX-OpenPocket radio firmware beside RivetTX. The intended product firmware is RivetTX for ESP32-S3 with OpenPocket-specific hardware services around it.

The local `firmware/s3` application is a bring-up harness for board, display, transport and decoder work. It exists so those subsystems can be tested without putting experimental video code directly into the flight-control application during early development.

The end-state firmware boundary is:

```text
RivetTX control core
    + Waveshare board profile
    + digital RGB presentation backend
    + C5 transport service
    + PAL/NTSC video service
    + optional C5 raw-input adapter
```

See [`rivettx-integration.md`](rivettx-integration.md).

## Design rule: each processor gets one difficult timing domain

### C5 owns RF/sample timing

The C5 side stays focused on operations directly coupled to RF/sample timing:

- frequency/tuner bring-up
- sample capture/streaming
- I/Q unpacking
- WBFM discrimination
- filtering/decimation
- output sample timing
- RF-side diagnostics
- optional **raw** ADC acquisition when the exact S3 board has insufficient free ADC-capable GPIO

If raw gimbal acquisition is placed on the C5, it does not move radio semantics to the C5. The C5 only produces timestamped raw input snapshots. RivetTX on the S3 remains responsible for calibration, filtering, stale-input detection, mixing, arming and safety.

The C5 should not own menus, model logic, channel mixing, failsafe decisions, framebuffers or LCD graphics.

### S3 owns product/control timing

The S3 owns everything that turns recovered video and raw controls into a usable handheld:

- RivetTX as the main application
- digital video transport receive
- PAL/NTSC sync and line recovery
- luma/chroma decoding
- LCD framebuffer and scan-out
- input adaptation/calibration
- mixer and safety gates
- CRSF link to ExpressLRS
- telemetry
- warnings and menus
- battery/UI services

This keeps the flight-control path alive even if the video decoder loses sync or the C5 stops producing video samples.

## Failure isolation

The video path must never be allowed to block the control path.

On the S3:

```text
C5 video DMA IRQ/task
       |
       +----> bounded queue / ring
                    |
                    +----> decoder task

raw input adapter ----+
                      v
RivetTX control task ------------------> independent deadlines
CRSF UART task ------------------------> independent deadlines
```

Expected behavior on video failure:

1. QSPI video stalls, underruns, becomes malformed or loses PAL/NTSC sync.
2. Video task marks the feed unavailable and stops consuming stale pixels.
3. UI shows a video fault/no-signal state when possible.
4. RivetTX controls and CRSF continue to run.
5. RF-control output is locked only by RivetTX's control/safety health conditions, not because the display decoder crashed.

If controls are sourced through C5 raw snapshots, stale or missing snapshots are a separate input-health event. RivetTX must detect that condition and apply its normal safe-input behavior rather than silently reusing old stick values.

## Core split on ESP32-S3

A starting task allocation is:

```text
core 0
  - C5 transport service
  - PAL/NTSC decode
  - display service
  - general UI work

core 1
  - RivetTX control task
  - input deadlines
  - CRSF critical work
```

This is an initial policy, not a permanent ABI. DMA should move bulk video data between peripherals and memory wherever possible.

## Memory model

The exact Waveshare development target has 8 MB PSRAM, so a full RGB565 framebuffer is practical.

For 800 x 480:

```text
800 * 480 * 2 bytes = 768,000 bytes
```

A double buffer is about 1.54 MB before alignment/metadata. The official drop-in Waveshare RGB panels are currently 480x480, but the memory model leaves room for a future compatible 800x480/480x800 panel or adapter.

The decoder should still avoid full-frame intermediate CVBS buffers. Composite samples are consumed line-by-line from a DMA ring and written into the output surface.

## Latency target

The architecture should avoid unnecessary frame queues.

Preferred flow:

```text
C5 DMA block
   -> S3 DMA ring
      -> line sync
         -> decode active line
            -> write output line
               -> apply RivetTX overlay
                  -> LCD scans RGB surface
```

One full extra video frame of buffering should not be added unless required for stability or scaling quality.

## RivetTX integration boundary

RivetTX already provides the radio-control core for ESP32-S3: deterministic input/mixer/safety behavior, CRSF, ExpressLRS module discovery/control, telemetry, model logic and UI state.

C5VRX-OpenPocket adds a digital product backend rather than cloning those systems.

Conceptually:

```text
                         +---------------------------+
raw controls ---------->| RivetTX input/control core|
                         +-------------+-------------+
                                       |
                                       | logical UI state
                                       v
                              digital presentation
                                       |
                                       +----> text/icons/warnings
                                       |
CVBS decoder --------------------------+----> RGB composition -> LCD
```

The video decoder should not know RivetTX menu semantics, and the RivetTX control core should not know PAL line timing or C5 RF implementation details.

## Repository split

Changes belong in **C5VRX** when they improve the generic C5 receiver, for example:

- RF dump reverse engineering
- arbitrary frequency tuning
- continuous I/Q producer work
- WBFM acceleration
- generic sampled-CVBS generation

Changes belong in **RivetTX** when they improve the generic transmitter firmware, for example:

- display/presentation abstractions
- reusable RGB framebuffer presentation backend
- generic external/raw input adapter interfaces
- control/safety/CRSF/telemetry behavior

Changes belong in **C5VRX-OpenPocket** when they are integration specific, for example:

- exact Waveshare 40-pin board profile
- C5<->S3 wire protocol
- S3 sampled-CVBS decoder
- exact-board LCD/pin-reuse bring-up
- C5 transport service
- OpenPocket-specific composition/wiring
- dual-radio coexistence tests

The goal is a clean product integration: **C5VRX provides the experimental 5.8 GHz video source; RivetTX remains the transmitter firmware; C5VRX-OpenPocket connects them on the exact hardware.**
