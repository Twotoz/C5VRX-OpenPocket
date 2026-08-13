#include <inttypes.h>

#include "esp_log.h"
#include "c5vrx_openpocket_protocol.h"
#include "board_waveshare_s3_lcd_driver.h"

static const char *TAG = "c5op-s3";

void app_main(void)
{
    ESP_LOGI(TAG, "C5VRX-OpenPocket ESP32-S3 bring-up harness");
    ESP_LOGI(TAG, "board target: %s", C5OP_BOARD_NAME);
    ESP_LOGI(TAG, "expecting protocol v%u with %u-byte headers",
             (unsigned)C5OP_VIDEO_PROTOCOL_VERSION,
             (unsigned)sizeof(c5op_video_block_header_t));
    ESP_LOGI(TAG,
             "candidate C5 link: CLK=%d CS=%d D0=%d D1=%d D2=%d D3=%d; CRSF TX/RX=%d/%d",
             C5OP_QSPI_SCLK_GPIO,
             C5OP_QSPI_CS_GPIO,
             C5OP_QSPI_IO0_GPIO,
             C5OP_QSPI_IO1_GPIO,
             C5OP_QSPI_IO2_GPIO,
             C5OP_QSPI_IO3_GPIO,
             C5OP_CRSF_TX_GPIO,
             C5OP_CRSF_RX_GPIO);
    ESP_LOGW(TAG,
             "This application is a board/video/transport test harness only; the target OpenPocket radio firmware is RivetTX on ESP32-S3.");
    ESP_LOGW(TAG,
             "LCD init must happen before GPIO1/2 are reused; touch must be held reset and native USB disconnected before QSPI starts.");
    ESP_LOGW(TAG,
             "LCD, QSPI RX and CVBS decode are intentionally isolated here until they can be integrated into RivetTX as non-blocking hardware services.");
}
