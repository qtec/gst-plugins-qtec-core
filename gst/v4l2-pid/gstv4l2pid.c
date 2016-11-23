/* GStreamer
 * Copyright (C) 2014 Qtechnology, Marianna S. Buschle <msb@qtec.com>
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
 * SECTION:element-gstv4l2pid
 *
 * The v4l2pid element is a PID Controller
 * which operates on a v4l2 control by sending queries to a v4l2control element.
 * Also requires a vhist element to produce the necessary metadata
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 --gst-plugin-path=./gst --gst-debug=v4l2control:5,v4l2pid:5
 * v4l2src device=/dev/qt5023_video0 io-mode=1 ! video/x-raw,width=1024,height=544,framerate=10/1 !
 * videocrop top=200 left=500 bottom=244 right=424 ! v4l2control device=/dev/qt5023_video0 !
 * tee name=t ! queue ! videoconvert ! ximagesink t. !
 * queue ! videoconvert ! vhist ! v4l2pid control-name="offset" target-value=0 stop=1
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
#include "gstv4l2pid.h"
#include <gst/histogram/gsthistmeta.h>
#include <gst/v4l2/gstv4l2Queries.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (v4l2pid_debug);
#define GST_CAT_DEFAULT v4l2pid_debug

/* prototypes */


static void gst_v4l2pid_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_v4l2pid_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_v4l2pid_dispose (GObject * object);
static void gst_v4l2pid_finalize (GObject * object);

static gboolean gst_v4l2pid_start(GstBaseSink *sink);
static gboolean gst_v4l2pid_stop(GstBaseSink *sink);

static GstFlowReturn gst_v4l2pid_show_frame (GstVideoSink * video_sink,
    GstBuffer * buf);

gboolean getControlInfo(GstV4l2PID *v4l2pid);

enum
{
  PROP_0,
  PROP_V4L2_CONTROL_NAME,
  PROP_INVERTED_CONTROL,
  PROP_TARGET_VAL,
  PROP_TARGET_TYPE,
  PROP_MAX_NR_RUNS,
  PROP_STOP_WHEN_DONE,
  PROP_MAX_ERROR,
  PROP_PID_KP,
  PROP_PID_KD,
  PROP_PID_KI,
  PROP_PID_ANTI_WINDUP_MAX,
  PROP_CONTROL_STEP
};

/* pad templates */

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ GRAY8 }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstV4l2PID, gst_v4l2pid, GST_TYPE_VIDEO_SINK,
  GST_DEBUG_CATEGORY_INIT (v4l2pid_debug, "v4l2pid", 0,
  "debug category for v4l2pid element"));

