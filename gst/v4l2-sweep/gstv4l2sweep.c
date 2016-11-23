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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstv4l2sweep
 *
 * The v4l2sweep element does a sweep through the range of the selected v4l2 control and chooses the best value.
 * It sends queries to a v4l2control element.
 * The definition of "best value" can be controlled by parameters.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 --gst-debug=v4l2sweep:5 v4l2src device=/dev/qt5023_video0 !
 * video/x-raw, format=GRAY16_BE, width=1024, height=544 !
 * videocrop top=400 left=200 bottom=44 right=224 !
 * v4l2control device=/dev/qt5023_video0 !
 * avgframes frameno=20 ! avgrow total-avg=1 ! fpncmagic ! v4l2sweep sweep-min=100
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "gstv4l2sweep.h"
#include <gst/v4l2/gstv4l2Queries.h>
#include <gst/fpncmagic/gstfpncmagicmeta.h>
#include <gst/histogram/gsthistmeta.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <byteswap.h>

GST_DEBUG_CATEGORY_STATIC (v4l2sweep_debug);
#define GST_CAT_DEFAULT v4l2sweep_debug

/* prototypes */


static void gst_v4l2sweep_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_v4l2sweep_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_v4l2sweep_dispose (GObject * object);
static void gst_v4l2sweep_finalize (GObject * object);

static gboolean gst_v4l2sweep_start(GstBaseSink *sink);
static gboolean gst_v4l2sweep_stop(GstBaseSink *sink);
static gboolean gst_v4l2_sweep_set_caps(GstBaseSink * sink, GstCaps * caps);

static GstFlowReturn gst_v4l2sweep_show_frame (GstVideoSink * video_sink,
    GstBuffer * buf);

enum
{
  PROP_0,
  PROP_V4L2_CONTROL_NAME,
  PROP_V4L2_BEST_VALUE,
  PROP_V4L2_SWEEP_MIN,
  PROP_V4L2_SWEEP_MAX,
  PROP_V4L2_SWEEP_STEP
};

enum{
	NONE = 1,
	MIN_COLUMN_DIFFERENCE = 2,
	MAX_AVG = 3,
	MIN_AVG = 4,
	MAX_MODE = 5,
	MIN_MODE = 6
};

#define MIN_MODE_PERCENTAGE 95.0

#define CONTROL_NAME_DEF "v ramp"

#define BEST_VALUE_DEF MIN_COLUMN_DIFFERENCE
#define SWEEP_STEP_DEF 1

/* pad templates */

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ GRAY8, GRAY16_BE, GRAY16_LE }")

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstV4l2sweep, gst_v4l2sweep, GST_TYPE_VIDEO_SINK,
  GST_DEBUG_CATEGORY_INIT (v4l2sweep_debug, "v4l2sweep", 0,
  "debug category for v4l2sweep element"));

static GType gst_v4l2sweep_best_value_get_type (void)
{
	static GType v4l2sweep_best_value_type = 0;

	static const GEnumValue v4l2sweep_best_value_types[] = {
	{NONE, "Don't look for best value, just sweep", "none"},
    {MIN_COLUMN_DIFFERENCE, "Minimize Column Difference", "min_col_diff"},
	{MAX_AVG, "Maximize Average", "max_avg"},
	{MIN_AVG, "Minimize Average", "min_avg"},
	{MAX_MODE, "Maximize Histogram Mode", "max_mode"},
	{MIN_MODE, "Minimize Histogram Mode", "min_mode"},
    {0, NULL, NULL},
	};

	if (!v4l2sweep_best_value_type)
		v4l2sweep_best_value_type =  g_enum_register_static ("GstV4l2SweepBestValueType", v4l2sweep_best_value_types);

	return v4l2sweep_best_value_type;
}

