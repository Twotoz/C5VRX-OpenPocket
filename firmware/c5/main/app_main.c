#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "c5vrx_openpocket_protocol.h"

static const char *TAG = "c5op-c5";

void app_main(void)
{
    c5op_video_block_header_t header;
    memset(&header, 0, sizeof(header));

    header.magic0 = C5OP_VIDEO_MAGIC0;
    header.magic1 = C5OP_VIDEO_MAGIC1;
    header.version = C5OP_VIDEO_PROTOCOL_VERSION;
    header.flags = C5OP_VIDEO_FLAG_TEST_PATTERN;
    header.sample_rate_hz_le = 10000000u;
    header.payload_bytes_le = 4096u;
    header.video_standard_hint = C5OP_VIDEO_STANDARD_UNKNOWN;

    ESP_LOGI(TAG, "C5VRX-OpenPocket ESP32-C5 scaffold");
    ESP_LOGI(TAG, "video protocol v%u, header=%u bytes",
             (unsigned)header.version,
             (unsigned)sizeof(header));
    ESP_LOGW(TAG,
             "No live RF path is enabled yet. Next milestone: DMA test-pattern producer -> QSPI slave transport.");
}