static void
gst_v4l2pid_class_init (GstV4l2PIDClass * klass)
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
      "V4L2 PID Controller", "Generic", "Adjusts a specified v4l2 parameter in order to achieve a specified value of a specified image property, requires a histogram element upstream",
      "Marianna Smidth Buschle <msb@qtec.com>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_v4l2pid_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_v4l2pid_stop);

  gobject_class->set_property = gst_v4l2pid_set_property;
  gobject_class->get_property = gst_v4l2pid_get_property;
  gobject_class->dispose = gst_v4l2pid_dispose;
  gobject_class->finalize = gst_v4l2pid_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_v4l2pid_show_frame);

  GST_DEBUG_CATEGORY_INIT (v4l2pid_debug, "v4l2pid", 0, "V4L2 Parameter Controller");

  // define properties
  g_object_class_install_property (gobject_class, PROP_V4L2_CONTROL_NAME, g_param_spec_string("control-name", "control-name",
		  "The name of the v4l2 control to modify in order to achieve the target value", CONTROL_NAME_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INVERTED_CONTROL, g_param_spec_boolean("inverted-control", "inverted-control",
		  "A flag specifying if the v4l2 control to be used is inverted: the values need to be decreased in order to achieve a high target value", FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TARGET_VAL, g_param_spec_uint("target-value", "target-value",
		  "The target value for the controller", TARGET_VAL_MIN, TARGET_VAL_MAX, TARGET_VAL_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_NR_RUNS, g_param_spec_uint("max-nr-runs", "max-nr-runs",
		  "The maximum amount of cycles the controller should run (0 == UNLIMITED)", 0, 50000, MAX_NR_RUNS_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STOP_WHEN_DONE, g_param_spec_boolean("stop", "stop",
		  "A flag specifying if the controller should stop upon reaching the target value", FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_ERROR, g_param_spec_double("max-err", "max-err",
		  "The maximum allowed error", 0, 10, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TARGET_TYPE, g_param_spec_int("target-type", "target-type",
  		  "Which image property should be matched to the target value.\n"
		  "\t\t\t\tAVERAGE INTENSITY = 0,\n"
		  "\t\t\t\tABSOLUTE MINIMUM INTENSITY = 1,\n"
		  "\t\t\t\tABSOLUTE MAXIMUM INTENSITY = 2,\n",
		  0, 2, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PID_KP, g_param_spec_double("pid-kp", "pid-kp",
  		  "The Proportional gain of the PID", 0, 1000, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PID_KD, g_param_spec_double("pid-kd", "pid-kd",
  		  "The Derivative gain of the PID", 0, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PID_KI, g_param_spec_double("pid-ki", "pid-ki",
  		  "The Integral gain of the PID", 0, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PID_ANTI_WINDUP_MAX, g_param_spec_double("anti-windup-max", "anti-windup-max",
  		  "The maximum value for the integral term of the PID (0 == UNLIMITED), use it to avoid wind-up", 0, 50000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONTROL_STEP, g_param_spec_uint("control-step", "control-step",
  		  "If set (!= 0), the control loop will increment/decrement the v4l2 control by this amount instead of using the PID", 0, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_v4l2pid_init (GstV4l2PID *v4l2pid)
{
	GST_DEBUG_OBJECT (v4l2pid, "gst_v4l2pid_init");

	//initialize properties with default values
	strncpy(v4l2pid->controlName, CONTROL_NAME_DEF, 32);
	v4l2pid->targetValue = TARGET_VAL_DEF;
	v4l2pid->invertedControl = FALSE;
	v4l2pid->maxNRuns = MAX_NR_RUNS_DEF;
	v4l2pid->pidControls = (PIDControls){.Kp=1, .Kd=0, .Ki=0, .antiWindupMax=0, .controlStep=0};
	v4l2pid->max_err = 0;
	v4l2pid->targetType = AVERAGE;
	v4l2pid->stopWhenDone = FALSE;
	v4l2pid->prev_buf = NULL;
}

static gboolean
gst_v4l2pid_start(GstBaseSink *sink)
{
	GstV4l2PID *v4l2pid = GST_V4L2PID (sink);

	GST_DEBUG_OBJECT (v4l2pid, "gst_v4l2pid_start");

	//Prepare PID Controller
	v4l2pid->pidControls.integral = 0;
	v4l2pid->pidControls.prev_err = 0;

	v4l2pid->firstRun = TRUE;
	v4l2pid->nRuns = 0;

	v4l2pid->last_err = 0;
	v4l2pid->dirUp = FALSE;
	v4l2pid->dirDown = FALSE;
	v4l2pid->prev_step = 0;

	return TRUE;
}

static gboolean
gst_v4l2pid_stop(GstBaseSink *sink)
{
	GstV4l2PID *v4l2pid = GST_V4L2PID (sink);

	GST_DEBUG_OBJECT (v4l2pid, "gst_v4l2pid_stop");

	return TRUE;
}

void
gst_v4l2pid_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
	GstV4l2PID *v4l2pid = GST_V4L2PID (object);

	GST_DEBUG_OBJECT (v4l2pid, "set_property id=%d", property_id);

	switch (property_id) {
		case PROP_V4L2_CONTROL_NAME:
			strncpy(v4l2pid->controlName, g_value_get_string(value), 32);
			GST_INFO_OBJECT (v4l2pid, "V4L2 Control name is %s\n", v4l2pid->controlName);
			break;
		case PROP_TARGET_VAL:
			v4l2pid->targetValue = g_value_get_uint(value);
			GST_INFO_OBJECT (v4l2pid, "Target value is %d\n", v4l2pid->targetValue);
			break;
		case PROP_INVERTED_CONTROL:
			v4l2pid->invertedControl = g_value_get_boolean(value);
			GST_INFO_OBJECT (v4l2pid, "Inverted control is %s\n", v4l2pid->invertedControl ? "True" : "False");
			break;
		case PROP_MAX_NR_RUNS:
			v4l2pid->maxNRuns = g_value_get_uchar(value);
			GST_INFO_OBJECT (v4l2pid, "Maximum number of runs is %d\n", v4l2pid->maxNRuns);
			break;
		case PROP_STOP_WHEN_DONE:
			v4l2pid->stopWhenDone = g_value_get_boolean(value);
			GST_INFO_OBJECT (v4l2pid, "Stop when done: %d\n", v4l2pid->stopWhenDone);
			break;
		case PROP_MAX_ERROR:
			v4l2pid->max_err = g_value_get_double(value);
			GST_INFO_OBJECT (v4l2pid, "Max error is %f\n", v4l2pid->max_err);
			break;
		case PROP_TARGET_TYPE:
			v4l2pid->targetType = g_value_get_int(value);
			GST_INFO_OBJECT (v4l2pid, "Target Type is %d\n", v4l2pid->targetType);
			break;
		case PROP_PID_KP:
			v4l2pid->pidControls.Kp = g_value_get_double(value);
			g_print ("PID proportional gain is %f\n", v4l2pid->pidControls.Kp);
			break;
		case PROP_PID_KD:
			v4l2pid->pidControls.Kd = g_value_get_double(value);
			GST_INFO_OBJECT (v4l2pid, "PID derivative gain is %f\n", v4l2pid->pidControls.Kd);
			break;
		case PROP_PID_KI:
			v4l2pid->pidControls.Ki = g_value_get_double(value);
			GST_INFO_OBJECT (v4l2pid, "PID integral gain is %f\n", v4l2pid->pidControls.Ki);
			break;
		case PROP_PID_ANTI_WINDUP_MAX:
			v4l2pid->pidControls.antiWindupMax = g_value_get_double(value);
			GST_INFO_OBJECT (v4l2pid, "PID anti wind-up integral max is %f\n", v4l2pid->pidControls.antiWindupMax);
			break;
		case PROP_CONTROL_STEP:
			v4l2pid->pidControls.controlStep = g_value_get_uint(value);
			GST_INFO_OBJECT (v4l2pid, "The control step is %d\n", v4l2pid->pidControls.controlStep);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

void
gst_v4l2pid_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
	GstV4l2PID *v4l2pid = GST_V4L2PID (object);

	GST_DEBUG_OBJECT (v4l2pid, "get_property id=%d", property_id);

	switch (property_id) {
		case PROP_V4L2_CONTROL_NAME:
			g_value_set_string(value, v4l2pid->controlName);
			break;
		case PROP_TARGET_VAL:
			g_value_set_uint(value, v4l2pid->targetValue);
			break;
		case PROP_INVERTED_CONTROL:
			g_value_set_boolean(value, v4l2pid->invertedControl);
			break;
		case PROP_MAX_NR_RUNS:
			g_value_set_uint(value, v4l2pid->maxNRuns);
			break;
		case PROP_STOP_WHEN_DONE:
			g_value_set_boolean(value, v4l2pid->stopWhenDone);
			break;
		case PROP_MAX_ERROR:
			g_value_set_double(value, v4l2pid->max_err);
			break;
		case PROP_TARGET_TYPE:
			g_value_set_int(value, v4l2pid->targetType);
			break;
		case PROP_PID_KP:
			g_value_set_double(value, v4l2pid->pidControls.Kp);
			break;
		case PROP_PID_KD:
			g_value_set_double(value, v4l2pid->pidControls.Kd);
			break;
		case PROP_PID_KI:
			g_value_set_double(value, v4l2pid->pidControls.Ki);
			break;
		case PROP_PID_ANTI_WINDUP_MAX:
			g_value_set_double(value, v4l2pid->pidControls.antiWindupMax);
			break;
		case PROP_CONTROL_STEP:
			g_value_set_uint(value, v4l2pid->pidControls.controlStep);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

void
gst_v4l2pid_dispose (GObject * object)
{
  GstV4l2PID *v4l2pid = GST_V4L2PID (object);

  GST_DEBUG_OBJECT (v4l2pid, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_v4l2pid_parent_class)->dispose (object);
}

void
gst_v4l2pid_finalize (GObject * object)
{
  GstV4l2PID *v4l2pid = GST_V4L2PID (object);

  GST_DEBUG_OBJECT (v4l2pid, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_v4l2pid_parent_class)->finalize (object);
}

int sendParamChangeQuery(GstV4l2PID *v4l2pid, int value)
{
	GST_DEBUG_OBJECT (v4l2pid, "sendParamChangeQuery value=%d", value);

	int ret = GST_FLOW_OK;

	//test limits
	if(v4l2pid->paramVal == v4l2pid->maxParamVal && value>0){
		if(v4l2pid->stopWhenDone)
			GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Reached maximum parameter value = %d", v4l2pid->maxParamVal), (NULL));
		else
			GST_ELEMENT_WARNING (v4l2pid, RESOURCE, FAILED, ("Reached maximum parameter value = %d", v4l2pid->maxParamVal), (NULL));
		return GST_FLOW_CUSTOM_ERROR;
	}
	if(v4l2pid->paramVal == v4l2pid->minParamVal && value<0){
		if(v4l2pid->stopWhenDone)
			GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Reached minimum parameter value = %d", v4l2pid->minParamVal), (NULL));
		else
			GST_ELEMENT_WARNING (v4l2pid, RESOURCE, FAILED, ("Reached minimum parameter value = %d", v4l2pid->minParamVal), (NULL));
		return GST_FLOW_CUSTOM_ERROR;
	}

	if(v4l2pid->paramVal+value > v4l2pid->maxParamVal){
		value = v4l2pid->maxParamVal;
	}else if(v4l2pid->paramVal+value < v4l2pid->minParamVal){
		value = v4l2pid->minParamVal;
	}else{
		value += v4l2pid->paramVal;
	}

	//make set control query
	GValue gv = G_VALUE_INIT;
	g_value_init(&gv, G_TYPE_INT);
	g_value_set_int(&gv, value);
	GstQuery* query = gst_v4l2_queries_new_set_control(v4l2pid->controlName, 0, &gv);
	gst_v4l2_queries_set_control_activate_flushing(query); //flush avg_frames, if present
	g_value_unset(&gv);

	//send query through pad
	gboolean res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2pid), query);

	//parse response
	if(res){
		const GValue* ptr;
		gst_v4l2_queries_parse_set_control(query, NULL, NULL, &ptr);
		//update paramVal
		v4l2pid->paramVal = g_value_get_int(ptr);
	}else{
		GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Set control query failed for control:%s val=%d", v4l2pid->controlName, value), (NULL));
		ret = GST_FLOW_ERROR;
	}
	gst_query_unref (query);

	return ret;
}

int increaseParam(GstV4l2PID *v4l2pid, int val)
{
	v4l2pid->dirUp = TRUE;
	v4l2pid->dirDown = FALSE;
	v4l2pid->prev_step = val;

	if(!v4l2pid->invertedControl){
		return sendParamChangeQuery(v4l2pid, val);
	}else{
		return sendParamChangeQuery(v4l2pid, -val);
	}
}

int decreaseParam(GstV4l2PID *v4l2pid, int val)
{
	v4l2pid->dirUp = FALSE;
	v4l2pid->dirDown = TRUE;
	v4l2pid->prev_step = val;

	if(!v4l2pid->invertedControl){
		return sendParamChangeQuery(v4l2pid, -val);
	}else{
		return sendParamChangeQuery(v4l2pid, val);
	}
}

int PIDController(PIDControls *pidControls, double err)
{
	//PID
	pidControls->integral += err;
	if(pidControls->antiWindupMax != 0){//anti wind-up
		if(pidControls->integral > pidControls->antiWindupMax)
			pidControls->integral = pidControls->antiWindupMax;
		if(pidControls->integral < -pidControls->antiWindupMax)
			pidControls->integral = -pidControls->antiWindupMax;
	}
	double derivative = err - pidControls->prev_err;
	pidControls->prev_err = err;
	int output = round(pidControls->Kp*err + pidControls->Ki*pidControls->integral + pidControls->Kd*derivative);

	/*g_print("PIDController output = %d = %f + %f + %f integral=%f derivative=%f\n", output,
			pidControls->Kp*err, pidControls->Ki*pidControls->integral, pidControls->Kd*derivative, pidControls->integral, derivative);*/
	return output;
}

int adjustParam(GstV4l2PID *v4l2pid, double val)
{
	double err = v4l2pid->targetValue-val;

	GST_DEBUG_OBJECT (v4l2pid, "setParam Err = %f Control_value = %d", err, v4l2pid->paramVal);

	int output;
	if(v4l2pid->pidControls.controlStep == 0) output = abs(PIDController(&v4l2pid->pidControls, err));
	else output = v4l2pid->pidControls.controlStep;

	if(err < -v4l2pid->max_err){ //above target
		if(v4l2pid->stopWhenDone && abs(v4l2pid->prev_step) == 1){
			//to avoid oscillating between 2 values
			if(v4l2pid->dirUp){
				//GST_DEBUG_OBJECT (v4l2pid, "Dir change up->down");
				if(abs(err) > abs(v4l2pid->last_err)){
					output = 1; //go back
				}else{
					//special case for ADC_GAIN
					if(v4l2pid->targetType == ABSOLUTE_MINIMUM && (val > 254 && v4l2pid->targetValue == 254)){
						GST_ELEMENT_WARNING (v4l2pid, RESOURCE, FAILED, ("Reached best value possible: val=%f param=%d", val, v4l2pid->paramVal), (NULL));
						return GST_FLOW_EOS;
					}else{
						GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Reached best value possible: val=%f param=%d", val, v4l2pid->paramVal), (NULL));
						return GST_FLOW_CUSTOM_ERROR;
					}
				}
			}else{
				//adc_gain gives worse values if set too high
				if(abs(err) > abs(v4l2pid->last_err)){
					v4l2pid->last_err = err;
					return increaseParam(v4l2pid, 1); //go back
				}
			}
		}
		v4l2pid->last_err = err;
		return decreaseParam(v4l2pid, output);
	}else if(err > v4l2pid->max_err){ //below target
		if(v4l2pid->stopWhenDone && abs(v4l2pid->prev_step) == 1){
			//to avoid oscillating between 2 values
			if(v4l2pid->dirDown){
				//GST_DEBUG_OBJECT (v4l2pid, "Dir change down->up");
				if(abs(err) > abs(v4l2pid->last_err)){
					output = 1; //go back
				}else{
					//special case for ADC_GAIN
					if(v4l2pid->targetType == ABSOLUTE_MINIMUM && (val > 254 && v4l2pid->targetValue == 254)){
						GST_ELEMENT_WARNING (v4l2pid, RESOURCE, FAILED, ("Reached best value possible: val=%f param=%d", val, v4l2pid->paramVal), (NULL));
						return GST_FLOW_EOS;
					}else{
						GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Reached best value possible: val=%f param=%d", val, v4l2pid->paramVal), (NULL));
						return GST_FLOW_CUSTOM_ERROR;
					}
				}
			}else{
				//adc_gain gives worse values if set too high
				if(abs(err) > abs(v4l2pid->last_err)){
					v4l2pid->last_err = err;
					return decreaseParam(v4l2pid, 1); //go back
				}
			}
		}
		v4l2pid->last_err = err;
		return increaseParam(v4l2pid, output);
	}else{ //at target
		return GST_FLOW_EOS;
	}
}

gboolean getControlInfo(GstV4l2PID *v4l2pid)
{
	GST_DEBUG_OBJECT (v4l2pid, "getControlInfo");

	gboolean ret = TRUE;

	//query control info
	GstQuery* query = gst_v4l2_queries_new_control_info (v4l2pid->controlName, 0);
	gboolean res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2pid), query);

	//parse response
	if(res){
		gst_v4l2_queries_parse_control_info(query, NULL, NULL, &v4l2pid->minParamVal, &v4l2pid->maxParamVal, NULL, NULL, NULL);
	}else{
		GST_ERROR_OBJECT (v4l2pid, "Get control info query failed for control:%s", v4l2pid->controlName);
		gst_query_unref (query);
		return FALSE;
	}
	gst_query_unref (query);

	//query control current value
	query = gst_v4l2_queries_new_get_control(v4l2pid->controlName, 0);

	//send query through pad
	res = gst_pad_peer_query(GST_BASE_SINK_PAD(v4l2pid), query);

	//parse response
	if(res){
		const GValue* ptr;
		gst_v4l2_queries_parse_get_control(query, NULL, NULL, &ptr);
		//update paramVal
		v4l2pid->paramVal = g_value_get_int(ptr);
		GST_INFO_OBJECT(v4l2pid, "Get control value query control:%s val=%d", v4l2pid->controlName, v4l2pid->paramVal);
	}else{
		GST_ERROR_OBJECT (v4l2pid, "Get control value query failed for control:%s", v4l2pid->controlName);
		ret = FALSE;
	}
	gst_query_unref (query);

	return ret;
}

static GstFlowReturn
gst_v4l2pid_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstV4l2PID *v4l2pid = GST_V4L2PID (sink);

  GST_DEBUG_OBJECT (v4l2pid, "show_frame");

  //Hack for T#643, check that we are not getting repeated buffers
  if(buf == v4l2pid->prev_buf){
	GST_DEBUG_OBJECT (v4l2pid, "Rejecting frame, identical buffer pointer");
	return GST_FLOW_OK;
  }
  v4l2pid->prev_buf = buf;

  //hack to deal with start function from v4l2pid running before v4l2control
  if(v4l2pid->firstRun){
	  v4l2pid->firstRun = FALSE;
	  if(!getControlInfo(v4l2pid)){
		  GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Error getting v4l2 control info"), (NULL));
		  return GST_FLOW_ERROR;
	  }
  }

  GstMapInfo mapInfo;
  gst_buffer_map (buf, &mapInfo, GST_MAP_READ);

  const GstMetaInfo *info = gst_meta_get_info("HistMeta");
  GstHistMeta * meta = NULL;
  if (info)
	  meta = (GstHistMeta *) gst_buffer_get_meta(buf, info->api);
  if (!meta) {
	  GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Histogram metadata not found"), (NULL));
	  return GST_FLOW_ERROR;
  }

  int val = -1;
  if(v4l2pid->targetType == AVERAGE){
	  val = meta->avgval;
  }else{
	  int requiredNrPxs = 0.01*mapInfo.size; //require 1% of pixels
	  if(requiredNrPxs == 0) requiredNrPxs = 1;
	  int nrPixels = 0;

	  if(v4l2pid->targetType == ABSOLUTE_MAXIMUM){
		  int index = meta->bin_no-1;
		  while(index >= 0){
			  nrPixels += meta->bins[index];
			  //GST_DEBUG_OBJECT (v4l2pid, "index: %d", index);
			  if(nrPixels >= requiredNrPxs){
				  val = index*((SATURATION_VALUE+1)/meta->bin_no);
				  break;
			  }
			  index--;
		  }
	  }else if(v4l2pid->targetType == ABSOLUTE_MINIMUM){
		  int index = 0;
		  while(index < meta->bin_no){
			  nrPixels += meta->bins[index];
			  if(nrPixels >= requiredNrPxs){
				  val = index*((SATURATION_VALUE+1)/meta->bin_no);
				  break;
			  }
			  index++;
		  }
	  }
  }

  gst_buffer_unmap (buf, &mapInfo);

  //no bin passed the test
  if(val == -1){
	  if(v4l2pid->stopWhenDone){
		  GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("No histogram bin had enough pixels in the desired category"), (NULL));
		  return GST_FLOW_ERROR;
	  }else{
		  GST_ELEMENT_WARNING(v4l2pid, RESOURCE, FAILED, ("No histogram bin had enough pixels in the desired category"), (NULL));
		  return GST_FLOW_OK;
	  }
  }

  int result;
  if(v4l2pid->nRuns < v4l2pid->maxNRuns || v4l2pid->maxNRuns == 0){

	  result = adjustParam(v4l2pid, val);
	  v4l2pid->nRuns++;

	  if(result != GST_FLOW_OK){
	  	  if(result == GST_FLOW_ERROR){
	  		  GST_WARNING_OBJECT (v4l2pid, "done, with errors");
	  		  return GST_FLOW_ERROR;
	  	  }else if(v4l2pid->stopWhenDone){
	  		  if(result == GST_FLOW_EOS){
	  			  GST_INFO_OBJECT (v4l2pid, "done, param = %d", v4l2pid->paramVal);
	  			  return GST_FLOW_EOS;
	  		  }else if(result == GST_FLOW_CUSTOM_ERROR){
	  			  GST_WARNING_OBJECT (v4l2pid, "done, with errors");
	  			return GST_FLOW_ERROR;
	  		  }
	  	  }
	    }
  }else{
	  if(v4l2pid->stopWhenDone){
		  GST_ELEMENT_ERROR (v4l2pid, RESOURCE, FAILED, ("Reached maximum number of runs = %d", v4l2pid->maxNRuns), (NULL));
		  return GST_FLOW_ERROR;
  	  }else{
		  GST_ELEMENT_WARNING (v4l2pid, RESOURCE, FAILED, ("Reached maximum number of runs = %d", v4l2pid->maxNRuns), (NULL));
	  	  return GST_FLOW_EOS; //stop stream
	  }
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "v4l2pid", GST_RANK_NONE,
      GST_TYPE_V4L2PID);
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
    v4l2pid,
    "V4L2 PID Controller",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

