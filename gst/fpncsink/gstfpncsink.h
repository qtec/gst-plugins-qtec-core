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

#ifndef _GST_FPNC_SINK_H_
#define _GST_FPNC_SINK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

G_BEGIN_DECLS

#define GST_TYPE_FPNC_SINK   (gst_fpnc_sink_get_type())
#define GST_FPNC_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FPNC_SINK,GstFpncSink))
#define GST_FPNC_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FPNC_SINK,GstFpncSinkClass))
#define GST_IS_FPNC_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FPNC_SINK))
#define GST_IS_FPNC_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FPNC_SINK))

typedef struct _GstFpncSink GstFpncSink;
typedef struct _GstFpncSinkClass GstFpncSinkClass;

struct _GstFpncSink
{
  GstVideoSink base_fpncsink;
  GstVideoInfo info;

  gint   *calc_errors;
  gint   *rolling_medians;
  gint   fpnc_no;
  gint   max_fpnc_no;

  gint fpnc_denominator;
  gint fpnc_min;
  gint fpnc_max;
  guint fpnc_elems;
  gint exposure_min;
  gint exposure_max;
  gint starting_exposure;

  gint exposure_range_min;
  gint exposure_range_max;
  gint exposure_range_step;
  gint exposure_last_val;

  gint dn_min;
  gint dn_max;

  gboolean skip_first;
  gboolean exposure_was_set;
  gboolean calibration_complete;
  gboolean throw_eos_flag;

  GString *filepath;
  GString *filename;
  gboolean write_to_file;
};

struct _GstFpncSinkClass
{
  GstVideoSinkClass base_fpncsink_class;
};

GType gst_fpnc_sink_get_type (void);

G_END_DECLS

#endif
