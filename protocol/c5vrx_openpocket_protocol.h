#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define C5OP_VIDEO_MAGIC0 0xC5u
#define C5OP_VIDEO_MAGIC1 0x56u
#define C5OP_VIDEO_PROTOCOL_VERSION 0u

#define C5OP_VIDEO_FLAG_PRODUCER_OVERRUN   (1u << 0)
#define C5OP_VIDEO_FLAG_DISCONTINUITY       (1u << 1)
#define C5OP_VIDEO_FLAG_LEVELS_NORMALIZED   (1u << 2)
#define C5OP_VIDEO_FLAG_TEST_PATTERN        (1u << 3)
#define C5OP_VIDEO_FLAG_RF_PRESENT          (1u << 4)

#define C5OP_VIDEO_STANDARD_UNKNOWN 0u
#define C5OP_VIDEO_STANDARD_PAL     1u
#define C5OP_VIDEO_STANDARD_NTSC    2u

typedef struct __attribute__((packed)) {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t flags;
    uint32_t sequence_le;
    uint32_t sample_rate_hz_le;
    uint16_t payload_bytes_le;
    uint8_t video_standard_hint;
    uint8_t reserved;
} c5op_video_block_header_t;

_Static_assert(sizeof(c5op_video_block_header_t) == 16,
               "C5OP video header must stay 16 bytes");

static inline int c5op_video_header_has_magic(const c5op_video_block_header_t *h)
{
    return h != 0 &&
           h->magic0 == C5OP_VIDEO_MAGIC0 &&
           h->magic1 == C5OP_VIDEO_MAGIC1;
}

#ifdef __cplusplus
}
#endif
