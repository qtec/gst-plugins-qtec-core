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
 * SECTION:element-gstvideohistogram
 *
 * The videohistogram element calculates the histogram for an input image
 * and adds it to the videoframe from which it was generated as metadata
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! vhist binno=5 ! drawhist ! glimagesink
 * ]|
 * Creates a test image, calculates the histogram for the generated image and
 * presents it
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstvideohistogram.h"
#include <gst/histogram/gsthistmeta.h>

GST_DEBUG_CATEGORY_STATIC (gst_videohistogram_debug_category);
#define GST_CAT_DEFAULT gst_videohistogram_debug_category

/* prototypes */


static void gst_videohistogram_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_videohistogram_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_videohistogram_transform_frame_ip (GstVideoFilter *
    filter, GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_BIN_NO
};

/* pad templates */

#define VIDEO_SRC_CAPS \
		GST_VIDEO_CAPS_MAKE("{ GRAY8 }")

#define VIDEO_SINK_CAPS \
		GST_VIDEO_CAPS_MAKE("{ GRAY8 }")


#define DEF_BIN_NO GST_VIDEO_HISTOGRAM_BINS_256


#define GST_TYPE_VIDEOHISTOGRAM_PATTERN (gst_videohistogram_pattern_get_type ())
static GType
gst_videohistogram_pattern_get_type (void)
{
  static GType videohistogram_pattern_type = 0;
  static const GEnumValue pattern_types[] = {
    {GST_VIDEO_HISTOGRAM_BINS_2, "2 bins", "bins_2"},
    {GST_VIDEO_HISTOGRAM_BINS_4, "4 bins", "bins_4"},
    {GST_VIDEO_HISTOGRAM_BINS_8, "8 bins", "bins_8"},
    {GST_VIDEO_HISTOGRAM_BINS_16, "16 bins", "bins_16"},
    {GST_VIDEO_HISTOGRAM_BINS_32, "32 bins", "bins_32"},
    {GST_VIDEO_HISTOGRAM_BINS_64, "64 bins", "bins_64"},
    {GST_VIDEO_HISTOGRAM_BINS_128, "128 bins", "bins_128"},
    {GST_VIDEO_HISTOGRAM_BINS_256, "256 bins", "bins_256"},
    {0, NULL, NULL}
  };

  if (!videohistogram_pattern_type) {
    videohistogram_pattern_type =
        g_enum_register_static ("GstVideoHistogramPattern", pattern_types);
  }
  return videohistogram_pattern_type;
}


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVideohistogram, gst_videohistogram,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_videohistogram_debug_category,
        "videohistogram", 0, "debug category for videohistogram element"));

static void
gst_videohistogram_class_init (GstVideohistogramClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);


  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "video histogram", "Transform/Metadata/Video",
      "Append histogram data as metadata to stream",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gobject_class->set_property = gst_videohistogram_set_property;
  gobject_class->get_property = gst_videohistogram_get_property;

  g_object_class_install_property (gobject_class, PROP_BIN_NO,
      g_param_spec_enum ("binno", "bin_no",
          "Number of bins", GST_TYPE_VIDEOHISTOGRAM_PATTERN,
          DEF_BIN_NO, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_videohistogram_transform_frame_ip);

}

static void
gst_videohistogram_set_bin_no (GstVideohistogram * videohistogram,
    int bin_no_enum)
{
    videohistogram->bin_enum_val = bin_no_enum;
    switch(bin_no_enum) {
      case GST_VIDEO_HISTOGRAM_BINS_2:
        videohistogram->bin_no = 2;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_4:
        videohistogram->bin_no = 4;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_8:
        videohistogram->bin_no = 8;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_16:
        videohistogram->bin_no = 16;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_32:
        videohistogram->bin_no = 32;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_64:
        videohistogram->bin_no = 64;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_128:
        videohistogram->bin_no = 128;
        break;
      case GST_VIDEO_HISTOGRAM_BINS_256:
        videohistogram->bin_no = 256;
        break;
      default:
        g_assert_not_reached ();
    }
}

static void
gst_videohistogram_init (GstVideohistogram * videohistogram)
{
   gst_videohistogram_set_bin_no(videohistogram,
        DEF_BIN_NO);
}


void
gst_videohistogram_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideohistogram *videohistogram = GST_VIDEOHISTOGRAM (object);
  GST_DEBUG_OBJECT (videohistogram, "set_property");

  switch (property_id) {
    case PROP_BIN_NO:
      gst_videohistogram_set_bin_no(videohistogram, g_value_get_enum(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_videohistogram_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideohistogram *videohistogram = GST_VIDEOHISTOGRAM (object);

  GST_DEBUG_OBJECT (videohistogram, "get_property");

  switch (property_id) {
    case PROP_BIN_NO:
      g_value_set_enum (value, videohistogram->bin_enum_val);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* transform */

static GstFlowReturn
gst_videohistogram_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstVideohistogram *videohistogram = GST_VIDEOHISTOGRAM (filter);

  gsize i;
  gint bin, modeid, medianid;
  guint bin_no = videohistogram->bin_no;
  gsize size = GST_VIDEO_FRAME_SIZE(frame);
  guint8 maxval = 0;
  guint8 minval = 255;
  gdouble avgval;

  uint32_t avgtotal = 0;


  guint8 *data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  guint bins[256] = { 0 };      /* trick to initialize array to 0 */
  for (i = 0; i < size; i++) {
    bin = data[i] >> (8 - __builtin_ctz(bin_no));
    bins[bin]++;
    if (maxval < data[i])
      maxval = data[i];
    if (minval > data[i])
      minval = data[i];
    avgtotal += data[i];
  }

  avgval = (gdouble)avgtotal/size;

  /* find median and mode */

  //TODO: print message if more than one mode found?
  modeid = 0;
  medianid = -1;
  uint32_t acc = 0;
  for (i = 0; i < bin_no; i++) {
	//mode is the bin with the highest amount of pixels
    if (bins[modeid] < bins[i])
      modeid = i;

    //median is the bin in which the accumulated nr pixels reaches half of the total
    acc += bins[i];
    if(medianid == -1 && acc >= size/2)
    	medianid = i;
  }

  gst_buffer_add_gst_hist_meta (frame->buffer, bins, bin_no,
      minval, maxval, avgval, medianid, modeid);

  GST_DEBUG ("number of bins: %d min val: %d avg val: %f max val: %d median val: %d (bin nr: %d) mode val: %d (bin nr: %d)\n",
		  	  bin_no, minval, avgval, maxval,
			  medianid*(256/bin_no), medianid,
			  modeid*(256/bin_no), modeid);

  return GST_FLOW_OK;
}


gboolean
gst_videohistogram_plugin_init (GstPlugin * plugin)
{

  GST_DEBUG_CATEGORY_INIT (gst_videohistogram_debug_category, "vhist", 0,
      "debugging for histogram generator");

  return gst_element_register (plugin, "vhist", GST_RANK_NONE,
      GST_TYPE_VIDEOHISTOGRAM);
}
