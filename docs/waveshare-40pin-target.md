# Exact S3 target: Waveshare ESP32-S3-LCD-Driver-Board

C5VRX-OpenPocket targets the **specific Waveshare ESP32-S3-LCD-Driver-Board shown in the prototype listing**, not a generic ESP32-S3 LCD board.

Official product name: **ESP32-S3-LCD-Driver-Board** (Waveshare SKU 27686).

The board uses an **ESP32-S3-WROOM-1-N8R8** and exposes the RGB panel through its onboard **40-pin 3SPI + RGB connector**. It also includes 8 MB PSRAM, 8 MB flash, USB-C, ETA6096 1-cell lithium charge/discharge management, an MX1.25 battery connector, a TCA9554 GPIO expander and the LCD backlight supply.

Official references:

- <https://docs.waveshare.com/ESP32-S3-LCD-Driver-Board>
- <https://files.waveshare.com/wiki/ESP32-S3-LCD-Driver-Board/ESP32-S3-LCD-Driver-Board.pdf>

## 40-pin RGB wiring

The following signals are fixed by the Waveshare PCB:

| Function | ESP32-S3 GPIO |
|---|---:|
| LCD serial init SDA | 1 |
| LCD serial init SCK | 2 |
| LCD serial init CS | 42 |
| PCLK | 41 |
| DE | 40 |
| VSYNC | 39 |
| HSYNC | 38 |
| B1..B5 | 5, 45, 48, 47, 21 |
| G0..G5 | 14, 13, 12, 11, 10, 9 |
| R1..R5 | 46, 3, 8, 18, 17 |
| touch INT | 16 |
| touch SDA | 15 |
| touch SCL | 7 |
| LCD reset | TCA9554 EXIO3 |
| touch reset | TCA9554 EXIO1 |
| backlight enable | TCA9554 EXIO2 |

Waveshare leaves R0 and B0 unconnected. Their official RGB examples currently use 480x480 ST7701 panels. An 800x480 or 480x800 panel is only a later option if its 40-pin electrical pinout, RGB timing, voltage and backlight requirements match or an adapter is used.

## Important consequence: GPIO is tight

This board spends most native S3 GPIO on the parallel RGB bus. The design therefore **cannot be treated like a generic S3 dev board** with six arbitrary spare pins for QSPI plus four ADC gimbals plus CRSF.

The prototype must deliberately reuse pins that become available after LCD initialization.

## Candidate C5 -> S3 full-QSPI allocation

The current candidate mapping is:

| Link signal | S3 GPIO | Why it can be used |
|---|---:|---|
| SCLK | 6 | unused by the 40-pin RGB path |
| CS | 16 | touch INT; available only when touch is held in reset |
| IO0 | 1 | LCD init SDA; reusable after panel init while LCD CS stays high |
| IO1 | 2 | LCD init SCK; reusable after panel init while LCD CS stays high |
| IO2 | 19 | native USB D-; reusable only when USB is disconnected |
| IO3 | 20 | native USB D+; reusable only when USB is disconnected |
| CRSF TX | 43 | UART header |
| CRSF RX | 44 | UART header |
| battery ADC | 4 | existing board divider |

This is a **candidate hardware experiment**, not a validated production pin map.

Runtime order should be:

```text
1. boot S3
2. initialize TCA9554
3. hold touch controller in reset
4. reset/init ST7701 RGB panel using GPIO1/2 and GPIO42
5. leave LCD_CS GPIO42 high
6. start RGB LCD peripheral
7. disconnect/avoid native USB
8. reconfigure GPIO1,2,6,16,19,20 for the C5 transport
9. start CRSF on GPIO43/44
```

The LCD serial init pins remain physically connected to the display, but with its CS held high the panel should ignore toggling on SDA/SCK. This must be verified electrically on the real panel.

GPIO19/20 remain physically routed to the USB-C connector, so the connector stub may affect signal integrity. Do not assume a 40 MHz QSPI clock passes until a sustained PRBS/counter test proves it.

## Touch policy

For the first OpenPocket prototype, **touch is disabled**. The touch controller is held in reset so GPIO16 can be used as C5-link chip select. RivetTX does not need touchscreen input for flight controls.

If touch becomes a product requirement later, the C5 transport pin map must change.

## Controls and gimbals

Four S3 ADC pins are not realistically available while this exact 40-pin RGB interface and the high-speed C5 link are active. The lowest-hardware-count prototype therefore treats control acquisition as a distributed job:

```text
analog gimbals / selected switches
             |
             v
          ESP32-C5
             |
  video blocks + control snapshot
             |
             v
          ESP32-S3
             |
           RivetTX
             |
          CRSF UART
             |
      ELRS RX-as-TX module
```

ESP32-C5 provides six ADC1 channels on GPIO1..GPIO6, so four gimbal axes are possible in principle. The exact C5 dev-board pin allocation must be chosen together with its SPI/QSPI pins and RF bring-up requirements.

Slow switches may also be placed on available C5 GPIO or on the Waveshare TCA9554 expander where appropriate.

This keeps the prototype at the intended hardware shape: **C5 board + exact Waveshare S3 40-pin board + ELRS RX-as-TX module + LCD + controls + battery**, without adding a separate ADC MCU.
