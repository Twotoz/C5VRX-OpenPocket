# RivetTX integration

## Decision: RivetTX is the S3 main firmware

C5VRX-OpenPocket does **not** develop a second radio firmware for the ESP32-S3.

The target architecture is:

```text
ESP32-C5
  5.8 GHz RF / I,Q / WBFM
  sampled CVBS
  optional raw control acquisition
        |
        | high-speed digital link
        v
Waveshare ESP32-S3-LCD-Driver-Board
  RivetTX = main application / radio firmware
  + C5 video-input service
  + PAL/NTSC decoder
  + digital RGB presentation backend
  + exact-board hardware profile
        |
        +--> 40-pin RGB LCD
        |
        +--> CRSF UART --> ELRS RX flashed as TX
```

RivetTX already targets ESP32-S3 and provides the transmitter behavior this project needs: deterministic input/mixer/safety processing, CRSF, ExpressLRS module control, telemetry, model logic, warnings, persistence and related services. C5VRX-OpenPocket should extend that codebase at hardware/presentation boundaries rather than reimplementing those systems.

The standalone `firmware/s3` program in this repository is therefore a **bring-up and transport test harness only**. It is useful for proving the exact Waveshare board, LCD, pin reuse, C5 transport and CVBS decode in isolation. It is not intended to become a parallel OpenPocket operating system.

## Ownership boundary

### RivetTX owns

- calibrated logical inputs
- mixer/channel generation
- arming and throttle safety gates
- control-loop deadlines and stale-input handling
- CRSF TX/RX
- ExpressLRS parameter discovery
- RX-as-TX module support
- telemetry decoding
- model match
- bind/update workflows
- model storage
- warnings/audio policies
- the OpenPocket UI state and menu semantics

### C5VRX-OpenPocket adds around RivetTX

- exact Waveshare ESP32-S3-LCD-Driver-Board hardware profile
- C5 high-speed transport service
- sampled-CVBS receive buffers
- PAL/NTSC sync/line recovery
- luma/chroma decode
- RGB framebuffer/scanout integration
- digital FPV + RivetTX overlay composition
- C5 control-snapshot input adapter if raw gimbal acquisition is placed on the C5
- video-loss/transport diagnostics that do not block the control path

### C5VRX remains upstream owner of

- ESP32-C5 RF reverse engineering
- 5.8 GHz tuning work
- continuous I/Q producer research
- WBFM acceleration
- generic sampled-CVBS generation

This keeps the RF experiment, product integration and transmitter firmware cleanly separated.

## Digital presentation backend

Current RivetTX OpenPocket presentation uses an AT7456E-compatible analog character OSD. This project has no AT7456E in the digital path.

Desired abstraction:

```text
RivetTX UI state
     |
     v
presentation interface
     |
     +--> existing analog OSD backend
     |
     +--> new RGB framebuffer backend
```

The RGB backend should expose bounded drawing primitives rather than coupling RivetTX to composite-video timing:

- clear/dirty region
- text
- icon/glyph
- rectangle/bar
- warning banner
- selection highlight
- optional transparent overlay surface

The CVBS decoder owns video timing. RivetTX owns UI semantics. The presentation backend is the boundary between them.

## Frame composition

The S3 owns the final RGB surface, so analog OSD hardware is unnecessary:

```text
C5 sampled CVBS
      |
      v
PAL/NTSC decoder -----> base RGB pixels
                            |
RivetTX UI -----------> overlay/compositor
                            |
                            v
                    RGB framebuffer
                            |
                            v
                    40-pin RGB LCD
```

Avoid unnecessary full-frame copies. Prefer writing decoded video directly into the display framebuffer/bounce buffer and applying bounded UI dirty regions or line-time overlay composition.

## Input path when gimbals are sampled on the C5

The exact Waveshare 40-pin board has a tight native GPIO budget. If the prototype uses the C5 ADC for the four gimbal axes, the ownership stays split like this:

```text
gimbal voltages
     |
     v
C5 ADC -> raw timestamped snapshot -> C5/S3 protocol
                                      |
                                      v
                              RivetTX input adapter
                                      |
                                      v
                         calibration/filter/mixer/safety
```

The C5 only acquires raw values. **RivetTX must remain responsible for calibration, stale-input detection, filtering, mixing and safety.** A lost or stale C5 control snapshot must be visible to RivetTX as an unhealthy input source, never silently reused forever.

## Control path must be independent of video

A missing or broken FPV feed must not turn into a radio failure.

The S3 design should preserve a short critical path:

```text
input adapter -> RivetTX control/mixer/safety -> CRSF UART -> ELRS
```

while video stays in bounded service tasks:

```text
C5 DMA/QSPI -> bounded ring -> PAL/NTSC decoder -> RGB output
```

The presentation/video side must tolerate:

- no C5 connected
- C5 reset
- no FPV signal
- PAL/NTSC unlock
- dropped or malformed transport blocks
- decoder restart
- display refresh stalls

RivetTX control and CRSF scheduling must continue independently. When possible the UI can show `VIDEO LOST`, but no video task may block the control loop waiting for a frame.

## RX-as-TX module

The external ExpressLRS module remains a separate RF module running ExpressLRS in TX mode.

```text
Waveshare S3 / RivetTX          ELRS receiver flashed TX
----------------------          ------------------------
GPIO43 / CRSF TX  ------------> UART RX
GPIO44 / CRSF RX  <------------ UART TX
GND               ------------- GND
regulated supply  ------------- VCC
```

Use the exact receiver's full-duplex RX-as-TX target and hardware configuration. RivetTX should discover the module's actual CRSF parameter set and power options rather than hard-coding `100 mW`.

## Repository/build strategy

The intended end state is not `firmware/s3` growing into its own full application. Instead:

1. keep `firmware/s3` as the board/video bring-up harness;
2. make the reusable Waveshare board, transport, decoder and RGB-presentation pieces components with clean interfaces;
3. integrate those components into the ESP32-S3 RivetTX target;
4. build the actual OpenPocket transmitter as a RivetTX-based firmware image;
5. keep C5-specific RF code outside RivetTX.

During development, the harness is allowed to fake CVBS or run without RivetTX so each subsystem can be measured independently.

## Suggested implementation order

1. Build upstream RivetTX for ESP32-S3 unchanged and preserve its control/CRSF tests.
2. Prove the exact Waveshare 40-pin LCD and board initialization in the local harness.
3. Add a generic RivetTX `RGB_LCD` presentation backend using a fake framebuffer.
4. Attach that backend to the real Waveshare RGB framebuffer.
5. Add the C5 transport as a non-blocking service.
6. Add grayscale PAL/NTSC decode underneath the same RGB surface.
7. Add the C5 raw-input adapter if gimbals are acquired by the C5.
8. Stress saturated video while measuring RivetTX deadline/stale-input counters.
9. Connect the real RX-as-TX module and validate the complete control path.
10. Add color decode only after control timing, transport margin and display stability are proven.

## Upstream strategy

Generic improvements should be structured so they can move back into RivetTX where appropriate. In particular, a display-agnostic presentation interface and an RGB backend are useful beyond C5VRX-OpenPocket.

Keep C5 transport framing, PAL/NTSC decoder internals and C5 RF behavior out of RivetTX's control core. The goal is **RivetTX with an OpenPocket digital hardware backend**, not RivetTX entangled with one experimental RF implementation.
