/* GStreamer
 * Copyright (C) <2015> Dimitrios Katsaros <patcherwork@gmail.com>
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

#include <gsthistmeta.h>
#include <string.h>

GType
gst_hist_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("HistMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_hist_meta_init (GstMeta *meta, gpointer params, GstBuffer *buffer)
{
  GstHistMeta *m = (GstHistMeta *) meta;
  memset (m->bins, 0x0, sizeof(m->bins));
  m->bin_no = 0;
  m->minval = 0;
  m->maxval = 0;
  m->avgval = 0;
  m->medianid = 0;
  m->modeid = 0;

  return TRUE;
}

static void
gst_hist_meta_free (GstMeta *meta, GstBuffer *buffer)
{

}

static gboolean
gst_hist_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstHistMeta *m = (GstHistMeta *) meta;

  /*This is called when data is copied into a new buffer so to maintain the metadata
    it needs to be set on the new buffer */
  gst_buffer_add_gst_hist_meta (transbuf, m->bins, m->bin_no,
      m->minval, m->maxval, m->avgval, m->medianid, m->modeid);

  return TRUE;
}

const GstMetaInfo *
gst_hist_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_HIST_META_API_TYPE,
        GST_HIST_META_IMPL_NAME,
        sizeof (GstHistMeta),
        gst_hist_meta_init,
        gst_hist_meta_free,
        gst_hist_meta_transform);
    g_once_init_leave (&meta_info, mi);
  }
  return meta_info;
}

GstHistMeta *
gst_buffer_add_gst_hist_meta (GstBuffer * buffer, guint * abins, guint bin_no,
    guint8 minval, guint8 maxval, gdouble avgval, gint medianid, gint modeid)
{
  GstHistMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (GstHistMeta *) gst_buffer_add_meta (buffer, GST_HIST_META_INFO, NULL);

  /* Perform operations and apply to meta object */
  memcpy (meta->bins, abins, sizeof (guint) * 256);
  meta->bin_no = bin_no;
  meta->minval = minval;
  meta->maxval = maxval;
  meta->avgval = avgval;
  meta->medianid = medianid;
  meta->modeid = modeid;

  return meta;
}
