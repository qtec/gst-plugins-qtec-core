/* GStreamer
 * Copyright (C) 2015 Dimitrios Katsaros <patcherwork@gmail.com>

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

#ifndef _GST_AVGROW_H_
#define _GST_AVGROW_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_AVGROW   (gst_avgrow_get_type())
#define GST_AVGROW(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVGROW,GstAvgrow))
#define GST_AVGROW_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVGROW,GstAvgrowClass))
#define GST_IS_AVGROW(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVGROW))
#define GST_IS_AVGROW_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVGROW))

typedef struct _GstAvgrow GstAvgrow;
typedef struct _GstAvgrowClass GstAvgrowClass;

struct _GstAvgrow
{
  GstVideoFilter base_avgrow;

  gint format_data_number;
  gint rows;
  gboolean total_avg;
};

struct _GstAvgrowClass
{
  GstVideoFilterClass base_avgrow_class;
};

GType gst_avgrow_get_type (void);

G_END_DECLS

#endif
