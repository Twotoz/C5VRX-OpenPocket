# Firmware workspaces

C5VRX-OpenPocket contains **two separate ESP-IDF applications** because the ESP32-C5 and ESP32-S3 have different targets and responsibilities.

## ESP32-C5

```bash
cd firmware/c5
idf.py set-target esp32c5
idf.py build
```

The initial scaffold only verifies the shared protocol header. Planned work:

1. generated CVBS/test-pattern producer
2. QSPI slave + DMA transport
3. integration with upstream C5VRX sampled-CVBS output

Generic RF work should remain in or be upstreamed to <https://github.com/Twotoz/C5VRX>.

## ESP32-S3

```bash
cd firmware/s3
idf.py set-target esp32s3
idf.py build
```

The initial scaffold only verifies the shared protocol header. Planned work:

1. Waveshare RGB LCD bring-up
2. synthetic sampled-CVBS decoder
3. QSPI master + DMA receiver
4. digital RGB-LCD RivetTX presentation backend
5. CRSF/RivetTX concurrency tests

Do not flash an S3 binary to the C5 or vice versa.

## Shared protocol

Both applications include:

```text
protocol/c5vrx_openpocket_protocol.h
```

Any wire-format change must update both the header and `docs/video-link.md`.
