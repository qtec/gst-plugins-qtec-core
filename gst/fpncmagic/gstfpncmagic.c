/* GStreamer
 * Copyright (C) 2015 Dimitrios Katsaros <patcherwork@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstfpncmagic
 *
 * The fpncmagic element is used to calculate the sensor error used by certain
 * calculations
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <byteswap.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <gst/fpncmagic/gstfpncmagicmeta.h>
#include "gstfpncmagic.h"
#include "quickselect.h"

GST_DEBUG_CATEGORY_STATIC (gst_fpncmagic_debug_category);
#define GST_CAT_DEFAULT gst_fpncmagic_debug_category

/* prototypes */


static void gst_fpncmagic_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_fpncmagic_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_fpncmagic_dispose (GObject * object);
static void gst_fpncmagic_finalize (GObject * object);

static gboolean gst_fpncmagic_start (GstBaseTransform * trans);
static gboolean gst_fpncmagic_stop (GstBaseTransform * trans);
static gboolean gst_fpncmagic_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_fpncmagic_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static GstCaps *gst_fpncmagic_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_fpncmagic_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

enum
{
  PROP_0
};

/* pad templates */


#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ GRAY8, GRAY16_LE, GRAY16_BE }")


#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ GRAY8, GRAY16_LE, GRAY16_BE }")

#define RANGE 50

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstFpncmagic, gst_fpncmagic, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_fpncmagic_debug_category, "fpncmagic", 0,
  "debug category for fpncmagic element"));

static void
gst_fpncmagic_class_init (GstFpncmagicClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "FPNC Magic", "Generic", "Calculates sensor `magic` error values",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_fpncmagic_set_property;
  gobject_class->get_property = gst_fpncmagic_get_property;
  gobject_class->dispose = gst_fpncmagic_dispose;
  gobject_class->finalize = gst_fpncmagic_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_fpncmagic_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_fpncmagic_stop);
  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_fpncmagic_fixate_caps);
  base_transform_class->transform_caps = gst_fpncmagic_transform_caps;
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_fpncmagic_set_info);
  video_filter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (gst_fpncmagic_transform_frame_ip);
}

static void
gst_fpncmagic_init (GstFpncmagic *fpncmagic)
{
  fpncmagic->rolling_medians = NULL;
  fpncmagic->errors = NULL;
}

void
gst_fpncmagic_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (object);

  GST_DEBUG_OBJECT (fpncmagic, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_fpncmagic_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (object);

  GST_DEBUG_OBJECT (fpncmagic, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_fpncmagic_dispose (GObject * object)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (object);

  GST_DEBUG_OBJECT (fpncmagic, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_fpncmagic_parent_class)->dispose (object);
}

void
gst_fpncmagic_finalize (GObject * object)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (object);

  GST_DEBUG_OBJECT (fpncmagic, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_fpncmagic_parent_class)->finalize (object);
}

static gboolean
gst_fpncmagic_start (GstBaseTransform * trans)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (trans);

  GST_DEBUG_OBJECT (fpncmagic, "start");

  return TRUE;
}

static GstCaps *
gst_fpncmagic_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstStructure *newstruct;
  GstCaps *newcaps, *ret;

  newcaps = gst_caps_copy (caps);
  newstruct = gst_caps_get_structure (newcaps, 0);

  GST_DEBUG_OBJECT (base,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  gst_structure_set (newstruct, "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
    NULL);

  /* if a filter is present, it needs to be applied */
  if (!filter)
    ret = newcaps;
  else {
    ret = gst_caps_intersect_full(filter, newcaps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (newcaps);
  }

  return ret;
}

static GstCaps *
gst_fpncmagic_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{

  GstStructure *outs;
  outs = gst_caps_get_structure (othercaps, 0);

  GST_DEBUG_OBJECT (base,
      "Fixating caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  GST_DEBUG ("othercaps %" GST_PTR_FORMAT, othercaps);

  gst_structure_set(outs, "height", G_TYPE_INT, 1, NULL);

  return othercaps;
}

static gboolean
gst_fpncmagic_stop (GstBaseTransform * trans)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (trans);

  if (fpncmagic->rolling_medians) {
    free (fpncmagic->rolling_medians);
    fpncmagic->rolling_medians = NULL;
  }

  if (fpncmagic->errors) {
    free (fpncmagic->errors);
    fpncmagic->errors = NULL;
  }

  return TRUE;
}

static gboolean
gst_fpncmagic_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (filter);

  if (in_info->height != 1 || out_info->height != 1) {
    GST_DEBUG("FPNC magic requires input and output of size 1");
    return FALSE;
  }

  if (fpncmagic->rolling_medians) {
    free (fpncmagic->rolling_medians);
    fpncmagic->rolling_medians = NULL;
  }

  if (fpncmagic->errors) {
    free (fpncmagic->errors);
    fpncmagic->errors = NULL;
  }

  fpncmagic->rolling_medians = malloc(in_info->width * sizeof(gint));
  if (!fpncmagic->rolling_medians) {
    GST_ERROR("Unable to allocate memory of size: %lu", in_info->width * sizeof(gint));
    return FALSE;
  }

  fpncmagic->errors = malloc(in_info->width * sizeof(gint));
  if (!fpncmagic->errors) {
    GST_ERROR("Unable to allocate memory of size: %lu", in_info->width * sizeof(gint));
    free(fpncmagic->rolling_medians);
    return FALSE;
  }

  GST_DEBUG ("in caps %" GST_PTR_FORMAT " out caps %" GST_PTR_FORMAT, incaps,
      outcaps);

  return TRUE;
}

