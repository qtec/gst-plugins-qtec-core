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

/*
 * A set of utility functions for v4l2 events/queries
 */

#ifndef __GST_V4L2_COMMON_UTILS_H__
#define __GST_V4L2_COMMON_UTILS_H__

#include <gst/gst.h>

/*
 * Function for creating a new GValue containing an array.
 * The initialized GValue should be cleared with g_value_unset
 * to handle the internally handled array.
 */

GValue *
gst_v4l2_new_value_array(GValue *value, gconstpointer data_array,
    guint array_length, guint element_size);


GValue *
gst_v4l2_new_string_array(GValue *value, const gchar * str);

/*
 * Helper function for extracting the array from within a GValue
 */
gpointer *
gst_v4l2_get_boxed_from_value(GValue *value);


#endif