/* GStreamer
 * Copyright (C) 2015 FIXME <fixme@example.com>
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

#ifndef _GST_V4L2SWEEP_H_
#define _GST_V4L2SWEEP_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

G_BEGIN_DECLS

#define GST_TYPE_V4L2SWEEP   (gst_v4l2sweep_get_type())
#define GST_V4L2SWEEP(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2SWEEP,GstV4l2sweep))
#define GST_V4L2SWEEP_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2SWEEP,GstV4l2sweepClass))
#define GST_IS_V4L2SWEEP(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2SWEEP))
#define GST_IS_V4L2SWEEP_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2SWEEP))

typedef struct _GstV4l2sweep GstV4l2sweep;
typedef struct _GstV4l2sweepClass GstV4l2sweepClass;

typedef struct ParamQuality
{
	int paramVal;
	double error;
}ParamQuality;

struct _GstV4l2sweep
{
	GstVideoSink base_v4l2sweep;

	//properties
	gchar controlName[32];

	int bestValue;
	double modePercentage;

	int sweepStep;
	int sweepMin;
	int sweepMax;
	gboolean setMin;
	gboolean setMax;

	int paramVal;
	int minParamVal;
	int maxParamVal;

	ParamQuality bestParam;

	GstVideoInfo info;

	gboolean firstRun;
	GstBuffer *prev_buf;
};

struct _GstV4l2sweepClass
{
  GstVideoSinkClass base_v4l2sweep_class;
};

GType gst_v4l2sweep_get_type (void);

G_END_DECLS

#endif
