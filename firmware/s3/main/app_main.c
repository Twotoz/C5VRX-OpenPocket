#include <inttypes.h>

#include "esp_log.h"
#include "c5vrx_openpocket_protocol.h"

static const char *TAG = "c5op-s3";

void app_main(void)
{
    ESP_LOGI(TAG, "C5VRX-OpenPocket ESP32-S3 scaffold");
    ESP_LOGI(TAG, "expecting protocol v%u with %u-byte headers",
             (unsigned)C5OP_VIDEO_PROTOCOL_VERSION,
             (unsigned)sizeof(c5op_video_block_header_t));
    ESP_LOGW(TAG,
             "LCD, QSPI RX, CVBS decode and RivetTX integration are intentionally not enabled in this scaffold yet.");
}
