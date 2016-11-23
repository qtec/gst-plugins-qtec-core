/* GStreamer
 * Copyright (C) 2015 Dimitrios Katsaros <patcherwrok@gmail.com>
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
 * SECTION:element-gstavgframes
 *
 * The avgframes element takes a number of frames and averages them
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! avgframes ! fakesink
 * ]|
 * takes frames and averages them
 * </refsect2>
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
#include "gstavgframes.h"

GST_DEBUG_CATEGORY_STATIC (gst_avg_frames_debug_category);
#define GST_CAT_DEFAULT gst_avg_frames_debug_category

/* prototypes */


static void gst_avg_frames_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_avg_frames_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_avg_frames_finalize (GObject * object);

static gboolean gst_avg_frames_start (GstBaseTransform * trans);
static gboolean gst_avg_frames_stop (GstBaseTransform * trans);
static gboolean gst_avg_frames_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_avg_frames_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static gboolean gst_avg_frames_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_avg_frames_src_event (GstBaseTransform * trans,
    GstEvent * event);

#define DEFAULT_FRAME_NO 10
#define MIN_FRAME_NO 1
#define MAX_FRAME_NO 100

enum
{
  PROP_0,
  PROP_NO_OF_FRAMES
};

/* pad templates */

#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ RGBA, GRAY8, GRAY16_BE, GRAY16_LE }")

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ RGBA, GRAY8, GRAY16_BE, GRAY16_LE }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstAvgFrames, gst_avg_frames, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_avg_frames_debug_category, "avgframes", 0,
  "debug category for avgframes element"));

static void
gst_avg_frames_class_init (GstAvgFramesClass * klass)
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
      "Frame averager", "Generic", "A plugin for averaging multiple frames",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_avg_frames_set_property;
  gobject_class->get_property = gst_avg_frames_get_property;

  g_object_class_install_property (gobject_class, PROP_NO_OF_FRAMES,
      g_param_spec_int ("frameno", "Frames",
          "Number of frames to average together", MIN_FRAME_NO, MAX_FRAME_NO,
          DEFAULT_FRAME_NO, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  gobject_class->finalize = gst_avg_frames_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_avg_frames_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_avg_frames_stop);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_avg_frames_set_info);
  video_filter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (gst_avg_frames_transform_frame_ip);
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR (gst_avg_frames_sink_event);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_avg_frames_src_event);
}

static void
gst_avg_frames_init (GstAvgFrames *avgframes)
{
  avgframes->frame_no = DEFAULT_FRAME_NO;
}

void
gst_avg_frames_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (object);

  GST_DEBUG_OBJECT (avgframes, "set_property");

  switch (property_id) {
    case PROP_NO_OF_FRAMES:
      avgframes->frame_no = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_avg_frames_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (object);

  GST_DEBUG_OBJECT (avgframes, "get_property");

  switch (property_id) {
    case PROP_NO_OF_FRAMES:
      g_value_set_int(value, avgframes->frame_no);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void release(FrameSums *framesums)
{
	if (framesums->data) {
		free (framesums->data);
		framesums->data = 0;
		framesums->size = 0;
	}
}

void
gst_avg_frames_finalize (GObject * object)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (object);

  GST_DEBUG_OBJECT (avgframes, "finalize");

  release(&avgframes->framesums);

  G_OBJECT_CLASS (gst_avg_frames_parent_class)->finalize (object);
}

static gboolean
gst_avg_frames_start (GstBaseTransform * trans)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (trans);
  avgframes->frame_counter = 0;

  return TRUE;
}

static gboolean
gst_avg_frames_stop (GstBaseTransform * trans)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (trans);

  GST_DEBUG_OBJECT (avgframes, "stop");
  release(&avgframes->framesums);

  return TRUE;
}

static gboolean
gst_avg_frames_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (filter);

  GST_DEBUG ("caps %" GST_PTR_FORMAT
      "othercaps %" GST_PTR_FORMAT, incaps, outcaps);

  release(&avgframes->framesums);

  //gsize frameSize = in_info->width*in_info->height*in_info->finfo->n_components;

  avgframes->framesums.size = in_info->size/(in_info->finfo->bits/8);
  avgframes->framesums.data = (guint32 *) calloc(avgframes->framesums.size, sizeof(guint32));

  GST_DEBUG_OBJECT (avgframes, "set_info");

  return TRUE;
}

