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
 * SECTION:element-gstavgrow
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <byteswap.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstavgrow.h"

GST_DEBUG_CATEGORY_STATIC (gst_avgrow_debug_category);
#define GST_CAT_DEFAULT gst_avgrow_debug_category

/* prototypes */


static void gst_avgrow_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_avgrow_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_avgrow_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_avgrow_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstCaps *gst_avgrow_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * otherCaps);
static GstCaps *gst_avgrow_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

enum
{
  PROP_0,
  PROP_NO_OF_ROWS,
  PROP_TOTAL_AVG
};

/* pad templates */

#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ RGBA, GRAY8, GRAY16_BE, GRAY16_LE }")

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ RGBA, GRAY8, GRAY16_BE, GRAY16_LE }")

#define MIN_ROWS 1
#define MAX_ROWS G_MAXINT
#define DEFAULT_ROWS 2

#define DEFAULT_PROP_TOTAL_AVG FALSE

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstAvgrow, gst_avgrow, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_avgrow_debug_category, "avgrow", 0,
  "debug category for avgrow element"));

static void
gst_avgrow_class_init (GstAvgrowClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Average row of column intensities", "Generic", "Averages colums, of data, creating a row of averages",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_avgrow_set_property;
  gobject_class->get_property = gst_avgrow_get_property;


  g_object_class_install_property (gobject_class, PROP_NO_OF_ROWS,
      g_param_spec_int ("norows", "Rows",
          "Number of rows to average together", MIN_ROWS, MAX_ROWS,
          DEFAULT_ROWS, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TOTAL_AVG,
      g_param_spec_boolean ("total-avg", "Total Average",
          "Calculates the total average of every column", DEFAULT_PROP_TOTAL_AVG,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_avgrow_fixate_caps);
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_avgrow_transform_caps);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_avgrow_set_info);
  video_filter_class->transform_frame = GST_DEBUG_FUNCPTR (gst_avgrow_transform_frame);

}

static void
gst_avgrow_init (GstAvgrow *avgrow)
{
  avgrow->rows = DEFAULT_ROWS;
}

void
gst_avgrow_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvgrow *avgrow = GST_AVGROW (object);

  GST_DEBUG_OBJECT (avgrow, "set_property");

  switch (property_id) {
    case PROP_TOTAL_AVG:
      avgrow->total_avg = g_value_get_boolean (value);
      break;
    case PROP_NO_OF_ROWS:
      avgrow->rows = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_avgrow_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvgrow *avgrow = GST_AVGROW (object);

  GST_DEBUG_OBJECT (avgrow, "get_property");

  switch (property_id) {
    case PROP_TOTAL_AVG:
      g_value_set_boolean(value, avgrow->total_avg);
      break;
    case PROP_NO_OF_ROWS:
      g_value_set_int(value, avgrow->rows);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GstCaps *
gst_avgrow_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstAvgrow *avgrow = GST_AVGROW (base);
  GstStructure *ins, *outs;
  gint from_w, from_h, to_w, to_h;

  GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  gst_structure_get_int (ins, "width", &from_w);
  gst_structure_get_int (ins, "height", &from_h);

  gst_structure_get_int (outs, "width", &to_w);
  gst_structure_get_int (outs, "height", &to_h);

  /* if rows is larger than from_h do not set the caps */
  if (!(from_h/avgrow->rows) && (!avgrow->total_avg))
    return othercaps;

  /*cases for source and sink negotiation*/
  if (direction == GST_PAD_SINK) {
    /*is the othercaps height is a range then we try to negotiate it correctly
      otherwise we cant really do anything*/
    if (gst_structure_has_field_typed(outs, "height", GST_TYPE_INT_RANGE)) {
      if (avgrow->total_avg)
        gst_structure_set(outs, "height", G_TYPE_INT, 1, NULL);
      else
        gst_structure_set(outs, "height", G_TYPE_INT, from_h / avgrow->rows, NULL);
    }

  }
  else {
    if (gst_structure_has_field_typed(outs, "height", GST_TYPE_INT_RANGE)) {
      if (!(avgrow->total_avg))
        gst_structure_set(outs, "height", G_TYPE_INT, from_h * avgrow->rows, NULL);
    }
  }

  gst_structure_set_value (outs, "format",
      gst_structure_get_value (ins, "format"));

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);
  return othercaps;
}

static GstCaps *
gst_avgrow_transform_caps (GstBaseTransform * base,
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

static gboolean
gst_avgrow_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstAvgrow *avgrow = GST_AVGROW (filter);

  if (avgrow->total_avg) {
    if (out_info->height!=1) {
      GST_ERROR("Unable to set output frame to height == 1 in total average mode");
      return FALSE;
    }
  }
  else if ((in_info->width != out_info->width) ||
        (in_info->height != (out_info->height * avgrow->rows))) {
    GST_ERROR("Input height is not larger than output by a factor of %d, input:%d, output:%d",
        avgrow->rows, in_info->height, out_info->height);
      return FALSE;
  }

  GST_DEBUG ("Final caps set");

  GST_DEBUG ("in caps %" GST_PTR_FORMAT " out caps %" GST_PTR_FORMAT, incaps,
      outcaps);

  return TRUE;
}


