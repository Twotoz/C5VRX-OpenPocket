# C5 -> S3 digital video link

The transport carries **sampled composite video**, not RGB pixels and not raw RF I/Q.

That boundary is important:

```text
C5                                            S3
RF -> I/Q -> WBFM -> filter -> CVBS samples   -> PAL/NTSC -> RGB -> LCD
```

Sending raw I/Q would move the hardest RF DSP onto the S3 and require much more bandwidth. Sending RGB would force the C5 to decode video and build frames. Sampled CVBS is the useful middle point.

## Payload target

Initial stream format:

| Field | Value |
|---|---|
| sample | unsigned 8-bit |
| nominal black/sync/white mapping | calibrated by C5 producer; exact levels carried in metadata if needed |
| initial rate | 10 MS/s |
| stretch target | 13.5 MS/s |
| byte order | one temporal sample per byte |
| compression | none |

Bandwidth:

```text
10.0 MS/s * 8 =  80 Mbit/s
13.5 MS/s * 8 = 108 Mbit/s
```

A 4-data-line 40 MHz bus has 160 Mbit/s raw capacity. That is enough on paper for 13.5 MS/s but the useful sustained rate must be measured with the actual ESP32-C5 slave, ESP32-S3 master, GPIO routing, DMA descriptors and transaction overhead.

**Do not lock 13.5 MS/s into the product until the sustained-throughput test passes with margin.** PAL/NTSC reconstruction can start at 10 MS/s and move upward after the transport and decoder are stable.

## Electrical bus

Preferred development bus:

```text
S3 (master)                         C5 (slave)
-----------                         ----------
SCLK       -----------------------> SCLK
CS         -----------------------> CS
IO0        <----------------------> IO0
IO1        <----------------------> IO1
IO2        <----------------------> IO2
IO3        <----------------------> IO3
READY      <----------------------- READY / block queued
GND        ------------------------ GND
```

The video phase is overwhelmingly C5 -> S3. Bidirectional IO lines are retained so the same physical bus can support short control/status transactions without another high-speed serial interface.

The READY line is recommended for prototype bring-up because an SPI slave cannot create clock edges. It tells the S3 that the C5 has a DMA block ready to be clocked out.

## Block transport

The stream is continuous logically, but moved as bounded DMA blocks physically.

Recommended first block size:

```text
4096 video samples = 4096 payload bytes
```

At 13.5 MS/s one block spans about 303 us. At 10 MS/s it spans about 410 us.

Large enough blocks reduce per-transaction overhead while remaining short enough to keep FIFO/latency diagnostics useful.

### Header v0

Each transfer begins with a compact fixed header followed by raw samples:

```text
byte 0      0xC5
byte 1      0x56                 # 'V'
byte 2      protocol version     # 0 initially
byte 3      flags
byte 4..7   sequence             # uint32 little-endian
byte 8..11  sample_rate_hz       # uint32 little-endian
byte 12..13 payload_bytes        # uint16 little-endian
byte 14     video_standard_hint  # 0 unknown, 1 PAL-like, 2 NTSC-like
byte 15     reserved
byte 16..   sample payload
```

Header size: 16 bytes.

At a 4096-byte payload, protocol overhead is under 0.4% before bus-level transaction overhead.

### Flags

Initial flags:

```text
bit 0  producer overrun occurred since previous block
bit 1  RF/sample source discontinuity
bit 2  sample levels calibrated/normalized
bit 3  test-pattern source instead of RF
bit 4  C5 reports RF source present
bit 5  reserved
bit 6  reserved
bit 7  reserved
```

The S3 must treat a sequence gap or discontinuity flag as a decoder resynchronization event. It must never silently splice unrelated sample time together.

## Buffering

### C5

Use at least ping-pong output buffers:

```text
RF/DSP fills A  ----+
                    +--> SPI slave DMA queues A
RF/DSP fills B  ----+
                    +--> SPI slave DMA queues B
```

A deeper ring may be required once real continuous RF production exists, but buffering should not become a way to hide an unsustainable producer/transport mismatch.

### S3

The receive path should be DMA-backed and feed a bounded software ring:

```text
QSPI DMA
   |
   v
RX block queue
   |
   +--> validate header / sequence
   |
   +--> stream samples into sync/line decoder
```

The video decoder should consume bytes incrementally. It should not copy every block into another full-frame staging area.

## Backpressure policy

Live analog video cannot be paused at the antenna. If S3 falls behind, preserving old samples is usually worse than dropping data and reacquiring sync.

Therefore:

- C5 increments an overrun/discontinuity counter when it must drop source data.
- S3 discards malformed/late blocks rather than building an unbounded queue.
- sequence gaps reset the PAL/NTSC timing state as needed.
- diagnostics expose the event visibly during development.

## Control/status transactions

Low-rate commands may use short bus transactions between video reads. Examples:

S3 -> C5:

- select channel/frequency
- start/stop producer
- choose sample-rate mode
- select RF vs generated test pattern
- request statistics reset

C5 -> S3 status:

- current center frequency
- sample-rate actual/estimated
- producer overrun count
- transport block count
- RF/sample-source state
- C5 build/protocol version

Frequency control should not be mixed into the time-critical sample payload itself.

## Bring-up ladder

Do not begin with live RF.

1. **Counter pattern** — C5 emits incrementing bytes. Proves bit ordering and sustained transport.
2. **PRBS/test pattern** — detects corruption that a simple counter can miss.
3. **Generated composite** — C5 emits synthetic sync bars/gray ramp at the exact sample rate.
4. **Recorded CVBS replay** — validates S3 decoder behavior on realistic timing/noise.
5. **C5VRX host-generated WBFM output** — verifies sample level conventions.
6. **Live C5 RF output** — only after continuous RF production exists.

## Acceptance targets

Before calling the transport usable for OpenPocket:

- sustained run >= 10 minutes without DMA deadlock
- zero unexplained byte corruption in generated-pattern mode
- measured payload rate above configured CVBS rate with >= 15% margin
- bounded queue depth
- explicit counter for every dropped block
- control/RivetTX deadlines remain healthy while video is saturated
- S3 cleanly reacquires video after forced C5 reset or sequence discontinuity

If 13.5 MS/s cannot meet those conditions, reduce the CVBS sample rate or evaluate the C5 SDIO-slave route rather than relying on fragile timing.