static void
gst_avg_frames_clear_avg(GstBaseTransform *trans)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (trans);
  guint32 *sum = avgframes->framesums.data;

  // If it has been deallocated by a stop and the event comes afterwards
  if (!sum) return;

  int i;
  for (i=0; i<avgframes->framesums.size; i++) {
    sum[i] = 0;
  }
  avgframes->frame_counter = 0;
}

static GstFlowReturn
gst_avg_frames_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  GstAvgFrames *avgframes = GST_AVG_FRAMES (filter);
  guint32 *sum = avgframes->framesums.data;
  int i;
  guint16 val;

  union {
    guint8  *d8;
    guint16 *d16;
    gpointer ptr;
  } data;

  data.ptr = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);

  if(frame->info.finfo->bits == 8) {

    for (i=0; i<avgframes->framesums.size; i++) {
      sum[i] += data.d8[i];
    }
    avgframes->frame_counter++;

    if (avgframes->frame_counter < avgframes->frame_no) {
      return GST_BASE_TRANSFORM_FLOW_DROPPED;
    } else {

      for (i=0; i<avgframes->framesums.size; i++) {
        data.d8[i] = (guint8) (sum[i]/avgframes->frame_no);
        sum[i] = 0;
      }
      avgframes->frame_counter = 0;

      return GST_FLOW_OK;
    }
  }
  else if (frame->info.finfo->bits == 16) {
    if (frame->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE){

      for (i=0; i<avgframes->framesums.size; i++) {
        sum[i] += data.d16[i];
      }
      avgframes->frame_counter++;

      if (avgframes->frame_counter < avgframes->frame_no) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
      } else {
        for (i=0; i<avgframes->framesums.size; i++) {
          data.d16[i] = (guint16) (sum[i]/avgframes->frame_no);
          sum[i] = 0;
        }
        avgframes->frame_counter = 0;

        return GST_FLOW_OK;
      }
    }
    else if (frame->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE){



      for (i=0; i<avgframes->framesums.size; i++) {
        sum[i] += __bswap_16(data.d16[i]);
      }
      avgframes->frame_counter++;

      if (avgframes->frame_counter < avgframes->frame_no) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
      } else {
        for (i=0; i<avgframes->framesums.size; i++) {
          val = (guint16) (sum[i]/avgframes->frame_no);
          data.d16[i] = __bswap_16(val);
          sum[i] = 0;
        }
        avgframes->frame_counter = 0;

        return GST_FLOW_OK;
      }
    }
    else {
      GST_ERROR("Unhandled format type");
      return GST_FLOW_ERROR;
    }
  }
  else {
    GST_ERROR("Unhandled data size of %d bits", frame->info.finfo->bits);
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_avg_frames_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  gboolean ret = TRUE;
  const GstStructure *s;
  const gchar *name;
  GST_DEBUG("Sink event %s",  gst_event_type_get_name (GST_EVENT_TYPE(event)));
  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_FLUSH_START:
      gst_avg_frames_clear_avg(trans);
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
      s = gst_event_get_structure (event);
      name = gst_structure_get_string(s, "name");
      if (name != NULL && strncmp(name, "qtec-flush", 10)==0){
        GST_DEBUG("FLUSH");
        gst_avg_frames_clear_avg(trans);
      }
      break;
    default:
      break;
  }

  ret &= GST_BASE_TRANSFORM_CLASS (gst_avg_frames_parent_class)->sink_event (
      trans, event);

  return ret;
}

static gboolean
gst_avg_frames_src_event (GstBaseTransform * trans, GstEvent * event)
{
  gboolean ret = TRUE;
  const GstStructure *s;
  const gchar *name;
  GST_DEBUG("Src event %s",  gst_event_type_get_name (GST_EVENT_TYPE(event)));
  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_FLUSH_START:
      gst_avg_frames_clear_avg(trans);
      break;
    case GST_EVENT_CUSTOM_UPSTREAM:
      s = gst_event_get_structure (event);
      name = gst_structure_get_string(s, "name");
      if (name != NULL && strncmp(name, "qtec-flush", 10)==0){
        GST_DEBUG("FLUSH");
        gst_avg_frames_clear_avg(trans);
      }
      break;
    default:
      break;
  }

  ret &= GST_BASE_TRANSFORM_CLASS (gst_avg_frames_parent_class)->src_event (
              trans, event);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{


  return gst_element_register (plugin, "avgframes", GST_RANK_NONE,
      GST_TYPE_AVG_FRAMES);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "Average Frames"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "AvgFrames"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    avgframes,
    "A plugin for averaging multiple frames",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

