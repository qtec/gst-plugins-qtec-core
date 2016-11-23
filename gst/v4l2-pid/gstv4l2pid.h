/* GStreamer
 * Copyright (C) 2014 Marianna S. Buschle <msb@qtec.com>
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

#ifndef _GST_V4L2PID_H_
#define _GST_V4L2PID_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

//PROPERTIES LIMITS

#define TARGET_VAL_MAX 254
#define TARGET_VAL_MIN 1
#define TARGET_VAL_DEF 127

#define MAX_NR_RUNS_DEF 0
#define CONTROL_NAME_DEF "exposure time, absolute"

#define DEFAULT_PIXEL_PERCENTAGE 10.0 //10%

#define SATURATION_VALUE 255

G_BEGIN_DECLS

#define GST_TYPE_V4L2PID   (gst_v4l2pid_get_type())
#define GST_V4L2PID(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2PID,GstV4l2PID))
#define GST_V4L2PID_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2PID,GstV4l2PIDClass))
#define GST_IS_V4L2PID(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2PID))
#define GST_IS_V4L2PID_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2PID))

typedef struct _GstV4l2PID GstV4l2PID;
typedef struct _GstV4l2PIDClass GstV4l2PIDClass;

enum TargetType{
	AVERAGE = 0,
	ABSOLUTE_MINIMUM = 1,
	ABSOLUTE_MAXIMUM = 2
};

typedef struct PIDControls
{
	//PID properties
	gdouble Kp;
	gdouble Ki;
	gdouble Kd;
	gdouble antiWindupMax; //anti integral wind-up
	guint controlStep; //fixed step to use instead of PID

	//local
	double prev_err;
	double integral;
}PIDControls;

struct _GstV4l2PID
{
  GstVideoSink base_v4l2pid;

  //properties
  gchar controlName[32];
  guchar targetValue;
  gdouble max_err;
  gboolean invertedControl;
  guint maxNRuns;
  gboolean stopWhenDone;
  PIDControls pidControls;
  int targetType;

  int paramVal;
  int minParamVal;
  int maxParamVal;

  gboolean firstRun;
  int nRuns;

  double last_err;
  char dirUp;
  char dirDown;
  int prev_step;

  GstBuffer *prev_buf;
};

struct _GstV4l2PIDClass
{
  GstVideoSinkClass base_v4l2pid_class;
};

GType gst_v4l2pid_get_type (void);

G_END_DECLS

#endif
