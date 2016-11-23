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

#include <gstfpncmagicmeta.h>
#include <string.h>
#include <stdlib.h>

GType
gst_fpnc_magic_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("FPNCMagicMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

gboolean
gst_fpnc_magic_meta_init (GstMeta *meta, gpointer params, GstBuffer *buffer)
{
  GstFpncMagicMeta *m = (GstFpncMagicMeta *) meta;
  //GST_WARNING("fpncm init");

  m->rolling_median = NULL;
  m->error = NULL;
  m->data_size = 0;
  m->avg = 0;

  return TRUE;
}

static gboolean
gst_fpnc_magic_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstFpncMagicMeta *m = (GstFpncMagicMeta *) meta;

  /*This is called when data is copied into a new buffer so to maintain the metadata
    it needs to be set on the new buffer */
  //GST_WARNING("fpncm trans");
  gst_buffer_add_gst_fpnc_magic_meta (transbuf, m->rolling_median, 
    m->error, m->data_size, m->avg);

  return TRUE;
}

void
gst_fpnc_magic_meta_free (GstMeta *meta, GstBuffer *buffer)
{
  GstFpncMagicMeta *m = (GstFpncMagicMeta *) meta;
  //GST_WARNING("fpncm free meta %p", m);
  if (m->rolling_median)
    free (m->rolling_median);
  m->rolling_median = NULL;
  if (m->error)
    free (m->error);
  m->error = NULL;
  m->data_size = 0;
  m->avg = 0;
}

const GstMetaInfo *
gst_fpnc_magic_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_FPNC_MAGIC_META_API_TYPE,
        GST_FPNC_MAGIC_META_IMPL_NAME,
        sizeof (GstFpncMagicMeta),
        gst_fpnc_magic_meta_init,
        gst_fpnc_magic_meta_free,
        gst_fpnc_magic_meta_transform);
    g_once_init_leave (&meta_info, mi);
  }
  return meta_info;
}

GstFpncMagicMeta *
gst_buffer_add_gst_fpnc_magic_meta (GstBuffer * buffer, gpointer rolling_median,
  gpointer error, guint data_size, gint avg)
{
  GstFpncMagicMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (rolling_median, NULL);
  g_return_val_if_fail (error, NULL);

  meta = (GstFpncMagicMeta *) gst_buffer_add_meta (buffer, GST_FPNC_MAGIC_META_INFO, NULL);

  
  meta->data_size = data_size;
  meta->rolling_median = malloc(data_size * sizeof(gint));
  g_return_val_if_fail (meta->rolling_median, NULL);
  memcpy(meta->rolling_median, rolling_median, data_size * sizeof(gint));
  meta->error = malloc(data_size * sizeof(gint));
  if (!meta->error){
    free(meta->rolling_median);
    return NULL;
  }
  memcpy(meta->error, error, data_size * sizeof(gint));
  meta->avg = avg;
  //GST_WARNING("meta addr %p", meta);
  return meta;
}