static void
fpncmagic_8b (GstVideoFilter * filter, GstVideoFrame *frame)
{
  gint i, min, max;
  guint8 medarray[RANGE];

  guint8 *data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (filter);
  gint *rms = (gint*)fpncmagic->rolling_medians;
  gint *errs = (gint*)fpncmagic->errors;
  gint avg = 0;

  for (i=0; i<frame->info.width; i++) {
    min = i - RANGE/2;
    max = i + RANGE/2;
    if (min < 0)
      min = 0;
    if (max >= frame->info.width)
      max = frame->info.width - 1;
    memcpy(medarray, data + min, max - min);
    rms[i] = quick_select_uint8_t(medarray, max - min);
    errs[i] = data[i] - rms[i];
    avg += data[i];
  }

  gst_buffer_add_gst_fpnc_magic_meta(frame->buffer, fpncmagic->rolling_medians,
    fpncmagic->errors, frame->info.width, avg/frame->info.width);
}

static void
fpncmagic_16b_LE (GstVideoFilter * filter, GstVideoFrame * frame)
{
  gint i, min, max;
  guint16 medarray[RANGE];

  guint16 *data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (filter);
  gint *rms = (gint*)fpncmagic->rolling_medians;
  gint *errs = (gint*)fpncmagic->errors;
  gint avg = 0;

  for (i=0; i<frame->info.width; i++) {
    min = i - RANGE/2;
    max = i + RANGE/2;
    if (min < 0)
      min = 0;
    if (max >= frame->info.width)
      max = frame->info.width - 1;
    memcpy(medarray, data + min, (max - min)*2);
    rms[i] = quick_select_uint16_t(medarray, max - min);
    errs[i] = data[i] - rms[i];
    avg += data[i];
  }

  gst_buffer_add_gst_fpnc_magic_meta(frame->buffer, fpncmagic->rolling_medians,
    fpncmagic->errors, frame->info.width, avg/frame->info.width);
}

static void
fpncmagic_16b_BE (GstVideoFilter * filter, GstVideoFrame * frame)
{
  gint i, j, min, max;
  guint16 medarray[RANGE];
  guint16 val;

  guint16 *data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  GstFpncmagic *fpncmagic = GST_FPNCMAGIC (filter);
  gint *rms = (gint*)fpncmagic->rolling_medians;
  gint *errs = (gint*)fpncmagic->errors;
  gint avg = 0;

  for (i=0; i<frame->info.width; i++) {
    val = __bswap_16(data[i]);
    min = i - RANGE/2;
    max = i + RANGE/2;
    if (min < 0)
      min = 0;
    if (max >= frame->info.width)
      max = frame->info.width - 1;
    /* need to copy and swap bytes for correct alignment */
    for (j=0; j<(max - min); j++)
      medarray[j] = __bswap_16(data[min + j]);
    rms[i] = quick_select_uint16_t(medarray, max - min);
    errs[i] = val - rms[i];
    avg += val;
  }

  gst_buffer_add_gst_fpnc_magic_meta(frame->buffer, fpncmagic->rolling_medians,
    fpncmagic->errors, frame->info.width, avg/frame->info.width);
}

static GstFlowReturn
gst_fpncmagic_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  if (frame->info.finfo->bits == 8)
    fpncmagic_8b (filter, frame);
  else if (frame->info.finfo->bits == 16){
    if (frame->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE)
      fpncmagic_16b_BE (filter, frame);
    else if (frame->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE)
      fpncmagic_16b_LE (filter, frame);
    else {
      GST_ERROR("Unhandled format type");
      return GST_FLOW_ERROR;
    }
  }
  else {
    GST_ERROR("Unhandled data size of %d bits", frame->info.finfo->bits);
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "fpncmagic", GST_RANK_NONE,
      GST_TYPE_FPNCMAGIC);
}


#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "fpncmagic"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "FPNC magic"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fpncmagic,
    "Plugin for fpnc magic value calculation",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)