static GstFlowReturn
gst_avgrow_transform_frame (GstVideoFilter * filter, GstVideoFrame * inframe,
    GstVideoFrame * outframe)
{
  GstAvgrow *avgrow = GST_AVGROW (filter);

  int i, j, z;
  guint64 sum;
  guint16 val;
  gint outwidth = outframe->info.width;
  guint n_comps = outframe->info.finfo->n_components;
  gint in_step = GST_VIDEO_FRAME_PLANE_STRIDE(inframe,0)/(inframe->info.finfo->bits/8);
  gint out_step = GST_VIDEO_FRAME_PLANE_STRIDE(outframe,0)/(outframe->info.finfo->bits/8);
  union {
    guint8  *d8;
    guint16 *d16;
    gpointer ptr;
  } indata, outdata;


  indata.ptr = GST_VIDEO_FRAME_PLANE_DATA (inframe, 0);
  outdata.ptr = GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);


  if (outframe->info.finfo->bits == 8) {
    if (avgrow->total_avg) {
      for (i=0; i < outwidth*n_comps; i++) {
        sum = 0;
        for (z=0; z<inframe->info.height; z++) {
          sum +=  indata.d8[i + z*in_step];
        }
        sum /= inframe->info.height;
        outdata.d8[i] = sum;
      }
    } else {
      if (inframe->info.height == (outframe->info.height * avgrow->rows)) {

        for (i=0; i < outwidth*n_comps; i++) {
          for (j=0; j < outframe->info.height; j++) {
            sum = 0;
            for (z=0; z<avgrow->rows; z++)
              sum +=  indata.d8[i + j*in_step*avgrow->rows + z*in_step];
            sum /= avgrow->rows;
            outdata.d8[i + (j*out_step)] = sum;
          }
        }

      } else {
        GST_ERROR("Input height is not larger than output by a factor of %d, input:%d, output:%d",
          avgrow->rows, inframe->info.height, outframe->info.height);
        return GST_FLOW_ERROR;
      }
    }
  }
  else if (outframe->info.finfo->bits == 16) {
    if (inframe->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE) {
      if (avgrow->total_avg) {
        for (i=0; i < outwidth*n_comps; i++) {
          sum = 0;
          for (z=0; z<inframe->info.height; z++)
            sum +=  indata.d16[i + z*in_step];
          sum /= inframe->info.height;
          outdata.d16[i] = sum;
        }
      } else {
        if (inframe->info.height == (outframe->info.height * avgrow->rows)) {

          for (i=0; i < outwidth*n_comps; i++) {
            for (j=0; j < outframe->info.height; j++) {
              sum = 0;
              for (z=0; z<avgrow->rows; z++)
                sum +=  indata.d16[i + j*in_step*avgrow->rows + z*in_step];
              sum /= avgrow->rows;
              outdata.d16[i + (j*out_step)] = sum;
            }
          }

        } else {
          GST_ERROR("Input height is not larger than output by a factor of %d, input:%d, output:%d",
            avgrow->rows, inframe->info.height, outframe->info.height);
          return GST_FLOW_ERROR;
        }
      }
    } else if (inframe->info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE) {
      if (avgrow->total_avg) {
        for (i=0; i < outwidth*n_comps; i++) {
          sum = 0;
          for (z=0; z<inframe->info.height; z++)
            sum +=  __bswap_16(indata.d16[i + z*in_step]);
          sum /= inframe->info.height;
          val = sum;
          outdata.d16[i] = __bswap_16(val);
        }
      } else {
        if (inframe->info.height == (outframe->info.height * avgrow->rows)) {

          for (i=0; i < outwidth*n_comps; i++) {
            for (j=0; j < outframe->info.height; j++) {
              sum = 0;
              for (z=0; z<avgrow->rows; z++)
                sum +=  __bswap_16(indata.d16[i + j*in_step*avgrow->rows + z*in_step]);
              sum /= avgrow->rows;
              val = sum;
              outdata.d16[i + (j*out_step)] = __bswap_16(val);
            }
          }

        } else {
          GST_ERROR("Input height is not larger than output by a factor of %d, input:%d, output:%d",
            avgrow->rows, inframe->info.height, outframe->info.height);
          return GST_FLOW_ERROR;
        }
      }
    }
    else {
      GST_ERROR("Unhandled format type");
      return GST_FLOW_ERROR;
    }
  }
  else {
    GST_ERROR("Unhandled data size of %d bits", outframe->info.finfo->bits);
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "avgrow", GST_RANK_NONE,
      GST_TYPE_AVGROW);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "qtec"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "row avg"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    avgrow,
    "average row of columns",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

