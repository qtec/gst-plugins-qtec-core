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


#include "gstv4l2Queries.h"

#define WARN_IF_FAIL(exp,msg) if(G_UNLIKELY(!(exp))){g_warning("%s",(msg));}

#define GST_V4L2_QUERY_HAS_TYPE(query,query_type) \
	(gst_v4l2_queries_get_type (query) == GST_V4L2_QUERY_ ## query_type)

Gstv4l2QueryType
get_v4l2_query_type_from_name(const gchar *type)
{
	if (g_str_equal (type, GST_V4L2_QUERY_SET_CONTROL_NAME))
		return GST_V4L2_QUERY_SET_CONTROL;
	else if (g_str_equal (type, GST_V4L2_QUERY_CONTROL_INFO_NAME))
		return GST_V4L2_QUERY_CONTROL_INFO;
	if (g_str_equal (type, GST_V4L2_QUERY_GET_CONTROL_NAME))
		return GST_V4L2_QUERY_GET_CONTROL;

	return GST_V4L2_QUERY_INVALID;
}

Gstv4l2QueryType
gst_v4l2_queries_get_type(GstQuery *query)
{
	const GstStructure *s;
	const gchar *type;

	if (query == NULL || (GST_QUERY_TYPE(query) != GST_QUERY_CUSTOM) )
		return GST_V4L2_QUERY_INVALID;

	s = gst_query_get_structure(query);
	if (s == NULL || !gst_structure_has_name (s, GST_V4L2_QUERY_NAME))
		return GST_V4L2_QUERY_INVALID;

	type = gst_structure_get_string(s, "type");
	if (type == NULL)
		return GST_V4L2_QUERY_INVALID;

	return get_v4l2_query_type_from_name(type);
}


gboolean
gst_v4l2_queries_parse_set_control(GstQuery * query,
	const gchar **name, gint *id, const GValue ** val)
{
	const GstStructure *s;
	gboolean ret = TRUE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		SET_CONTROL), FALSE);


	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = FALSE;
	}
	if (id) {
		if(gst_structure_get_int(s, "id", id));
			ret = FALSE;
	}
	if (val) {
		*val = gst_structure_get_value(s, "value");
		if (*val == NULL)
			ret = FALSE;
	}

	WARN_IF_FAIL (ret, "Couldn't extract details from set control query");

	return ret;
}

void
gst_v4l2_queries_set_set_control(GstQuery * query, const GValue * val )
{
	GstStructure *s;

	g_return_if_fail (GST_V4L2_QUERY_HAS_TYPE (query, SET_CONTROL));

	s = gst_query_writable_structure (query);
	gst_structure_set_value (s, "value", val);
}

void
gst_v4l2_queries_set_control_activate_flushing(GstQuery * query)
{
	GstStructure *s;

	g_return_if_fail (GST_V4L2_QUERY_HAS_TYPE (query, SET_CONTROL));

	s = gst_query_writable_structure (query);
	gst_structure_set (s, "flush", G_TYPE_BOOLEAN, TRUE, NULL);
}

gboolean
gst_v4l2_queries_set_control_is_flushing(GstQuery * query)
{
	const GstStructure *s;
	gboolean ret = FALSE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		SET_CONTROL), FALSE);

	s = gst_query_get_structure (query);
	if(!gst_structure_get_boolean(s, "flush", &ret))
		ret = FALSE;
	return ret;

}

GstQuery*
gst_v4l2_queries_new_set_control(gchar *name,
		gint id, GValue * val)
{
	GstStructure *s = gst_structure_new (GST_V4L2_QUERY_NAME,
				"name", G_TYPE_STRING, name,
				"id", G_TYPE_INT, id,
				"type", G_TYPE_STRING, GST_V4L2_QUERY_SET_CONTROL_NAME,
				"flush", G_TYPE_BOOLEAN, FALSE,
				NULL);
	gst_structure_set_value (s, "value", val);
	return gst_query_new_custom (GST_QUERY_CUSTOM, s);
}

gboolean
gst_v4l2_queries_parse_get_control(GstQuery * query,
	const gchar **name, gint *id, const GValue ** val)
{
	const GstStructure *s;
	gboolean ret = TRUE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		GET_CONTROL), FALSE);


	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name == NULL)
			ret = FALSE;
	}
	if (id) {
		if(!gst_structure_get_int(s, "id", id))
			ret = FALSE;
	}
	if (val) {
		*val = gst_structure_get_value(s, "value");
		if (*val == NULL)
			ret = FALSE;
	}

	WARN_IF_FAIL (ret, "Couldn't extract details from get control query");

	return ret;
}

void
gst_v4l2_queries_set_get_control(GstQuery * query, gchar *name, gint id, const GValue * val )
{
	GstStructure *s;

	g_return_if_fail (GST_V4L2_QUERY_HAS_TYPE (query, GET_CONTROL));

	s = gst_query_writable_structure (query);
	gst_structure_set (s,
		"name", G_TYPE_STRING, name,
		"id", G_TYPE_INT, id,
		NULL);
	gst_structure_set_value (s, "value", val);
}

