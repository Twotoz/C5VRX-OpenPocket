# C5 -> S3 digital video link

The transport carries **sampled composite video**, not RGB pixels and not raw RF I/Q.

```text
C5                                            S3
RF -> I/Q -> WBFM -> filter -> CVBS samples   -> PAL/NTSC -> RGB -> 40-pin LCD
```

The S3 target is specifically the Waveshare ESP32-S3-LCD-Driver-Board with its 40-pin 3SPI+RGB connector. That exact board's GPIO budget determines the transport pinout.

## Payload target

Initial stream format:

| Field | Value |
|---|---|
| sample | unsigned 8-bit |
| initial rate | 10 MS/s |
| stretch target | 13.5 MS/s |
| transport | 4-data-line SPI / QSPI |
| C5 role | slave / producer |
| S3 role | master / consumer |
| buffering | DMA ping-pong/ring buffers |
| compression | none initially |

Bandwidth:

```text
10.0 MS/s * 8 =  80 Mbit/s
13.5 MS/s * 8 = 108 Mbit/s
```

A 4-data-line 40 MHz bus has 160 Mbit/s raw capacity. The real sustained rate must be measured with the chosen C5 GPIO routing and the exact S3 board because the S3 candidate mapping deliberately reuses LCD-init and USB pins.

## Exact S3 candidate pinout

After the 40-pin RGB panel has been initialized:

| QSPI signal | S3 GPIO | Existing board connection |
|---|---:|---|
| SCLK | 6 | unused by 40-pin RGB path |
| CS | 16 | touch INT; touch held in reset |
| IO0 | 1 | LCD serial-init SDA |
| IO1 | 2 | LCD serial-init SCK |
| IO2 | 19 | native USB D- |
| IO3 | 20 | native USB D+ |

CRSF remains on GPIO43/44. Battery sensing remains on GPIO4. The TCA9554 stays on GPIO7/15.

The required boot/runtime sequence is documented in [`waveshare-40pin-target.md`](waveshare-40pin-target.md).

### Why GPIO1/2 can potentially be reused

The ST7701-style RGB panel only needs its 3-wire serial interface for initialization/configuration; continuous pixels use the parallel RGB bus. The S3 initializes the panel using GPIO1/2 with LCD CS on GPIO42, then keeps GPIO42 high before remapping GPIO1/2 into the C5 transport.

The panel still physically sees activity on those two wires, so this is only safe if it truly ignores them while CS is inactive. That is a hardware-validation item.

### Why USB cannot stay connected

GPIO19/20 are the S3's native USB D-/D+ on this board. They become two QSPI data lines in the proposed runtime mapping. Therefore native USB is a **bring-up/flash interface**, not an interface that can remain active while full-QSPI video is running.

The USB-C connector and routing remain attached electrically, so the resulting stubs are part of the signal-integrity test.

## No READY wire on the exact board

The generic design originally reserved a separate producer-ready GPIO. The exact Waveshare 40-pin target does not have a comfortable spare direct pin after RGB, QSPI, CRSF and battery functions are allocated.

The prototype therefore uses **polling / fixed-cadence master reads** instead:

```text
C5 slave:
    keep next DMA block queued
    update sequence/discontinuity counters

S3 master:
    issue the next fixed-size read on schedule
    validate the returned block header
```

If the producer has discontinuous data, the block header says so. No unbounded queue is allowed.

## Block transport

Recommended first block size:

```text
4096 video samples = 4096 payload bytes
```

At 13.5 MS/s one block spans about 303 us. At 10 MS/s it spans about 410 us.

### Header v0

Each transfer begins with a compact fixed header followed by raw samples:

```text
byte 0      0xC5
byte 1      0x56                 # 'V'
byte 2      protocol version
byte 3      flags
byte 4..7   sequence             # uint32 little-endian
byte 8..11  sample_rate_hz       # uint32 little-endian
byte 12..13 payload_bytes        # uint16 little-endian
byte 14     video_standard_hint  # 0 unknown, 1 PAL-like, 2 NTSC-like
byte 15     reserved
byte 16..   sample payload
```

Header size: 16 bytes.

Initial flags:

```text
bit 0  producer overrun occurred since previous block
bit 1  RF/sample source discontinuity
bit 2  sample levels calibrated/normalized
bit 3  test-pattern source instead of RF
bit 4  C5 reports RF source present
bit 5..7 reserved
```

A sequence gap or discontinuity flag forces the S3 video decoder to reacquire sync.

## Control-input sideband

Because the exact S3 board has almost no ADC pin budget left, the preferred prototype samples the four gimbal axes on the C5. Those values need only hundreds of updates per second, so they can be carried with negligible overhead.

A later protocol revision may add a small control snapshot after the fixed video header or in a periodic status block:

```text
4 x gimbal ADC values
switch bitfield
input sequence/timestamp
```

The S3/RivetTX side remains authoritative for calibration, mixing, arming logic, failsafe state and CRSF output.

## Buffering

### C5

Use at least ping-pong output buffers and prequeue the next SPI-slave DMA transaction.

### S3

Use DMA-backed receives feeding a bounded decoder ring. The decoder consumes samples incrementally; it should not copy every transport block into a second full-frame staging buffer.

## Backpressure policy

Live analog video cannot pause. If the S3 falls behind, dropping a bounded block and reacquiring sync is preferable to increasing latency forever.

- C5 counts producer drops/overruns.
- S3 discards malformed or late blocks.
- sequence gaps reset video timing state when necessary.
- diagnostics expose every discontinuity during development.

## Bring-up ladder

1. Counter pattern.
2. PRBS/test pattern.
3. Generated composite sync/bars.
4. Recorded CVBS replay.
5. Host-generated C5VRX WBFM output.
6. Live C5 RF output.

For the exact Waveshare board, repeat steps 1 and 2 while increasing SPI clock because this is also the pin-reuse/signal-integrity qualification.

## Acceptance targets

Before calling the link usable:

- >= 10 minutes sustained operation without DMA deadlock
- zero unexplained byte corruption in PRBS mode
- payload throughput >= configured video rate with at least 15% measured margin
- RGB LCD remains stable while GPIO1/2 are reused
- no false ST7701 commands with LCD CS held high
- no contention from the disabled touch controller on GPIO16
- USB-C disconnected during runtime transport
- bounded decoder queue and explicit drop counters
- RivetTX control/CRSF deadlines remain healthy under maximum video load
- clean decoder reacquisition after forced C5 reset/discontinuity

If 13.5 MS/s cannot meet these conditions, reduce/pack the CVBS stream or redesign the final carrier rather than pretending the exact development board has unlimited GPIO margin.
