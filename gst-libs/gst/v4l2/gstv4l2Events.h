/*
 * GStreamer
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

#ifndef __GST_V4L2_EVENTS_H__
#define __GST_V4L2_EVENTS_H__

#include <gst/gst.h>

#define GST_V4L2_EVENT_NAME "v4l2_event"

#define GST_V4L2_EVENT_SET_CONTROL_NAME "v4l2-set-control"

typedef enum {
  GST_V4L2_EVENT_INVALID = 0,
  GST_V4L2_EVENT_SET_CONTROL
} Gstv4l2EventType;

/* helper function for retrieving the type of an event */
Gstv4l2EventType gst_v4l2_events_get_type (GstEvent *event);

/* helper function for sending upstream events */
gboolean gst_v4l2_events_send_event_upstream (GstElement *ele, GstStructure * s);

/* function used by reciever to parse a recieved event */
gboolean gst_v4l2_events_parse_set_control (GstEvent * event,
    const gchar **name, gint *id, const GValue ** val, gboolean * flush);

/* function used by sender to send a new set control event */
void gst_v4l2_events_send_set_control (GstElement * ele,
    gchar *name, gint id, GValue * val, gboolean flush);


/* "internal" functions */


/* function used by reciever to parse a recieved event */
gboolean gst_v4l2_events_parse_reciever_set_control (GstEvent * event,
    const gchar **name, gint *id, const GValue ** val, gboolean * flush);

#endif /* __GST_V4L2_EVENTS_H__ */