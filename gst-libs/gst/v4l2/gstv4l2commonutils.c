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

#include "gstv4l2commonutils.h"

GValue *
gst_v4l2_new_value_array(GValue *value, gconstpointer data_array, guint array_length, guint element_size)
{
    GValue *val2;
    GArray *arr;

    g_return_val_if_fail(value, NULL);
    g_return_val_if_fail(data_array, NULL);

    val2 = g_value_init (value, G_TYPE_ARRAY);
    arr = g_array_new (FALSE, FALSE, element_size);
    g_array_append_vals(arr, data_array, array_length);
    g_value_take_boxed (value, arr);
    return val2;
}

GValue *
gst_v4l2_new_string_array(GValue *value, const gchar * str)
{
    GValue *val2;
    GString *arr;

    g_return_val_if_fail(value, NULL);
    g_return_val_if_fail(str, NULL);

    val2 = g_value_init (value, G_TYPE_STRING);
    arr = g_string_new(str);
    g_value_take_boxed (value, arr);
    return val2;
}

gpointer *
gst_v4l2_get_boxed_from_value(GValue *value)
{
    g_return_val_if_fail((G_VALUE_TYPE(value)==G_TYPE_ARRAY ||
        G_VALUE_TYPE(value)==G_TYPE_GSTRING), NULL);
    return g_value_get_boxed(value);
}