static void
gst_v4l2sweep_class_init (GstV4l2sweepClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "V4L2 Sweep", "Generic", "Sweep a specified v4l2 parameter in order to achieve the best value of a specified image property, requires a v4l2control element in the pipeline",
      "Marianna Smidth Buschle <msb@qtec.com>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_v4l2sweep_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_v4l2sweep_stop);
  gstbasesink_class->set_caps = gst_v4l2_sweep_set_caps;

  gobject_class->set_property = gst_v4l2sweep_set_property;
  gobject_class->get_property = gst_v4l2sweep_get_property;
  gobject_class->dispose = gst_v4l2sweep_dispose;
  gobject_class->finalize = gst_v4l2sweep_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_v4l2sweep_show_frame);

  GST_DEBUG_CATEGORY_INIT (v4l2sweep_debug, "v4l2sweep", 0, "V4L2 Sweep");

  // define properties
  g_object_class_install_property (gobject_class, PROP_V4L2_CONTROL_NAME,
		  g_param_spec_string("control-name", "control-name",
		  "The name of the v4l2 control to modify in order to achieve the best value",
		  CONTROL_NAME_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_V4L2_BEST_VALUE,
		  g_param_spec_enum ("best-value", "best-value",
		  "The definition of what is the best value",
		  gst_v4l2sweep_best_value_get_type(), BEST_VALUE_DEF,
		  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_V4L2_SWEEP_MIN,
		  g_param_spec_int("sweep-min", "sweep-min",
		  "Start the sweep from this value instead of the v4l2 control minimum",
		  INT_MIN, INT_MAX, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_V4L2_SWEEP_MAX,
		  g_param_spec_int("sweep-max", "sweep-max",
		  "Stop the sweep at this value instead of the v4l2 control maximum",
		  INT_MIN, INT_MAX, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_V4L2_SWEEP_STEP,
		  g_param_spec_uint("sweep-step", "sweep-step",
		  "Increment the v4l2 control by this much for each sweep step",
		  1, UINT_MAX, SWEEP_STEP_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_v4l2sweep_init (GstV4l2sweep *v4l2sweep)
{
	GST_DEBUG_OBJECT (v4l2sweep, "gst_v4l2sweep_init");

	//initialize properties with default values
	strncpy(v4l2sweep->controlName, CONTROL_NAME_DEF, 32);
	v4l2sweep->bestValue = BEST_VALUE_DEF;

	v4l2sweep->sweepStep = SWEEP_STEP_DEF;
	v4l2sweep->setMin = FALSE;
	v4l2sweep->setMax = FALSE;
	v4l2sweep->prev_buf = NULL;

	//setting here makes the value from gst-inspect get updated
	g_object_set (G_OBJECT (v4l2sweep), "max-lateness", -1, NULL);

	//for some reason it is not working for show-preroll-frame
	g_object_set (G_OBJECT (v4l2sweep), "show-preroll-frame", 0, NULL);

	//gst_util_set_object_arg(G_OBJECT (v4l2sweep), "show-preroll-frame", "0");
	//g_object_set_property(object, property_name, value)
}

static gboolean
gst_v4l2sweep_start(GstBaseSink *sink)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (sink);

	GST_DEBUG_OBJECT (v4l2sweep, "gst_v4l2sweep_start");

	v4l2sweep->firstRun = TRUE;
	v4l2sweep->modePercentage = 0;

	//but works here
	//note that then it doesn't reflect the default value in gst-inspect
	g_object_set (G_OBJECT (v4l2sweep), "show-preroll-frame", 0, NULL);

	return TRUE;
}

static gboolean
gst_v4l2sweep_stop(GstBaseSink *sink)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (sink);

	GST_DEBUG_OBJECT (v4l2sweep, "gst_v4l2sweep_stop");

	return TRUE;
}

static gboolean
gst_v4l2_sweep_set_caps(GstBaseSink * sink, GstCaps * caps)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (sink);
	GstVideoInfo info;

	GST_DEBUG("Caps info: %" GST_PTR_FORMAT, caps);

	if (!gst_video_info_from_caps (&info, caps)){
		GST_ELEMENT_ERROR (v4l2sweep, STREAM, FAILED, ("Invalid caps"), (NULL));
		return FALSE;
	}

	v4l2sweep->info = info;

	return TRUE;
}

void
gst_v4l2sweep_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (object);

	GST_DEBUG_OBJECT (v4l2sweep, "set_property id=%d", property_id);

	switch (property_id) {
		case PROP_V4L2_CONTROL_NAME:
			strncpy(v4l2sweep->controlName, g_value_get_string(value), 32);
			GST_INFO_OBJECT (v4l2sweep, "V4L2 Control name is %s\n", v4l2sweep->controlName);
			break;
		case PROP_V4L2_BEST_VALUE:
			v4l2sweep->bestValue = g_value_get_enum(value);
			GEnumClass* enumclass = g_type_class_ref(gst_v4l2sweep_best_value_get_type());
			GEnumValue* v = g_enum_get_value(enumclass, v4l2sweep->bestValue);
			GST_INFO_OBJECT (v4l2sweep, "Best value is %d = %s\n", v4l2sweep->bestValue, v->value_name);
			g_type_class_unref(enumclass);
			break;
		case PROP_V4L2_SWEEP_MIN:
			v4l2sweep->sweepMin = g_value_get_int(value);
			v4l2sweep->setMin = TRUE;
			GST_INFO_OBJECT (v4l2sweep, "The sweep min is %d\n", v4l2sweep->sweepMin);
			break;
		case PROP_V4L2_SWEEP_MAX:
			v4l2sweep->sweepMax = g_value_get_int(value);
			v4l2sweep->setMax = TRUE;
			GST_INFO_OBJECT (v4l2sweep, "The sweep max is %d\n", v4l2sweep->sweepMax);
			break;
		case PROP_V4L2_SWEEP_STEP:
			v4l2sweep->sweepStep = g_value_get_uint(value);
			GST_INFO_OBJECT (v4l2sweep, "The sweep step is %d\n", v4l2sweep->sweepStep);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

void
gst_v4l2sweep_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (object);

	GST_DEBUG_OBJECT (v4l2sweep, "get_property id=%d", property_id);

	switch (property_id) {
		case PROP_V4L2_CONTROL_NAME:
			g_value_set_string(value, v4l2sweep->controlName);
			break;
		case PROP_V4L2_BEST_VALUE:
			g_value_set_enum (value, v4l2sweep->bestValue);
			break;
		case PROP_V4L2_SWEEP_MIN:
			g_value_set_int(value, v4l2sweep->sweepMin);
			break;
		case PROP_V4L2_SWEEP_MAX:
			g_value_set_int(value, v4l2sweep->sweepMax);
			break;
		case PROP_V4L2_SWEEP_STEP:
			g_value_set_uint(value, v4l2sweep->sweepStep);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

void
gst_v4l2sweep_dispose (GObject * object)
{
  GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (object);

  GST_DEBUG_OBJECT (v4l2sweep, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_v4l2sweep_parent_class)->dispose (object);
}

void
gst_v4l2sweep_finalize (GObject * object)
{
  GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (object);

  GST_DEBUG_OBJECT (v4l2sweep, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_v4l2sweep_parent_class)->finalize (object);
}

GstFlowReturn sendParamSetQuery(GstV4l2sweep *v4l2sweep, int value)
{
	GST_DEBUG_OBJECT (v4l2sweep, "sendParamSetQuery value=%d", value);

	int ret = GST_FLOW_OK;

	int old_paramVal = v4l2sweep->paramVal;

	//make set control query
	GValue gv = G_VALUE_INIT;
	g_value_init(&gv, G_TYPE_INT);
	g_value_set_int(&gv, value);
	GstQuery* query = gst_v4l2_queries_new_set_control(v4l2sweep->controlName, 0, &gv);
	gst_v4l2_queries_set_control_activate_flushing(query); //flush avg_frames
	g_value_unset(&gv);

	//send query through pad
	gboolean res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2sweep), query);

	//parse response
	if(res){
		GValue* ptr;
		gst_v4l2_queries_parse_set_control(query, NULL, NULL, (const GValue **)&ptr);
		//update paramVal
		v4l2sweep->paramVal = g_value_get_int(ptr);
	}else{
		GST_ELEMENT_ERROR (v4l2sweep, RESOURCE, FAILED, ("Set control query failed for control:%s val=%d", v4l2sweep->controlName, value), (NULL));
		ret = GST_FLOW_ERROR;
	}
	gst_query_unref (query);

	if(v4l2sweep->paramVal != value){
		GST_DEBUG_OBJECT (v4l2sweep, "Couldn't change parameter value = %d to %d, achieved %d", old_paramVal, value, v4l2sweep->paramVal);
	}

	return ret;
}

GstFlowReturn sendParamChangeQuery(GstV4l2sweep *v4l2sweep, int value)
{
	GST_DEBUG_OBJECT (v4l2sweep, "sendParamChangeQuery value=%d", value);

	int ret = GST_FLOW_OK;

	int old_paramVal = v4l2sweep->paramVal;

	//test limits
	if(v4l2sweep->paramVal == v4l2sweep->maxParamVal && value>0){
		GST_ELEMENT_ERROR (v4l2sweep, RESOURCE, FAILED, ("Reached maximum parameter value = %d", v4l2sweep->maxParamVal), (NULL));
		return GST_FLOW_ERROR;
	}
	if(v4l2sweep->paramVal == v4l2sweep->minParamVal && value<0){
		GST_ELEMENT_ERROR (v4l2sweep, RESOURCE, FAILED, ("Reached minimum parameter value = %d", v4l2sweep->minParamVal), (NULL));
		return GST_FLOW_ERROR;
	}

	if(v4l2sweep->paramVal+value > v4l2sweep->maxParamVal){
		value = v4l2sweep->maxParamVal;
	}else if(v4l2sweep->paramVal+value < v4l2sweep->minParamVal){
		value = v4l2sweep->minParamVal;
	}else{
		value += v4l2sweep->paramVal;
	}

	ret = sendParamSetQuery(v4l2sweep, value);

	if(ret != GST_FLOW_OK)
		return ret;

	if(v4l2sweep->paramVal == old_paramVal){
		if(value == v4l2sweep->maxParamVal && value > v4l2sweep->paramVal){
			GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("Can't change parameter value by requested step (%d->%d) and already at the limit, aborting", old_paramVal, value), (NULL));
			return GST_FLOW_CUSTOM_ERROR_1;
		}else if(value == v4l2sweep->minParamVal && value < v4l2sweep->paramVal){
			GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("Can't change parameter value by requested step (%d->%d) and already at the limit, aborting", old_paramVal, value), (NULL));
			return GST_FLOW_CUSTOM_ERROR_1;
		}else{
			GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("Can't change parameter value by requested step (%d->%d), increasing step", old_paramVal, value), (NULL));
			return GST_FLOW_CUSTOM_ERROR_2;
		}
	}

	return ret;
}

