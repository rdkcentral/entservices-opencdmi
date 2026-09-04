/*
 * Minimal GStreamer type stubs for RDK adapter unit tests.
 * Replaces <gst/gst.h> so open_cdm_decrypt.cpp compiles without GStreamer.
 */

#pragma once

#include <cstddef>
#include <cstdint>

typedef int      gboolean;
typedef uint8_t  guint8;
typedef size_t   gsize;
typedef char     gchar;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef enum {
    GST_MAP_READ      = 0x01,
    GST_MAP_WRITE     = 0x02,
    GST_MAP_READWRITE = 0x03
} GstMapFlags;

struct _GstBuffer   { int _unused; };
struct _GstCaps     { int _unused; };
struct _GstStructure{ int _unused; };
struct _GValue      { int _unused; };

typedef struct _GstBuffer    GstBuffer;
typedef struct _GstCaps      GstCaps;
typedef struct _GstStructure GstStructure;
typedef struct _GValue       GValue;

struct GstMapInfo {
    guint8* data;
    gsize   size;
};

typedef struct {
    GstStructure* info;
} GstProtectionMeta;

#ifdef __cplusplus
extern "C" {
#endif

gboolean        gst_buffer_map(GstBuffer* buffer, GstMapInfo* info, GstMapFlags flags);
void            gst_buffer_unmap(GstBuffer* buffer, GstMapInfo* info);
GstProtectionMeta* gst_buffer_get_protection_meta(GstBuffer* buffer);
const GValue*   gst_structure_get_value(const GstStructure* structure, const gchar* fieldname);
GstBuffer*      gst_value_get_buffer(const GValue* value);

#ifdef __cplusplus
}
#endif