GstQuery*
gst_v4l2_queries_new_get_control(gchar *name,
		gint id)
{
	GstStructure *s = gst_structure_new (GST_V4L2_QUERY_NAME,
				"name", G_TYPE_STRING, name,
				"id", G_TYPE_INT, id,
				"type", G_TYPE_STRING, GST_V4L2_QUERY_GET_CONTROL_NAME,
				NULL);
	return gst_query_new_custom (GST_QUERY_CUSTOM, s);
}


gboolean
gst_v4l2_queries_parse_control_info(GstQuery * query,
	const gchar **name, gint *id, gint *min, gint *max,
	guint *flags, gint *defaut_val, gint *step)
{
	const GstStructure *s;
	gboolean ret = TRUE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		CONTROL_INFO), FALSE);

	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = FALSE;
	}
	if (id)
		ret &= gst_structure_get_int(s, "id", id);
	if (min)
		ret &= gst_structure_get_int(s, "min", min);
	if (max)
		ret &= gst_structure_get_int(s, "max", max);
	if (flags)
		ret &= gst_structure_get_uint(s, "flags", flags);
	if (defaut_val)
		ret &= gst_structure_get_int(s, "default_value", defaut_val);
	if (step)
		ret &= gst_structure_get_int(s, "step", step);

	WARN_IF_FAIL (ret, "Couldn't extract details from control info query");

	return ret;
}

void
gst_v4l2_queries_set_control_info(GstQuery * query, gchar *name, gint id,
	gint min, gint max, guint flags, gint defaut_val, gint step)
{
	GstStructure *s;

	g_return_if_fail (GST_V4L2_QUERY_HAS_TYPE (query, CONTROL_INFO));

	s = gst_query_writable_structure (query);
	gst_structure_set (s,
	 	"name", G_TYPE_STRING, name,
	 	"id", G_TYPE_INT, id,
		"min", G_TYPE_INT, min,
		"max", G_TYPE_INT, max,
		"flags", G_TYPE_UINT, flags,
		"default_value", G_TYPE_INT, defaut_val,
		"step", G_TYPE_INT, step,
		"extended", G_TYPE_BOOLEAN, FALSE,
		NULL);
}

gboolean
gst_v4l2_queries_parse_control_extended_info(GstQuery * query,
	const gchar **name, gint *id, gint *min, gint *max,
	guint *flags, gint *defaut_val, gint *step, guint *elem_size,
    guint *elems, guint *nr_of_dims, const GValue **dims)
{
	const GstStructure *s;
	gboolean ret = TRUE;
	gboolean isextended;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		CONTROL_INFO), FALSE);

	s = gst_query_get_structure (query);

	g_return_val_if_fail (gst_structure_get_boolean(s,
		"extended", &isextended), FALSE);

	if (!isextended) {
		GST_DEBUG("query is not from an extended control");
		return FALSE;
	}

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = FALSE;
	}
	if (id)
		ret &= gst_structure_get_int(s, "id", id);
	if (min)
		ret &= gst_structure_get_int(s, "min", min);
	if (max)
		ret &= gst_structure_get_int(s, "max", max);
	if (flags)
		ret &= gst_structure_get_uint(s, "flags", flags);
	if (defaut_val)
		ret &= gst_structure_get_int(s, "default_value", defaut_val);
	if (step)
		ret &= gst_structure_get_int(s, "step", step);
	if (elem_size)
		ret &= gst_structure_get_uint(s, "elem_size", elem_size);
	if (elems)
		ret &= gst_structure_get_uint(s, "elems", elems);
	if (nr_of_dims)
		ret &= gst_structure_get_uint(s, "nr_of_dims", nr_of_dims);
	if (dims) {
		*dims = gst_structure_get_value(s, "dims");
		if (*dims == NULL)
			ret = FALSE;
	}

	WARN_IF_FAIL (ret, "Couldn't extract details from extended control info query");

	return ret;
}

void

gst_v4l2_queries_set_control_extended_info(GstQuery * query, gchar *name, gint id,
	gint min, gint max, guint flags, gint defaut_val, gint step, guint elem_size,
    guint elems, guint nr_of_dims, GValue * dims)
{
	GstStructure *s;

	g_return_if_fail (GST_V4L2_QUERY_HAS_TYPE (query, CONTROL_INFO));

	s = gst_query_writable_structure (query);
	gst_structure_set (s,
	 	"name", G_TYPE_STRING, name,
	 	"id", G_TYPE_INT, id,
		"min", G_TYPE_INT, min,
		"max", G_TYPE_INT, max,
		"flags", G_TYPE_UINT, flags,
		"default_value", G_TYPE_INT, defaut_val,
		"step", G_TYPE_INT, step,
		"extended", G_TYPE_BOOLEAN, TRUE,
		"elem_size", G_TYPE_UINT, elem_size,
		"elems", G_TYPE_UINT, elems,
		"nr_of_dims", G_TYPE_UINT, nr_of_dims,
		NULL);
	gst_structure_set_value (s, "dims", dims);
}

