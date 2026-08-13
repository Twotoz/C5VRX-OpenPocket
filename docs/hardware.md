# Development hardware

This document defines the **bring-up hardware**, not the final production PCB.

## Exact main processor / LCD board

The prototype target is specifically the **Waveshare ESP32-S3-LCD-Driver-Board (SKU 27686) with its onboard 40-pin 3SPI + RGB connector** — the same board shown in the product photo used for this project.

It is not a generic "Waveshare-style" S3 board.

Official board documentation:

- <https://www.waveshare.com/wiki/ESP32-S3-LCD-Driver-Board>
- <https://docs.waveshare.com/ESP32-S3-LCD-Driver-Board>
- <https://files.waveshare.com/wiki/ESP32-S3-LCD-Driver-Board/ESP32-S3-LCD-Driver-Board.pdf>

Relevant onboard hardware:

- ESP32-S3-WROOM-1-N8R8
- dual-core LX7 up to 240 MHz
- 8 MB PSRAM
- 8 MB flash
- USB Type-C
- **40-pin 3SPI + RGB LCD connector**
- 18-pin SPI LCD connector, unused by the first OpenPocket RGB prototype
- ETA6096 lithium battery charge/discharge manager
- MX1.25 2-pin connector intended for a 3.7 V lithium battery
- TCA9554 GPIO expander
- AP3032KTR LCD backlight boost converter
- ME6217C33M5G 3.3 V regulator

The official Waveshare RGB examples currently target 480x480 ST7701 panels. The S3 architecture is not inherently limited to 480x480, but an arbitrary 800x480/480x800 panel must not be assumed to match this 40-pin connector. Check the exact panel pinout, rails, backlight and timings first.

See [`waveshare-40pin-target.md`](waveshare-40pin-target.md) for the exact fixed GPIO map and the proposed C5 transport reuse scheme.

## GPIO reality on this exact board

The 40-pin RGB bus consumes most native S3 GPIO. This changes the architecture materially: we cannot allocate a generic six-wire QSPI bus, four S3 ADC gimbal inputs and CRSF without deliberate pin reuse.

The current candidate full-QSPI mapping is:

| Function | S3 GPIO | Constraint |
|---|---:|---|
| C5 SCLK | 6 | unused by 40-pin RGB path |
| C5 CS | 16 | touch INT; touch must be held reset |
| C5 IO0 | 1 | reused after ST7701 serial init |
| C5 IO1 | 2 | reused after ST7701 serial init |
| C5 IO2 | 19 | native USB D-; USB disconnected in run mode |
| C5 IO3 | 20 | native USB D+; USB disconnected in run mode |
| CRSF TX | 43 | dedicated UART header |
| CRSF RX | 44 | dedicated UART header |
| battery ADC | 4 | existing board divider |

LCD serial init CS remains GPIO42 and is held high after initialization so the panel ignores subsequent traffic on GPIO1/2.

This pin plan is intentionally provisional until the real board passes signal-integrity and sustained-transfer testing.

## ESP32-C5 board

The C5 side remains replaceable during early RF research. Use a C5 board that exposes enough GPIO for the high-speed C5->S3 link and at least four ADC-capable inputs if gimbals are acquired on the C5.

Requirements:

- ESP32-C5
- working 5 GHz RF path/antenna
- common ground with S3
- six GPIO for the chosen QSPI mapping on the C5 side
- four usable ADC inputs for gimbal axes, unless an alternate control-input plan is chosen
- independent debug access during transport testing

ESP32-C5 provides six ADC1 channels on GPIO1..GPIO6. The exact C5 pin assignment must be chosen together with SPI/QSPI timing requirements; do not assume the default FSPI IO-MUX pins and all six ADC channels can be used simultaneously without conflicts.

## C5 <-> S3 bus

Prototype wiring is full QSPI with no separate READY pin on the S3 target because the exact Waveshare board does not have a comfortable spare high-speed GPIO after RGB, CRSF and battery functions are reserved.

```text
S3 master                         C5 slave
----------                        --------
SCLK  --------------------------> SCLK
CS    --------------------------> CS
IO0   <-------------------------- IO0
IO1   <-------------------------- IO1
IO2   <-------------------------- IO2
IO3   <-------------------------- IO3
GND   --------------------------- GND
```

The S3 polls on a fixed cadence and the C5 keeps the next DMA block queued. Sequence numbers and discontinuity flags detect underrun/overrun instead of relying on a READY wire.

Keep prototype wiring short. Start below 40 MHz, run counter/PRBS tests, then increase the clock. GPIO19/20 are also routed to the USB-C connector, so that physical stub is part of the real signal-integrity experiment.

## ExpressLRS module

OpenPocket uses a **supported ExpressLRS receiver deliberately flashed with RX-as-TX firmware** as the 2.4 GHz transmitter module.

Connection on the exact Waveshare board:

```text
S3 GPIO43 / CRSF TX  -----------> ELRS UART RX
S3 GPIO44 / CRSF RX  <----------- ELRS UART TX
S3 GND               ------------ ELRS GND
regulated supply      ------------ ELRS VCC
```

Use the exact receiver's ExpressLRS target and preserve/restore its own `hardware.json`. A seller label saying "100 mW" is not enough to define the safe PA configuration.

## Controls

Minimum useful OpenPocket control set:

- 4 analog gimbal axes
- dedicated ARM switch
- at least 2 auxiliary switches
- menu/navigation inputs or encoder
- optional trims
- buzzer

### Control-input strategy for this board

The exact 40-pin S3 target does not leave four clean ADC inputs once RGB and the high-speed C5 bus are active. The preferred low-part-count prototype therefore samples the four gimbal axes on the **ESP32-C5** and carries a control snapshot alongside each/selected video transport blocks.

```text
gimbals -> C5 ADC -> transport metadata -> S3 -> RivetTX mixer -> CRSF -> ELRS
```

This is extremely low bandwidth compared with video and adds negligible transport load. Slow switches may live on free C5 GPIO or the Waveshare TCA9554 expander.

The S3 remains responsible for RivetTX timing, mixing, safety state and CRSF; the C5 is only the remote input sampler in addition to its RF/video role.

## Power architecture

Prototype intent:

```text
1S Li-ion/LiPo
      |
      +--> exact Waveshare S3 board ETA6096 power/charge path
      |
      +--> C5 rail (through a verified regulator/path)
      |
      +--> ELRS module rail (through a verified regulator/path)
```

Do **not** assume the development board's 3.3 V rail can supply the C5, LCD backlight and a 100 mW ELRS PA simultaneously. Measure the real rail topology and current capability first.

For bring-up it is acceptable to power the C5 and ELRS module from separate known-good bench supplies while sharing ground. Merge rails only after peak-current and noise measurements exist.

## RF coexistence

The handset contains two active radios:

- 2.4 GHz ELRS TX
- 5.8 GHz C5 VRX

Test C5 video quality with ELRS disabled and at each intended TX power, supply ripple during ELRS bursts, digital-clock coupling into the 5.8 GHz path, antenna placement and thermals.

## Minimum development prototype

```text
[1S battery]
     |
[Waveshare ESP32-S3-LCD-Driver-Board]
     |             |
  40-pin RGB      GPIO43/44
     |             |
   [LCD]         [ELRS RX-as-TX]
     |
     +---- QSPI on reused runtime pins ---- [ESP32-C5]
                                               |
                              gimbals ---------+
                                               |
                                         5.8 GHz antenna
```

That is the exact hardware shape this repository now targets before a custom integrated PCB is considered.
