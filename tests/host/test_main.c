#include <stdio.h>
#include <string.h>

#include "velocity9x/build.h"
#include "velocity9x/components.h"
#include "velocity9x/s3_virge.h"

static unsigned int failures = 0u;

#define CHECK(expression) do { \
    if (!(expression)) { \
        printf("FAIL %s:%u: %s\n", __FILE__, (unsigned int)__LINE__, #expression); \
        ++failures; \
    } \
} while (0)

struct capture_sink {
    struct v9x_log_record records[8];
    v9x_u16 count;
};

static v9x_status capture_log(void *context,
                              const struct v9x_log_record *record)
{
    struct capture_sink *sink = (struct capture_sink *)context;
    if (sink->count >= 8u) {
        return V9X_STATUS_INSUFFICIENT_MEMORY;
    }
    sink->records[sink->count++] = *record;
    return V9X_STATUS_OK;
}

static void test_mode_layout(void)
{
    struct v9x_mode_request request;
    struct v9x_mode_layout layout;

    request.width = 640u;
    request.height = 480u;
    request.bits_per_pixel = 8u;
    request.pitch_alignment = 8u;
    request.framebuffer_bytes = 4ul * 1024ul * 1024ul;
    CHECK(v9x_mode_calculate(&request, &layout) == V9X_STATUS_OK);
    CHECK(layout.pitch_bytes == 640ul);
    CHECK(layout.visible_bytes == 307200ul);
    CHECK(layout.offscreen_bytes == request.framebuffer_bytes - 307200ul);

    request.width = 641u;
    request.bits_per_pixel = 16u;
    CHECK(v9x_mode_calculate(&request, &layout) == V9X_STATUS_OK);
    CHECK(layout.pitch_bytes == 1288ul);

    request.bits_per_pixel = 32u;
    CHECK(v9x_mode_calculate(&request, &layout) == V9X_STATUS_UNSUPPORTED);

    request.bits_per_pixel = 8u;
    request.pitch_alignment = 3u;
    CHECK(v9x_mode_calculate(&request, &layout) == V9X_STATUS_INVALID_ARGUMENT);

    request.pitch_alignment = 8u;
    request.framebuffer_bytes = 1024ul;
    CHECK(v9x_mode_calculate(&request, &layout) ==
          V9X_STATUS_INSUFFICIENT_MEMORY);
}

static void test_probe_is_strict(void)
{
    struct v9x_backend_state state;
    struct v9x_pci_identity pci;
    struct v9x_mode_request request;
    struct v9x_mode_layout layout;

    memset(&state, 0, sizeof(state));
    pci.vendor_id = V9X_PCI_VENDOR_S3;
    pci.device_id = V9X_PCI_DEVICE_VIRGE_DX;
    pci.revision = 1u;
    CHECK(v9x_s3_virge_probe(&state, &pci) == V9X_STATUS_OK);
    CHECK(state.initialized == V9X_TRUE);
    CHECK(state.capabilities == 0ul);

    request.width = 640u;
    request.height = 480u;
    request.bits_per_pixel = 8u;
    request.pitch_alignment = 8u;
    request.framebuffer_bytes = 1ul; /* The backend must use trusted state. */
    state.vram_bytes = 4ul * 1024ul * 1024ul;
    CHECK(v9x_s3_virge_validate_mode(&state, &request, &layout) ==
          V9X_STATUS_OK);
    CHECK(layout.visible_bytes == 307200ul);

    pci.device_id = 0x5631u;
    CHECK(v9x_s3_virge_probe(&state, &pci) == V9X_STATUS_UNSUPPORTED);
    CHECK(state.initialized == V9X_FALSE);
    CHECK(state.capabilities == 0ul);
    CHECK(state.pci.vendor_id == 0u);
    CHECK(v9x_s3_virge_validate_mode(&state, &request, &layout) ==
          V9X_STATUS_INVALID_STATE);
}

static void test_components_and_log(void)
{
    struct capture_sink sink;
    struct v9x_logger logger;
    struct v9x_backend_state backend;
    struct v9x_component_state display;
    struct v9x_component_state minivdd;

    memset(&sink, 0, sizeof(sink));
    memset(&backend, 0, sizeof(backend));
    memset(&display, 0, sizeof(display));
    memset(&minivdd, 0, sizeof(minivdd));
    v9x_log_init(&logger, capture_log, &sink);

    CHECK(v9x_display16_start(&display, &logger, &backend) == V9X_STATUS_OK);
    CHECK(v9x_display16_start(&display, &logger, &backend) ==
          V9X_STATUS_INVALID_STATE);
    CHECK(v9x_minivdd32_start(&minivdd, &logger, &backend) == V9X_STATUS_OK);
    CHECK(v9x_display16_stop(&display) == V9X_STATUS_OK);
    CHECK(v9x_minivdd32_stop(&minivdd) == V9X_STATUS_OK);

    CHECK(sink.count == 4u);
    CHECK(sink.records[0].magic == V9X_LOG_MAGIC);
    CHECK(sink.records[0].size == 32u);
    CHECK(sink.records[0].sequence == 0ul);
    CHECK(sink.records[1].sequence == 1ul);
    CHECK(sink.records[0].argument0 == 16ul);
    CHECK(sink.records[1].argument0 == 32ul);
}

static void test_build_identity(void)
{
    const struct v9x_build_identity *identity = v9x_get_build_identity();
    CHECK(identity != 0);
    CHECK(identity->major == V9X_VERSION_MAJOR);
    CHECK(identity->build_id != 0);
    CHECK(identity->build_id[0] != '\0');
}

int main(void)
{
    test_mode_layout();
    test_probe_is_strict();
    test_components_and_log();
    test_build_identity();

    if (failures != 0u) {
        printf("%u host test(s) failed\n", failures);
        return 1;
    }
    puts("Velocity9x host tests passed");
    return 0;
}
