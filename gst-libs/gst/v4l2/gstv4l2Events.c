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

#include "gstv4l2Events.h"

#define WARN_IF_FAIL(exp,msg) if(G_UNLIKELY(!(exp))){g_warning("%s",(msg));}

#define GST_V4L2_EVENT_HAS_TYPE(event,event_type) \
  (gst_v4l2_events_get_type (event) == GST_V4L2_EVENT_ ## event_type)

Gstv4l2EventType
get_v4l2_event_type_from_name(const gchar *type)
{
  if (g_str_equal (type, GST_V4L2_EVENT_SET_CONTROL_NAME))
    return GST_V4L2_EVENT_SET_CONTROL;

  return GST_V4L2_EVENT_INVALID;
}

Gstv4l2EventType
gst_v4l2_events_get_type(GstEvent *event)
{
  const GstStructure *s;
  const gchar *type;

  if (event == NULL || (GST_EVENT_TYPE(event) != GST_EVENT_CUSTOM_UPSTREAM &&
    GST_EVENT_TYPE(event) != GST_EVENT_CUSTOM_DOWNSTREAM) )
    return GST_V4L2_EVENT_INVALID;

  s = gst_event_get_structure(event);
  if (s == NULL || !gst_structure_has_name (s, GST_V4L2_EVENT_NAME))
    return GST_V4L2_EVENT_INVALID;

  type = gst_structure_get_string(s, "type");
  if (type == NULL)
    return GST_V4L2_EVENT_INVALID;

  return get_v4l2_event_type_from_name(type);
}

gboolean
gst_v4l2_events_send_event_upstream (GstElement *ele, GstStructure * s)
{
  g_return_val_if_fail (s != NULL, FALSE);
  return gst_element_send_event (GST_ELEMENT(ele), gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s));
}

gboolean
gst_v4l2_events_parse_set_control(GstEvent * event, const gchar **name, gint *id,
  const GValue ** val, gboolean *flush)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_V4L2_EVENT_HAS_TYPE (event,
    SET_CONTROL), FALSE);


  s = gst_event_get_structure (event);

  if (name) {
    *name = gst_structure_get_string(s, "name");
    if (*name != NULL)
      ret = FALSE;
  }
  if (id) {
    if (gst_structure_get_int(s, "id", id))
      ret = FALSE;
  }
  if (flush) {
    if (gst_structure_get_boolean(s, "flush", flush))
      ret = TRUE;
  }

  if(val) {
    *val = gst_structure_get_value(s, "value");
    if (*val == NULL)
      ret = FALSE;
    else
      ret = TRUE;
  }


  WARN_IF_FAIL (ret, "Couldn't extract details from set control event");

  return ret;
}

void
gst_v4l2_events_send_set_control (GstElement * ele, gchar *name,
  gint id, GValue * val, gboolean flush)
{
  GstStructure *s = gst_structure_new (GST_V4L2_EVENT_NAME,
    "name", G_TYPE_STRING, name,
    "id", G_TYPE_INT, id,
    "flush", G_TYPE_BOOLEAN, flush,
    "type", G_TYPE_STRING, GST_V4L2_EVENT_SET_CONTROL_NAME,
    NULL);
  gst_structure_set_value (s, "value", val);
  gst_v4l2_events_send_event_upstream (ele, s);
}


gboolean
gst_v4l2_events_parse_reciever_set_control(GstEvent * event, const gchar **name, gint *id,
  const GValue ** val, gboolean *flush)
{
  const GstStructure *s;
  gboolean ret = FALSE;

  g_return_val_if_fail (GST_V4L2_EVENT_HAS_TYPE (event,
    SET_CONTROL), FALSE);


  s = gst_event_get_structure (event);

  if (name) {
    *name = gst_structure_get_string(s, "name");
    if (*name != NULL)
      ret = TRUE;
  }
  if (id) {
    if (gst_structure_get_int(s, "id", id))
      ret = TRUE;
  }

  if (flush) {
    if (gst_structure_get_boolean(s, "flush", flush))
      ret = TRUE;
  }

  if(val) {
    *val = gst_structure_get_value(s, "value");
    if (*val == NULL)
      ret = FALSE;
    else
      ret = TRUE;
  }


  WARN_IF_FAIL (ret, "Couldn't extract details from set control event");

  return ret;
}