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
 +---------+---------+
           |
           | high-speed digital sampled video
           | QSPI / DMA
           v
 +-------------------+
 |     ESP32-S3      |
 |                   |
 | transport RX      |
 | PAL/NTSC recovery |
 | RGB framebuffer   |
 | LCD engine        |
 | RivetTX           |
 | controls / safety |
 | CRSF / telemetry  |
 +---+------------+--+
     |            |
     |            +------------------> RGB LCD
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

## Design rule: each processor gets one difficult job

### C5 owns RF timing

The C5 side must remain focused on operations that are directly coupled to RF sample timing:

- frequency/tuner bring-up
- sample capture/streaming
- I/Q unpacking
- WBFM discrimination
- filtering/decimation
- output sample timing
- RF-side diagnostics

It should not own menus, stick processing, framebuffers or LCD graphics.

### S3 owns product timing

The S3 owns everything that turns the recovered video into a usable handheld:

- digital video transport receive
- PAL/NTSC sync and line recovery
- luma/chroma decoding
- LCD framebuffer and scan-out
- RivetTX control loop
- input sampling
- CRSF link to ExpressLRS
- telemetry
- warnings and menus
- battery/UI services

This keeps the flight-control path alive even if the video decoder loses sync or the C5 stops producing samples.

## Failure isolation

The video path must never be allowed to block the control path.

On the S3:

```text
video DMA IRQ/task
       |
       +----> bounded queue / ring
                    |
                    +----> decoder task

RivetTX control task ------------------> independent deadlines
CRSF UART task ------------------------> independent deadlines
```

Expected behavior on video failure:

1. QSPI stream stalls, underruns, becomes malformed or loses PAL/NTSC sync.
2. Video task marks the feed unavailable and stops consuming stale pixels.
3. UI shows a video fault/no-signal state.
4. RivetTX controls and CRSF continue to run.
5. RF-control output is locked only by RivetTX's own safety conditions, not by a display decoder crash.

## Core split on ESP32-S3

A starting task allocation is:

```text
core 0
  - transport service
  - PAL/NTSC decode
  - display service
  - general UI work

core 1
  - RivetTX control task
  - input deadlines
  - CRSF critical work
```

This is an initial policy, not a permanent ABI. DMA should move bulk data between peripherals and memory wherever possible.

## Memory model

The S3 development target has PSRAM, so a full RGB565 framebuffer is acceptable.

For 800 x 480:

```text
800 * 480 * 2 bytes = 768,000 bytes
```

A double buffer is about 1.54 MB before alignment/metadata, leaving substantial room on an 8 MB PSRAM target.

The decoder should still avoid full-frame intermediate CVBS buffers. Composite samples are consumed line-by-line from a DMA ring and written into the output framebuffer.

## Latency target

The architecture should avoid unnecessary frame queues.

Preferred flow:

```text
C5 DMA block
   -> S3 DMA ring
      -> line sync
         -> decode active line
            -> write output line
               -> LCD scans framebuffer
```

One full extra video frame of buffering should not be added unless required for stability or scaling quality.

## RivetTX integration boundary

Current RivetTX already contains ESP32-S3 support, CRSF, ELRS module discovery, control logic and safety machinery. Its existing OpenPocket presentation path is analog-OSD based, so C5VRX-OpenPocket needs a new presentation adapter.

The desired interface is conceptually:

```text
RivetTX logical UI state
        |
        v
OpenPocket digital presentation backend
        |
        +--> draw text/icons/warnings into S3 RGB surface
        |
        +--> optionally expose transparent overlay primitives
```

The video decoder should not know RivetTX menu semantics, and RivetTX should not know PAL line timing.

## Upstream split

Changes belong in **C5VRX** when they improve the generic C5 receiver, for example:

- RF dump reverse engineering
- arbitrary frequency tuning
- continuous I/Q producer work
- WBFM acceleration
- generic sampled-CVBS generation

Changes belong in **C5VRX-OpenPocket** when they are product/integration specific, for example:

- C5↔S3 wire protocol
- S3 sampled-CVBS decoder
- Waveshare-style LCD-board bring-up
- digital RivetTX presentation
- OpenPocket pin map
- dual-radio coexistence tests
