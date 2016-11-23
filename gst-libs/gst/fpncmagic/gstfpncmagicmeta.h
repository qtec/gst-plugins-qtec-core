/*
 * GStreamer
 * Copyright (C) 2015 Dimitrios Katsaros <patcherwork@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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



#ifndef __GST_FPNC_MAGIC_META_H__
#define __GST_FPNC_MAGIC_META_H__

#include <gst/gst.h>

#define GST_FPNC_MAGIC_META_IMPL_NAME "FpncMagicMeta"

typedef struct _GstFpncMagicMeta GstFpncMagicMeta;

/*union arrayptr {
    guint8  *ui8;
    gint8   *i8;
    guint16 *ui16;
    gint16  *i16;
    gpointer ptr;
};*/

struct _GstFpncMagicMeta {
    GstMeta        meta;
    gint          *rolling_median;
    gint          *error;
    guint          data_size;
    gint           avg;
};


GType gst_fpnc_magic_meta_api_get_type (void);
#define GST_FPNC_MAGIC_META_API_TYPE (gst_fpnc_magic_meta_api_get_type())

#define gst_buffer_get_gst_fpnc_magic_meta(b) \
	((GstFpncMagicMeta*)gst_buffer_get_meta((b),GST_FPNC_MAGIC_META_API_TYPE))



const GstMetaInfo *gst_fpnc_magic_meta_get_info (void);
#define GST_FPNC_MAGIC_META_INFO (gst_fpnc_magic_meta_get_info())

GstFpncMagicMeta * gst_buffer_add_gst_fpnc_magic_meta (GstBuffer      *buffer,
                                                       gpointer        rolling_median,
                                                       gpointer        error,
                                                       guint           data_size,
                                                       gint            avg);



#endif /* __GST_FPNC_MAGIC_META_H__ */