GstFlowReturn increaseParam(GstV4l2sweep *v4l2sweep, int val)
{
	GstFlowReturn ret = sendParamChangeQuery(v4l2sweep, val);

	while(ret != GST_FLOW_OK){
		if(ret == GST_FLOW_ERROR)
			return ret;
		else if(ret == GST_FLOW_CUSTOM_ERROR_2){
			val++;
			ret = sendParamChangeQuery(v4l2sweep, val);
		}else if(ret == GST_FLOW_CUSTOM_ERROR_1){
			return ret;
		}
	}

	return ret;
}

//Calculates column difference
//Uses the normalized RMS of the difference between neighbor pixels:
//sqrt( sum( (cols[i]-cols[i+1])² )/n )/avg
double colDiff(GstV4l2sweep *v4l2sweep, GstBuffer * buf)
{
	GstVideoFrame frame;
	GstMapFlags flags = GST_MAP_READ;

	double diff = 0;
	double temp = 0;
	double avg = 0;
	int i;

	union {
		guint8  *d8;
		guint16 *d16;
		gpointer ptr;
	} data;

	if (gst_video_frame_map (&frame, &v4l2sweep->info, buf, flags)){
		if(v4l2sweep->info.height > 1){
			GST_ELEMENT_ERROR (v4l2sweep, STREAM, FAILED, ("When calculating column difference the image height must be 1"), (NULL));
			return 0;
		}

		data.ptr = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
		for (i=0; i<v4l2sweep->info.width-1; i++) {
			//calc avg intensity
			if(frame.info.finfo->bits == 8) {
				avg += data.d8[i];
			}else if (frame.info.finfo->bits == 16) {
				if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE){
					avg += data.d16[i];
				}else if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE){
					avg += __bswap_16(data.d16[i]);
				}
			}

			//calc neighbor difference
			if(frame.info.finfo->bits == 8) {
				temp = abs(data.d8[i] - data.d8[i+1]);
			}else if (frame.info.finfo->bits == 16) {
				if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE){
					temp = abs(data.d16[i] - data.d16[i+1]);
				}else if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE){
					temp = abs(__bswap_16(data.d16[i]) - __bswap_16(data.d16[i+1]));
				}
			}

			//calc RMS of neighbor difference
			diff += temp*temp;
		}

		if(frame.info.finfo->bits == 8) {
			avg += data.d8[i];
		}else if (frame.info.finfo->bits == 16) {
			if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE){
				avg += data.d16[i];
			}else if (frame.info.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE){
				avg += __bswap_16(data.d16[i]);
			}
		}
		avg /= v4l2sweep->info.width;

		//RMS
		diff /= (v4l2sweep->info.width-1);
		diff = sqrt(diff*10000); //multiply by 10000 to make the final number bigger (100x), more precision

		//normalize
		diff /= avg;

		gst_video_frame_unmap (&frame);
	}else{
		GST_ELEMENT_ERROR (v4l2sweep, STREAM, FAILED, ("Invalid video buffer received"), (NULL));
	}

	GST_DEBUG_OBJECT (v4l2sweep, "colDiff = %f avg = %f param = %d", diff, avg, v4l2sweep->paramVal);

	return diff;
}

