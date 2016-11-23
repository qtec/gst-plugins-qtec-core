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

#ifndef _GST_AVG_FRAMES_H_
#define _GST_AVG_FRAMES_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_AVG_FRAMES   (gst_avg_frames_get_type())
#define GST_AVG_FRAMES(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVG_FRAMES,GstAvgFrames))
#define GST_AVG_FRAMES_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVG_FRAMES,GstAvgFramesClass))
#define GST_IS_AVG_FRAMES(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVG_FRAMES))
#define GST_IS_AVG_FRAMES_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVG_FRAMES))

typedef struct _GstAvgFrames GstAvgFrames;
typedef struct _GstAvgFramesClass GstAvgFramesClass;

typedef struct FrameSums
{
	guint32 *data;
	gsize size;
}FrameSums;

struct _GstAvgFrames
{
  GstVideoFilter base_avgframes;
  gint frame_no;
  gint frame_counter;
  FrameSums framesums;
};

struct _GstAvgFramesClass
{
  GstVideoFilterClass base_avgframes_class;
};

GType gst_avg_frames_get_type (void);

G_END_DECLS

#endif
