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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_VIDEOHISTOGRAM_H_
#define _GST_VIDEOHISTOGRAM_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEOHISTOGRAM   (gst_videohistogram_get_type())
#define GST_VIDEOHISTOGRAM(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEOHISTOGRAM,GstVideohistogram))
#define GST_VIDEOHISTOGRAM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEOHISTOGRAM,GstVideohistogramClass))
#define GST_IS_VIDEOHISTOGRAM(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEOHISTOGRAM))
#define GST_IS_VIDEOHISTOGRAM_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEOHISTOGRAM))


typedef enum {
  GST_VIDEO_HISTOGRAM_BINS_2,
  GST_VIDEO_HISTOGRAM_BINS_4,
  GST_VIDEO_HISTOGRAM_BINS_8,
  GST_VIDEO_HISTOGRAM_BINS_16,
  GST_VIDEO_HISTOGRAM_BINS_32,
  GST_VIDEO_HISTOGRAM_BINS_64,
  GST_VIDEO_HISTOGRAM_BINS_128,
  GST_VIDEO_HISTOGRAM_BINS_256
} GstVideoHistogramBinNo;

typedef struct _GstVideohistogram GstVideohistogram;
typedef struct _GstVideohistogramClass GstVideohistogramClass;

struct _GstVideohistogram
{
  GstVideoFilter base_videohistogram;
  unsigned int bin_no;
  GstVideoHistogramBinNo bin_enum_val;
};

struct _GstVideohistogramClass
{
  GstVideoFilterClass base_videohistogram_class;
};

GType gst_videohistogram_get_type (void);

gboolean gst_videohistogram_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