double colDiffMagic(GstV4l2sweep *v4l2sweep, GstFpncMagicMeta* meta)
{
	double diff = 0;
	double temp = 0;
	int i;

	for (i=0; i<meta->data_size-1; i++) {
		//calc neighbor difference
		temp = abs(meta->error[i] - meta->error[i+1]);

		//calc RMS of neighbor difference
		diff += temp*temp;
	}

	//RMS
	diff /= (meta->data_size-1);
	diff = sqrt(diff*10000); //multiply by 10000 to make the final number bigger (100x), more precision

	//FIXME: is this necessary?
	//normalize
	diff /= meta->avg;

	GST_DEBUG_OBJECT (v4l2sweep, "colDiff = %f avg = %d param = %d", diff, meta->avg, v4l2sweep->paramVal);

	return diff;
}

gboolean getControlInfo(GstV4l2sweep *v4l2sweep)
{
	GST_DEBUG_OBJECT (v4l2sweep, "getControlInfo");

	gboolean ret = TRUE;

	//query control info
	GstQuery* query = gst_v4l2_queries_new_control_info (v4l2sweep->controlName, 0);
	gboolean res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2sweep), query);

	//parse response
	if(res){
		gst_v4l2_queries_parse_control_info(query, NULL, NULL, &v4l2sweep->minParamVal, &v4l2sweep->maxParamVal, NULL, NULL, NULL);
	}else{
		GST_ERROR_OBJECT (v4l2sweep, "Get control info query failed for control:%s", v4l2sweep->controlName);
		gst_query_unref (query);
		return FALSE;
	}
	gst_query_unref (query);

	//query control current value
	query = gst_v4l2_queries_new_get_control(v4l2sweep->controlName, 0);

	//send query through pad
	res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2sweep), query);

	//parse response
	if(res){
		GValue* ptr;
		gst_v4l2_queries_parse_get_control(query, NULL, NULL, (const GValue **)&ptr);
		//update paramVal
		v4l2sweep->paramVal = g_value_get_int(ptr);
		GST_INFO_OBJECT(v4l2sweep, "Get control value query control:%s val=%d", v4l2sweep->controlName, v4l2sweep->paramVal);
	}else{
		GST_ERROR_OBJECT (v4l2sweep, "Get control value query failed for control:%s", v4l2sweep->controlName);
		ret = FALSE;
	}
	gst_query_unref (query);

	return ret;
}

