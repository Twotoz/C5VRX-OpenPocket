# Development hardware

This document defines the **bring-up hardware**, not the final production PCB.

## Main processor / LCD board

Initial target: **Waveshare ESP32-S3-LCD-Driver-Board**.

Official board documentation:

- <https://www.waveshare.com/wiki/ESP32-S3-LCD-Driver-Board>
- <https://docs.waveshare.com/ESP32-S3-LCD-Driver-Board>

Relevant onboard hardware:

- ESP32-S3-WROOM-1-N8R8
- dual-core LX7 up to 240 MHz
- 8 MB PSRAM
- 8 MB flash
- USB Type-C
- 40-pin SPI + RGB LCD connector
- 18-pin SPI LCD connector
- ETA6096 lithium battery charge/discharge manager
- MX1.25 2-pin connector intended for a 3.7 V lithium battery
- TCA9554 GPIO expander
- AP3032KTR boost converter
- ME6217C33M5G 3.3 V regulator

The official Waveshare examples currently list 480x480 ST7701 RGB displays for the board. C5VRX-OpenPocket is **not limited to 480x480 by the ESP32-S3 architecture**, but an arbitrary 800x480 panel cannot simply be assumed to be pin-compatible with the Waveshare 40-pin connector. Check the exact panel pinout, voltage rails, backlight requirements and timing before connecting it.

### Why this board is useful

It bundles the parts we otherwise would have to prototype separately:

```text
USB-C
  |
charge / power management
  |
1S battery ------------+
                        |
                   ESP32-S3 + PSRAM
                        |
                     RGB LCD
```

That makes it a good development platform even if the final OpenPocket uses a custom S3 carrier/mainboard.

## ESP32-C5 board

The C5 side should remain replaceable during early RF research. Use a C5 board that exposes enough GPIO for the selected high-speed C5->S3 link and allows normal USB/JTAG logging/flash recovery.

Requirements:

- ESP32-C5
- working 5 GHz RF path/antenna
- common ground with S3
- enough exposed high-speed GPIO for SCLK, CS and four data lines
- preferably one extra READY/IRQ GPIO
- independent debug access during transport testing

Do not commit a production pin map until continuous RF capture and the video transport are both proven.

## C5 <-> S3 bus

Preferred prototype wiring:

| Signal | Direction during video | Notes |
|---|---|---|
| SCLK | S3 -> C5 | S3 clocks slave transfers |
| CS | S3 -> C5 | dedicated chip select |
| IO0 | C5 -> S3 | QSPI data during video |
| IO1 | C5 -> S3 | QSPI data during video |
| IO2 | C5 -> S3 | QSPI data during video |
| IO3 | C5 -> S3 | QSPI data during video |
| READY | C5 -> S3 | recommended producer-ready signal |
| GND | shared | mandatory common reference |

Keep the prototype wires short. A 40 MHz multi-line bus over loose Dupont leads is not a production-quality interconnect. Start at a lower clock and increase it while measuring errors.

## ExpressLRS module

OpenPocket uses a **supported ExpressLRS receiver deliberately flashed with RX-as-TX firmware** as the 2.4 GHz transmitter module.

Connection:

```text
S3 / RivetTX                 ELRS RX-as-TX
------------                 ------------
CRSF TX  ------------------> UART RX
CRSF RX  <------------------ UART TX
GND      ------------------- GND
supply   ------------------- VCC
```

Use the exact receiver's ExpressLRS target and preserve/restore its own `hardware.json`. A seller label saying "100 mW" is not enough to define the safe PA configuration.

For a board that really supports a 100 mW PA, size the supply for its peak draw and add local bulk capacitance close to the module.

## Controls

Minimum useful OpenPocket control set:

- 4 analog gimbal axes
- dedicated ARM switch
- at least 2 auxiliary switches
- menu/navigation inputs or encoder
- optional trims
- buzzer

The Waveshare LCD board consumes many GPIOs for the RGB panel, so **pin-budgeting is a first-class constraint**. Do not copy RivetTX's generic S3 development defaults blindly.

### GPIO strategy

Prefer, in order:

1. ADC-capable native S3 pins for the four gimbal axes.
2. Native UART-capable routing for CRSF.
3. Native high-speed pins for the C5 QSPI link.
4. GPIO expander for slow menu buttons/switches where latency is non-critical.
5. Avoid touch functionality if its pins are needed for the C5 transport during the first prototype.

The final allocation must be generated from the exact Waveshare schematic plus the exact LCD/touch configuration in use.

## Power architecture

Prototype intent:

```text
1S Li-ion/LiPo
      |
      +--> S3 LCD board power/charge path
      |
      +--> C5 rail (through verified regulator/path)
      |
      +--> ELRS module rail (through verified regulator/path)
```

Do **not** assume the development board's exposed 3.3 V rail can supply the C5, LCD backlight and a 100 mW ELRS PA simultaneously. Measure the board's real rail topology and current capability first.

For bring-up it is acceptable to power the C5 and ELRS module from separate known-good bench supplies while sharing ground. Merge the rails only after peak-current and noise measurements exist.

## RF coexistence

The handset contains two active radios:

- 2.4 GHz ELRS TX
- 5.8 GHz C5 VRX

They are far apart in frequency but still share board power, digital ground and a small enclosure. Test:

- C5 video quality with ELRS disabled vs 10/25/50/100 mW
- supply ripple during ELRS bursts
- S3/C5 digital-clock coupling into the 5.8 GHz path
- antenna placement/orientation
- thermal behavior

Keep the 2.4 GHz PA/antenna physically away from the C5 RF input and from high-impedance analog/RF nodes.

## What the development prototype should contain

```text
[1S battery]
     |
[Waveshare S3 LCD board] ----- [RGB LCD]
     |       |
     |       +---- CRSF ---- [ELRS RX-as-TX]
     |
     +------ QSPI --------- [ESP32-C5 board]
                                |
                             5.8 GHz antenna
```

That is the minimum architecture this repository is trying to validate before designing a custom integrated PCB.