gboolean
gst_v4l2_queires_is_extended_control_info(GstQuery *query)
{
	const GstStructure *s;
	gboolean isextended;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		CONTROL_INFO), FALSE);

	s = gst_query_get_structure (query);

	g_return_val_if_fail (gst_structure_get_boolean(s,
		"extended", &isextended), FALSE);

	return isextended;
}

GstQuery*
gst_v4l2_queries_new_control_info(gchar *name, gint id)
{
	GstStructure *s = gst_structure_new (GST_V4L2_QUERY_NAME,
				"name", G_TYPE_STRING, name,
				"id", G_TYPE_INT, id,
				"type", G_TYPE_STRING, GST_V4L2_QUERY_CONTROL_INFO_NAME,
				NULL);
	return gst_query_new_custom (GST_QUERY_CUSTOM, s);
}



/* additional "internal" functions */


gboolean
gst_v4l2_queries_parse_reciever_set_control(GstQuery * query,
	const gchar **name, gint *id, const GValue ** val)
{
	const GstStructure *s;
	gboolean ret = FALSE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		SET_CONTROL), FALSE);


	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = TRUE;
	}
	if (id) {
		if(gst_structure_get_int(s, "id", id));
			ret = TRUE;
	}
	if (val) {
		*val = gst_structure_get_value(s, "value");
		if (*val == NULL)
			ret = FALSE;
	}


	WARN_IF_FAIL (ret, "Couldn't extract details from set control query");

	return ret;
}


gboolean
gst_v4l2_queries_parse_reciever_get_control(GstQuery * query,
	const gchar **name, gint *id, const GValue ** val)
{
	const GstStructure *s;
	gboolean ret = FALSE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		GET_CONTROL), FALSE);


	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = TRUE;
	}
	if (id) {
		if(gst_structure_get_int(s, "id", id));
			ret = TRUE;
	}
	if (val) {
		*val = gst_structure_get_value(s, "value");
		if (*val == NULL)
			ret = FALSE;
	}


	WARN_IF_FAIL (ret, "Couldn't extract details from get control query");

	return ret;
}


gboolean
gst_v4l2_queries_parse_reciever_control_info(GstQuery * query,
	const gchar **name, gint *id, gint *min, gint *max,
	guint *flags, gint *defaut_val, gint *step)
{
	const GstStructure *s;
	gboolean ret = FALSE;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		CONTROL_INFO), FALSE);

	s = gst_query_get_structure (query);

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = TRUE;
	}
	if (id) {
		if (gst_structure_get_int(s, "id", id))
			ret = TRUE;
	}
	if (min)
		ret &= gst_structure_get_int(s, "min", min);
	if (max)
		ret &= gst_structure_get_int(s, "max", max);
	if (flags)
		ret &= gst_structure_get_uint(s, "flags", flags);
	if (defaut_val)
		ret &= gst_structure_get_int(s, "default_value", defaut_val);
	if (step)
		ret &= gst_structure_get_int(s, "step", step);

	WARN_IF_FAIL (ret, "Couldn't extract details from control info query");

	return ret;
}


gboolean
gst_v4l2_queries_parse_reciever_control_extended_info(GstQuery * query,
	const gchar **name, gint *id, gint *min, gint *max,
	guint *flags, gint *defaut_val, gint *step, guint *elem_size,
    guint *elems, guint *nr_of_dims, const GValue **dims)
{
	const GstStructure *s;
	gboolean ret = FALSE;
	gboolean isextended;

	g_return_val_if_fail (GST_V4L2_QUERY_HAS_TYPE (query,
		CONTROL_INFO), FALSE);

	s = gst_query_get_structure (query);

	g_return_val_if_fail (gst_structure_get_boolean(s,
		"extended", &isextended), FALSE);

	if (!isextended) {
		GST_DEBUG("query is not from an extended control");
		return FALSE;
	}

	if (name) {
		*name = gst_structure_get_string(s, "name");
		if (*name != NULL)
			ret = TRUE;
	}
	if (id) {
		if (gst_structure_get_int(s, "id", id))
			ret = TRUE;
	}
	if (min)
		ret &= gst_structure_get_int(s, "min", min);
	if (max)
		ret &= gst_structure_get_int(s, "max", max);
	if (flags)
		ret &= gst_structure_get_uint(s, "flags", flags);
	if (defaut_val)
		ret &= gst_structure_get_int(s, "default_value", defaut_val);
	if (step)
		ret &= gst_structure_get_int(s, "step", step);
	if (elem_size)
		ret &= gst_structure_get_uint(s, "elem_size", elem_size);
	if (elems)
		ret &= gst_structure_get_uint(s, "elems", elems);
	if (nr_of_dims)
		ret &= gst_structure_get_uint(s, "nr_of_dims", nr_of_dims);
	if (dims) {
		*dims = gst_structure_get_value(s, "dims");
		if (*dims == NULL)
			ret = FALSE;
	}

	WARN_IF_FAIL (ret, "Couldn't extract details from extended control info query");

	return ret;
}