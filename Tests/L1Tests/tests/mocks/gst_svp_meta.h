/*
 * Stub for <gst_svp_meta.h> / rdk_perf.h used by the RDK GStreamer adapter.
 * RDKPerf is a scoped performance timer; stubbed as a no-op class in unit tests.
 */

#pragma once

#include <cstdint>

class RDKPerf {
public:
    explicit RDKPerf(const char*) {}
    RDKPerf(const char*, uint32_t) {}
    void SetThreshhold(uint32_t) {}
};

#ifdef ENABLE_MULTI_DECRYPT
bool gst_svp_is_multiple_decrypt_supported();
#endif