static GstFlowReturn
gst_v4l2sweep_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
	GstV4l2sweep *v4l2sweep = GST_V4L2SWEEP (sink);

	GST_DEBUG_OBJECT (v4l2sweep, "show_frame");

	//Hack for T#643, check that we are not getting repeated buffers
	if(buf == v4l2sweep->prev_buf){
		GST_DEBUG_OBJECT (v4l2sweep, "Rejecting frame, identical buffer pointer");
		return GST_FLOW_OK;
	}
	v4l2sweep->prev_buf = buf;

	//hack to deal with start function from v4l2pid running before v4l2control
	if(v4l2sweep->firstRun){
		v4l2sweep->firstRun = FALSE;
		if(!getControlInfo(v4l2sweep)){
			GST_ELEMENT_ERROR (v4l2sweep, RESOURCE, FAILED, ("Error getting v4l2 control info"), (NULL));
			return GST_FLOW_ERROR;
		}

		if(v4l2sweep->setMin){
			v4l2sweep->setMin = FALSE;
			if(v4l2sweep->sweepMin < v4l2sweep->minParamVal || v4l2sweep->sweepMin > v4l2sweep->maxParamVal){
				int temp;
				if(v4l2sweep->sweepMin < v4l2sweep->minParamVal)
					temp = v4l2sweep->minParamVal;
				else
					temp = v4l2sweep->maxParamVal;
				GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("Warning, invalid sweep min requested (%d), must be between: %d and %d. Using %d", v4l2sweep->sweepMin, v4l2sweep->minParamVal, v4l2sweep->maxParamVal, temp), (NULL));
				v4l2sweep->sweepMin = temp;
			}
			v4l2sweep->minParamVal = v4l2sweep->sweepMin;
		}

		if(v4l2sweep->setMax){
			v4l2sweep->setMax = FALSE;
			if(v4l2sweep->sweepMax < v4l2sweep->minParamVal || v4l2sweep->sweepMax > v4l2sweep->maxParamVal){
				int temp;
				if(v4l2sweep->sweepMax < v4l2sweep->minParamVal)
					temp = v4l2sweep->minParamVal;
				else
					temp = v4l2sweep->maxParamVal;
				GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("Warning, invalid sweep max requested (%d), must be between: %d and %d. Using %d", v4l2sweep->sweepMax, v4l2sweep->minParamVal, v4l2sweep->maxParamVal, temp), (NULL));
				v4l2sweep->sweepMax = temp;
			}
			v4l2sweep->maxParamVal = v4l2sweep->sweepMax;
		}

		if(v4l2sweep->minParamVal > v4l2sweep->maxParamVal){
			GST_ELEMENT_ERROR(v4l2sweep, RESOURCE, FAILED, ("Warning, incompatible sweep min and max requested (%d > %d).", v4l2sweep->minParamVal, v4l2sweep->maxParamVal), (NULL));
			return GST_FLOW_ERROR;
		}

		return sendParamSetQuery(v4l2sweep, v4l2sweep->minParamVal);
	}

	double modePercentage = 0;
	const GstMetaInfo *info = NULL;
	switch(v4l2sweep->bestValue){
		case NONE:
		{
			info = gst_meta_get_info("HistMeta");
			GstHistMeta * meta = NULL;
			if (info)
				meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
			if (meta) {
				GST_DEBUG_OBJECT (v4l2sweep, "param: %d min: %d avg: %f max: %d median: %d mode: %d\n", v4l2sweep->paramVal,
						meta->minval, meta->avgval, meta->maxval, meta->medianid*(256/meta->bin_no), meta->modeid*(256/meta->bin_no));
			}
			break;
		}

		case MIN_COLUMN_DIFFERENCE:
		{
			info = gst_meta_get_info("FpncMagicMeta");
			GstFpncMagicMeta * meta = NULL;
			if (info)
				meta = (GstFpncMagicMeta *) gst_buffer_get_meta(buf, info->api);

			double diff;
			if (!meta) {
				GST_ELEMENT_WARNING (v4l2sweep, RESOURCE, FAILED, ("FPNC magic metadata not found. A fpncmagic element is required upstream"), (NULL));
				diff = colDiff(v4l2sweep, buf);
			}else{
				diff = colDiffMagic(v4l2sweep, meta);
			}
			GST_DEBUG_OBJECT (v4l2sweep, "param: %d col diff: %f\n", v4l2sweep->paramVal , diff);

			if( (v4l2sweep->paramVal == v4l2sweep->minParamVal) || (diff < v4l2sweep->bestParam.error) ){
				v4l2sweep->bestParam.paramVal = v4l2sweep->paramVal;
				v4l2sweep->bestParam.error = diff;
			}
			break;
		}

		case MIN_AVG:
		{
			info = gst_meta_get_info("HistMeta");
			GstHistMeta * meta = NULL;
			if (info)
				meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
			if (!meta) {
				GST_ELEMENT_ERROR(v4l2sweep, RESOURCE, FAILED, ("Histogram metadata not found. A vhist element is required upstream"), (NULL));
				return GST_FLOW_ERROR;
			}
			GST_DEBUG_OBJECT (v4l2sweep, "param: %d avg: %f\n", v4l2sweep->paramVal , meta->avgval);

			if( (v4l2sweep->paramVal == v4l2sweep->minParamVal) || (meta->avgval < v4l2sweep->bestParam.error) ){
				v4l2sweep->bestParam.paramVal = v4l2sweep->paramVal;
				v4l2sweep->bestParam.error = meta->avgval;
			}
			break;
		}

		case MAX_AVG:
		{
			info = gst_meta_get_info("HistMeta");
			GstHistMeta * meta = NULL;
			if (info)
				meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
			if (!meta) {
				GST_ELEMENT_ERROR(v4l2sweep, RESOURCE, FAILED, ("Histogram metadata not found. A vhist element is required upstream"), (NULL));
				return GST_FLOW_ERROR;
			}
			GST_DEBUG_OBJECT (v4l2sweep, "param: %d avg: %f\n", v4l2sweep->paramVal , meta->avgval);

			if( (v4l2sweep->paramVal == v4l2sweep->minParamVal) || (meta->avgval > v4l2sweep->bestParam.error) ){
				v4l2sweep->bestParam.paramVal = v4l2sweep->paramVal;
				v4l2sweep->bestParam.error = meta->avgval;
			}
			break;
		}

		case MIN_MODE:
		{
			info = gst_meta_get_info("HistMeta");
			GstHistMeta * meta = NULL;
			if (info)
				meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
			if (!meta) {
				GST_ELEMENT_ERROR(v4l2sweep, RESOURCE, FAILED, ("Histogram metadata not found. A vhist element is required upstream"), (NULL));
				return GST_FLOW_ERROR;
			}
			int mode = meta->modeid*(256/meta->bin_no);
			modePercentage = (100.0*meta->bins[meta->modeid])/(v4l2sweep->info.width*v4l2sweep->info.height);
			GST_DEBUG_OBJECT (v4l2sweep, "param: %d mode: %d (%f %% pixels)\n", v4l2sweep->paramVal, mode, modePercentage);

			if( (v4l2sweep->paramVal == v4l2sweep->minParamVal) || (mode < v4l2sweep->bestParam.error) ||
				(mode == v4l2sweep->bestParam.error && modePercentage > v4l2sweep->modePercentage) ){
				v4l2sweep->bestParam.paramVal = v4l2sweep->paramVal;
				v4l2sweep->bestParam.error = mode;
				v4l2sweep->modePercentage = modePercentage;
			}
			break;
		}

		case MAX_MODE:
		{
			info = gst_meta_get_info("HistMeta");
			GstHistMeta * meta = NULL;
			if (info)
				meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
			if (!meta) {
				GST_ELEMENT_ERROR(v4l2sweep, RESOURCE, FAILED, ("Histogram metadata not found. A vhist element is required upstream"), (NULL));
				return GST_FLOW_ERROR;
			}
			int mode = meta->modeid*(256/meta->bin_no);
			modePercentage = (100.0*meta->bins[meta->modeid])/(v4l2sweep->info.width*v4l2sweep->info.height);
			GST_DEBUG_OBJECT (v4l2sweep, "param: %d mode: %d (%f %% pixels)\n", v4l2sweep->paramVal, mode, modePercentage);

			if( (v4l2sweep->paramVal == v4l2sweep->minParamVal) || (mode > v4l2sweep->bestParam.error) ||
				(mode == v4l2sweep->bestParam.error && modePercentage > v4l2sweep->modePercentage) ){
				v4l2sweep->bestParam.paramVal = v4l2sweep->paramVal;
				v4l2sweep->bestParam.error = mode;
				v4l2sweep->modePercentage = modePercentage;
			}
			break;
		}

		//TODO: add stuff as needed

		default:
			GST_ERROR_OBJECT (v4l2sweep, "Error, unsupported definition for best value.");
			return GST_FLOW_NOT_SUPPORTED;
	}

	if(v4l2sweep->paramVal < v4l2sweep->maxParamVal){
		if((v4l2sweep->bestValue == MAX_AVG && v4l2sweep->bestParam.error >= 255.0) ||
			(v4l2sweep->bestValue == MIN_AVG && v4l2sweep->bestParam.error <= 0.0) ||
			(v4l2sweep->bestValue == MAX_MODE && v4l2sweep->bestParam.error >= 255.0 && v4l2sweep->modePercentage > MIN_MODE_PERCENTAGE) ||
			(v4l2sweep->bestValue == MIN_MODE && v4l2sweep->bestParam.error <= 0.0 && v4l2sweep->modePercentage > MIN_MODE_PERCENTAGE) ){
			//stop
			GST_DEBUG_OBJECT (v4l2sweep, "Sweep done, bestParam = %d val = %f", v4l2sweep->bestParam.paramVal, v4l2sweep->bestParam.error);
			int ret = sendParamSetQuery(v4l2sweep, v4l2sweep->bestParam.paramVal);
			if(ret == GST_FLOW_OK)
				return GST_FLOW_EOS; //stop stream
			else
				return ret;
		}

		//next value
		int ret = increaseParam(v4l2sweep, v4l2sweep->sweepStep);
		if(ret == GST_FLOW_CUSTOM_ERROR_1){
			//Stop
			if(v4l2sweep->bestValue != NONE){
				GST_DEBUG_OBJECT (v4l2sweep, "Sweep done, bestParam = %d val = %f", v4l2sweep->bestParam.paramVal, v4l2sweep->bestParam.error);
				int ret = sendParamSetQuery(v4l2sweep, v4l2sweep->bestParam.paramVal);
				if(ret == GST_FLOW_OK)
					return GST_FLOW_EOS; //stop stream
				else
					return ret;
			}else{
				GST_DEBUG_OBJECT (v4l2sweep, "Sweep done");
				return GST_FLOW_EOS; //stop stream
			}
		}
		return ret;
	}else{
		if(v4l2sweep->bestValue != NONE){
			GST_DEBUG_OBJECT (v4l2sweep, "Sweep done, bestParam = %d val = %f", v4l2sweep->bestParam.paramVal, v4l2sweep->bestParam.error);
			int ret = sendParamSetQuery(v4l2sweep, v4l2sweep->bestParam.paramVal);
			if(ret == GST_FLOW_OK)
				return GST_FLOW_EOS; //stop stream
			else
				return ret;
		}else{
			GST_DEBUG_OBJECT (v4l2sweep, "Sweep done");
			return GST_FLOW_EOS; //stop stream
		}
	}
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "v4l2sweep", GST_RANK_NONE,
      GST_TYPE_V4L2SWEEP);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gst-plugins-qtec"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst-plugins-qtec"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    v4l2sweep,
    "V4L2 Sweep",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

