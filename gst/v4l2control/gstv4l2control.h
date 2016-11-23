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

#ifndef _GST_V4L2_CONTROL_H_
#define _GST_V4L2_CONTROL_H_

#include <gst/base/gstbasetransform.h>
#include "gstv4l2utils.h"


G_BEGIN_DECLS

#define GST_TYPE_V4L2_CONTROL   (gst_v4l2_control_get_type())
#define GST_V4L2_CONTROL(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_CONTROL,GstV4l2Control))
#define GST_V4L2_CONTROL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_CONTROL,GstV4l2ControlClass))
#define GST_IS_V4L2_CONTROL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_CONTROL))
#define GST_IS_V4L2_CONTROL_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_CONTROL))

typedef struct _GstV4l2Control GstV4l2Control;
typedef struct _GstV4l2ControlClass GstV4l2ControlClass;

struct _GstV4l2Control
{
  GstBaseTransform base_v4l2control;
  gchar videoDevice[32];

  gboolean drop_on_update;
  GstClockTime last_update;
  gboolean flushing, drop_extra_frame,
    perfrom_extra_frame_drop;

  V4l2ControlList v4l2ControlList;
};

struct _GstV4l2ControlClass
{
  GstBaseTransformClass base_v4l2control_class;
};

GType gst_v4l2_control_get_type (void);

gboolean
gst_v4l2_control_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
