# RivetTX integration

C5VRX-OpenPocket should reuse RivetTX's control, safety, CRSF, telemetry and model logic rather than creating a second radio firmware.

The integration work is mostly a **new presentation/input hardware profile for the ESP32-S3 digital-LCD product**.

## Keep upstream control semantics

Reuse from RivetTX:

- calibrated analog inputs
- mixer/channel generation
- arming and throttle safety gates
- 250 Hz control scheduling
- CRSF TX/RX
- ExpressLRS parameter discovery
- RX-as-TX module support
- telemetry decoding
- model match
- bind/update workflows
- warnings and audio policies
- storage/OTA where applicable

Do not fork these systems merely to draw them on a different screen.

## Replace the analog OpenPocket presentation backend

Current RivetTX OpenPocket presentation uses an AT7456E-compatible analog character OSD. This project has no AT7456E in the digital path.

Desired abstraction:

```text
RivetTX UI state
     |
     v
presentation interface
     |
     +--> existing analog OSD backend      (upstream OpenPocket)
     |
     +--> new RGB framebuffer backend      (C5VRX-OpenPocket)
```

The RGB backend should expose simple primitives rather than making RivetTX depend on the composite decoder:

- clear region
- text
- icon/glyph
- rectangle/bar
- warning banner
- selection highlight
- optional transparent overlay layer

## Overlay model

The S3 receives FPV video as pixels and owns the final framebuffer. Therefore RivetTX UI can be composited digitally:

```text
CVBS decoder -> base RGB line/frame
                         |
RivetTX UI  -> overlay --+
                         |
                         v
                       LCD
```

This removes the need for an analog OSD chip while preserving the same conceptual UI.

For minimum latency, full-screen copies should be avoided. Prefer drawing overlays into the destination buffer or maintaining a compact dirty-region overlay that is composited as output lines are written.

## Video failure must not become radio failure

The presentation backend must tolerate:

- no C5 connected
- C5 resetting
- no FPV signal
- PAL/NTSC unlock
- dropped QSPI blocks
- decoder task restart

RivetTX's control task and CRSF link must continue independently.

A video fault should produce a UI state such as `VIDEO LOST` when possible, but it must never block the control scheduler waiting for a frame.

## RX-as-TX module

The external ExpressLRS module remains a separate processor/radio module running ExpressLRS in TX mode.

```text
S3 / RivetTX                   ELRS receiver flashed TX
------------                   ------------------------
CRSF TX  --------------------> UART RX
CRSF RX  <-------------------- UART TX
GND      --------------------- GND
power    --------------------- VCC
```

Use the exact board's full-duplex RX-as-TX target and hardware configuration. RivetTX should discover the module's actual CRSF parameter set and power options rather than hard-coding `100 mW`.

## Suggested implementation order

1. Build RivetTX for ESP32-S3 unchanged and prove the core/control path.
2. Add a compile-time `RGB_LCD` presentation profile with a fake framebuffer.
3. Port menu/status rendering to generic primitives.
4. Attach those primitives to the real LCD framebuffer.
5. Add the sampled-CVBS decoder underneath the same buffer.
6. Stress video while measuring the RivetTX deadline counters.
7. Connect the real RX-as-TX module only after scheduler stability is proven.

## Upstream strategy

If the RGB presentation abstraction is generally useful, structure it so the generic interface can eventually be proposed upstream to RivetTX. Keep C5-specific transport and PAL/NTSC decoding out of RivetTX itself.
