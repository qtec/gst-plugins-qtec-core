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


#ifndef __GST_V4L2_QUERIES_H__
#define __GST_V4L2_QUERIES_H__

#include <gst/gst.h>

#define GST_V4L2_QUERY_NAME "v4l2_query"

#define GST_V4L2_QUERY_SET_CONTROL_NAME "v4l2-set-control"
#define GST_V4L2_QUERY_GET_CONTROL_NAME "v4l2-get-control"
#define GST_V4L2_QUERY_CONTROL_INFO_NAME "v4l2-control-info"

typedef enum {
  GST_V4L2_QUERY_INVALID = 0,
  GST_V4L2_QUERY_SET_CONTROL,
  GST_V4L2_QUERY_CONTROL_INFO,
  GST_V4L2_QUERY_GET_CONTROL
} Gstv4l2QueryType;

/* helper function for retrieving the type of a query */
Gstv4l2QueryType gst_v4l2_queries_get_type(GstQuery *query);


/* SET_CONTROL */

/* function used to retrieve values from set control query
 * used by both the sender and the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_set_control(GstQuery * query,
    const gchar **name, gint *id, const GValue ** val);


/*
 * Used by receiver to set the results of the query
 */
void gst_v4l2_queries_set_set_control(GstQuery * query, const GValue * val);

/*
 * Used by the sender to create a new set control query
 */
GstQuery* gst_v4l2_queries_new_set_control (gchar *name,
    gint id, GValue * val);

/*
 * helper function for determining if flushing should occure
 */
gboolean gst_v4l2_queries_set_control_is_flushing(GstQuery * query);

/*
 * helper function for activating flushing with set control query
 */
void gst_v4l2_queries_set_control_activate_flushing(GstQuery * query);


/* GET_CONTROL */

/* function used to retrieve values from get control query
 * used by both the sender and the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_get_control(GstQuery * query,
    const gchar **name, gint *id, const GValue ** val);

/*
 * Used by receiver to set the results of the query
 */
void gst_v4l2_queries_set_get_control(GstQuery * query, gchar *name, gint id, const GValue * val);

/*
 * Used by the sender to create a new set control query
 */
GstQuery* gst_v4l2_queries_new_get_control (gchar *name,
    gint id);




/* CONTROL_INFO */

/* function used to retrieve values from control info query.
 * Used by both the sender and the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_control_info(GstQuery * query,
    const gchar **name, gint *id, gint *min, gint *max,
    guint *flags, gint *defaut_val, gint *step);

/*
 * Used by receiver to set the results of the query
 */
void gst_v4l2_queries_set_control_info(GstQuery * query, gchar *name, gint id,
    gint min, gint max, guint flags, gint defaut_val, gint step);


/* function used to retrieve values from extended control info query.
 * Used by both the sender and the receiver. Unwanted values can
 * be left NULL. Will fail if the received query is not an extended query
 */
gboolean gst_v4l2_queries_parse_control_extended_info(GstQuery * query,
    const gchar **name, gint *id, gint *min, gint *max,
    guint *flags, gint *defaut_val, gint *step, guint *elem_size,
    guint *elems, guint *nr_of_dims, const GValue **dims);


/*
 * Used by receiver to set the results of the extended api query
 */

void gst_v4l2_queries_set_control_extended_info(GstQuery * query, gchar *name,
    gint id, gint min, gint max, guint flags, gint defaut_val, gint step,
    guint elem_size, guint elems, guint nr_of_dims, GValue * dims);

/*
 * Utility function for checking if a query is from the extended api
 */

gboolean gst_v4l2_queires_is_extended_control_info(GstQuery *query);

/*
 * Used by the sender to create a new set control query
 */
GstQuery* gst_v4l2_queries_new_control_info (gchar *name, gint id);




/*
 * Reciever specific "internal" functions
 * placing here to not confuse the user.
 * these fucnctions performe correct checks for name and id
 */

 /* function used to retrieve values from set control query
 * used by the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_reciever_set_control(GstQuery * query,
    const gchar **name, gint *id, const GValue ** val);

/* function used to retrieve values from get control query
 * used by both the sender and the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_reciever_get_control(GstQuery * query,
    const gchar **name, gint *id, const GValue ** val);

/* function used to retrieve values from control info query.
 * Used by both the sender and the receiver. Unwanted values can
 * be left NULL.
 */
gboolean gst_v4l2_queries_parse_reciever_control_info(GstQuery * query,
    const gchar **name, gint *id, gint *min, gint *max,
    guint *flags, gint *defaut_val, gint *step);

/* function used to retrieve values from extended control info query.
 * Used by both the sender and the receiver. Unwanted values can
 * be left NULL. Will fail if the received query is not an extended query
 */
gboolean gst_v4l2_queries_parse_reciever_control_extended_info(GstQuery * query,
    const gchar **name, gint *id, gint *min, gint *max,
    guint *flags, gint *defaut_val, gint *step, guint *elem_size,
    guint *elems, guint *nr_of_dims, const GValue **dims);


#endif /* _GST_V4L2_QUERIES_H_ */