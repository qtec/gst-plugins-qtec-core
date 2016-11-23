/* GStreamer
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstv4l2control
 *
 * The v4l2control element provides events and querries to pipeline
 * components for handling a v4lq device. The interface for creating
 * and handling v4l2 querries and events can be found under
 * gst-libs/gst/v4l2
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v v4l2src device=(devicepath) ! v4l2control device=(devicepath) ! glimagesink
 * ]|
 * adds v4l2 control handling to pipeline
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <linux/videodev2.h>
#include <string.h>
#include "gstv4l2control.h"
#include "gst/v4l2/gstv4l2Events.h"
#include "gst/v4l2/gstv4l2Queries.h"
#include "gst/v4l2/gstv4l2commonutils.h"
#include <sys/time.h>

typedef enum {
  V4L2_CONTROL_SIMPLE,
  V4L2_CONTROL_EXTENDED,
  V4L2_CONTROL_INVALID
} req_query_type;


GST_DEBUG_CATEGORY_STATIC (gst_v4l2_control_debug_category);
#define GST_CAT_DEFAULT gst_v4l2_control_debug_category

/* prototypes */

static void gst_v4l2_control_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_v4l2_control_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_v4l2_control_dispose (GObject * object);
static void gst_v4l2_control_finalize (GObject * object);

static gboolean gst_v4l2_control_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static gboolean gst_v4l2_control_start (GstBaseTransform * trans);
static gboolean gst_v4l2_control_stop (GstBaseTransform * trans);
static gboolean gst_v4l2_control_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_v4l2_control_src_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_v4l2_control_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

enum
{
  PROP_0,
  PROP_VIDEO_DEVICE,
  PROP_DROP_FRAMES_ON_UPDATE,
  PROP_DROP_EXTRA_FRAME_AFTER_UPDATE,
};

/* pad templates */

static GstStaticPadTemplate gst_v4l2_control_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static GstStaticPadTemplate gst_v4l2_control_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );


#define V4L2_DEVICE_DEF "/dev/video0"
#define DROP_ON_UPDATE_DEFAULT TRUE
#define DEFAULT_PERFORM_EXTRA_FRAME_DROP TRUE
/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstV4l2Control, gst_v4l2_control, GST_TYPE_BASE_TRANSFORM,
  GST_DEBUG_CATEGORY_INIT (gst_v4l2_control_debug_category, "v4l2control", 0,
  "debug category for v4l2control element"));

static void
gst_v4l2_control_class_init (GstV4l2ControlClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_v4l2_control_src_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&gst_v4l2_control_sink_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "v4l2 device controller", "Generic", "A component for handiling controls on a v4l2 device",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_v4l2_control_set_property;
  gobject_class->get_property = gst_v4l2_control_get_property;

  gobject_class->dispose = gst_v4l2_control_dispose;
  gobject_class->finalize = gst_v4l2_control_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_v4l2_control_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_control_stop);
  base_transform_class->query = GST_DEBUG_FUNCPTR (gst_v4l2_control_query);
  base_transform_class->transform_ip = GST_DEBUG_FUNCPTR (gst_v4l2_control_transform_ip);
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR (gst_v4l2_control_sink_event);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_v4l2_control_src_event);

  g_object_class_install_property (gobject_class, PROP_VIDEO_DEVICE, g_param_spec_string("device", "device",
      "The device to use", V4L2_DEVICE_DEF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DROP_FRAMES_ON_UPDATE, g_param_spec_boolean("drop-on-update", "dou",
      "Defines wether we want to drop frames that have not had a control change applied to them",
      DROP_ON_UPDATE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DROP_EXTRA_FRAME_AFTER_UPDATE,
    g_param_spec_boolean("drop-extra-frame", "Drop Extra Frame",
      "Drops an additional frame after update fames have been dropped. "
      "Only when \"drop-on-update\" is set ",
      DROP_ON_UPDATE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

void
gst_v4l2_control_dispose (GObject * object)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (object);

  GST_DEBUG_OBJECT (v4l2control, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_v4l2_control_parent_class)->dispose (object);
}

void
gst_v4l2_control_finalize (GObject * object)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (object);

  GST_DEBUG_OBJECT (v4l2control, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_v4l2_control_parent_class)->finalize (object);
}


/* sink and src pad event handlers */
static gboolean
gst_v4l2_control_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  //GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);

  //GST_DEBUG_OBJECT (v4l2control, "sink_event");

  return GST_BASE_TRANSFORM_CLASS (gst_v4l2_control_parent_class)->sink_event (
      trans, event);
}

void
gst_v4l2_control_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (object);

  GST_DEBUG_OBJECT (v4l2control, "set_property");

  switch (property_id) {
    case PROP_VIDEO_DEVICE:
      strncpy(v4l2control->videoDevice, g_value_get_string(value), 32);
      break;
    case PROP_DROP_FRAMES_ON_UPDATE:
      v4l2control->drop_on_update = g_value_get_boolean(value);
      break;
    case PROP_DROP_EXTRA_FRAME_AFTER_UPDATE:
      v4l2control->perfrom_extra_frame_drop = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_v4l2_control_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (object);

  GST_DEBUG_OBJECT (v4l2control, "get_property");

  switch (property_id) {
    case PROP_VIDEO_DEVICE:
      g_value_set_string(value, v4l2control->videoDevice);
      break;
    case PROP_DROP_FRAMES_ON_UPDATE:
      g_value_set_boolean(value, v4l2control->drop_on_update);
      break;
    case PROP_DROP_EXTRA_FRAME_AFTER_UPDATE:
      g_value_set_boolean(value, v4l2control->perfrom_extra_frame_drop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/*** utility functions ***/

static void
set_last_update(GstV4l2Control * v4l2control)
{
  GstClockTime gstnow, base_time;
  struct timespec now;

  clock_gettime (CLOCK_MONOTONIC, &now);
  gstnow = GST_TIMESPEC_TO_TIME (now);

  base_time = GST_ELEMENT(v4l2control)->base_time;

  v4l2control->last_update = gstnow - base_time;
  if (v4l2control->drop_on_update)
    v4l2control->drop_extra_frame = TRUE;
  GST_DEBUG("last_update:%" GST_TIME_FORMAT,GST_TIME_ARGS(v4l2control->last_update));
}

static gboolean
checkV4L2andGValueTypes(uint v4l2Type, guint flags, const GValue * val) {

  GType gvalType = G_VALUE_TYPE(val);

  if (!G_TYPE_IS_VALUE_TYPE(gvalType)){
    GST_DEBUG("Invalid gtype detected in checkV4L2andGValueTypes");
  }

  if (flags & V4L2_CTRL_FLAG_HAS_PAYLOAD)
  {
    GST_DEBUG("Compound control detected");
    switch (v4l2Type) {
      case V4L2_CTRL_TYPE_STRING:
        if (gvalType == G_TYPE_GSTRING)
          return V4L2_CONTROL_EXTENDED;
        break;
      case V4L2_CTRL_TYPE_INTEGER:
      case V4L2_CTRL_TYPE_INTEGER64:

        return V4L2_CONTROL_EXTENDED;
      default:
        GST_DEBUG("Invalid or unhandled v4l2 compound control type %d", v4l2Type);
        return V4L2_CONTROL_INVALID;
    }
    return V4L2_CONTROL_INVALID;
  }

  switch (v4l2Type) {
    case V4L2_CTRL_TYPE_INTEGER:
    case V4L2_CTRL_TYPE_MENU:
    case V4L2_CTRL_TYPE_INTEGER_MENU:
      if (gvalType == G_TYPE_INT)
        return V4L2_CONTROL_SIMPLE;
      break;
    case V4L2_CTRL_TYPE_BOOLEAN:
      if (gvalType == G_TYPE_BOOLEAN)
        return V4L2_CONTROL_SIMPLE;
      break;
    case V4L2_CTRL_TYPE_BUTTON:
      /* we assume that buttons just need to be pressed, so the value doesnt matter */
      return V4L2_CONTROL_SIMPLE;
    case V4L2_CTRL_TYPE_INTEGER64:
      if (gvalType == G_TYPE_INT64)
        return V4L2_CONTROL_SIMPLE;
      break;
    case V4L2_CTRL_TYPE_STRING:
      if (gvalType == G_TYPE_STRING)
        return V4L2_CONTROL_SIMPLE;
      break;
    /* ignored types */
    case V4L2_CTRL_TYPE_CTRL_CLASS:
      GST_DEBUG("Ignored v4l2 control type %d", v4l2Type);
      return V4L2_CONTROL_INVALID;
    /* unhandled types */
    case V4L2_CTRL_TYPE_BITMASK:
    /*case V4L2_CTRL_TYPE_U8:*/
    /*case V4L2_CTRL_TYPE_U16:*/
      GST_DEBUG("Unhandled v4l2 control type %d", v4l2Type);
      return V4L2_CONTROL_INVALID;
    default:
      GST_DEBUG("Invalid v4l2 control type %d", v4l2Type);
      return V4L2_CONTROL_INVALID;
  }

  return V4L2_CONTROL_INVALID;
}

/*** SIMPLE CONTROL HELPER FUNCTIONS ***/

static gboolean
checkValueRange(struct v4l2_queryctrl *qctrl, const GValue *val) {

  union {
    gint     i;
    gboolean b;
    const gchar *  s;
  } uval;

  switch (qctrl->type) {
    case V4L2_CTRL_TYPE_INTEGER:
      uval.i = g_value_get_int(val);
      if ((uval.i >= qctrl->minimum) && (uval.i <= qctrl->maximum))
        return TRUE;
      GST_DEBUG("Integer value range is incorrect: min: %d, val: %d, max:%d",
        qctrl->minimum, uval.i, qctrl->maximum);
      break;
    case V4L2_CTRL_TYPE_INTEGER_MENU:
      uval.i = g_value_get_int(val);
      if ((uval.i >= qctrl->minimum) && (uval.i <= qctrl->maximum))
        return TRUE;
      GST_DEBUG("Integer value range is incorrect: min: %d, val: %d, max:%d",
        qctrl->minimum, uval.i, qctrl->maximum);
      break;
    case V4L2_CTRL_TYPE_BOOLEAN:
      uval.b = g_value_get_boolean(val);
      if (uval.b == TRUE || uval.b == FALSE)
        return TRUE;
      GST_DEBUG("boolean value range is incorrect: min: %d, val: %d, max:%d",
        qctrl->minimum, uval.b, qctrl->maximum);
      break;
    case V4L2_CTRL_TYPE_BUTTON:
      /* we assume that buttons just need to be pressed, so the value doesnt matter */
      return TRUE;
    case V4L2_CTRL_TYPE_INTEGER64:
      /* Leaving empty since type is not implemented */
      return TRUE;
    case V4L2_CTRL_TYPE_STRING:
      /* from min>=0 to max>=0 string length, with size minimum + N*step chars long*/
      uval.s = g_value_get_string (val);
      if ((strlen(uval.s) >= qctrl->minimum) && (strlen(uval.s) <= qctrl->minimum)
          && ((strlen(uval.s)%qctrl->step - qctrl->minimum) == 0))
        return TRUE;
      break;
    default:
      return FALSE;
  }

  return FALSE;

}

static gboolean
setv4l2control(GstV4l2Control *v4l2control, struct v4l2_queryctrl *qctrl, const GValue *val) {

  int res = 0;
  struct v4l2_control control;

  control.id = qctrl->id;

  switch (qctrl->type) {
    case V4L2_CTRL_TYPE_INTEGER:
      control.value =  g_value_get_int (val);
      GST_DEBUG("setting integer control %s to value %d", qctrl->name, control.value);
      break;
    case V4L2_CTRL_TYPE_INTEGER_MENU:
      control.value =  g_value_get_int (val);
      GST_DEBUG("setting menu control %s to value %d", qctrl->name, control.value);
      break;
    case V4L2_CTRL_TYPE_BOOLEAN:
      control.value = g_value_get_boolean(val);
      GST_DEBUG("setting boolean control %s to value %d", qctrl->name, control.value);
      break;
    case V4L2_CTRL_TYPE_BUTTON:
      control.value = 0;
      GST_DEBUG("Activating button control %s", qctrl->name);
      return TRUE;
    case V4L2_CTRL_TYPE_INTEGER64:
      GST_ERROR("Extended api is not implemented, cannot handle Integer 64 type controls");
      return FALSE;
    case V4L2_CTRL_TYPE_STRING:
      GST_ERROR("String controls should be handled by the extended api");
      return FALSE;
    default:
      GST_DEBUG("cannot handle control %s", qctrl->name);
      return FALSE;
  }


  res = setControlUnchecked(v4l2control->videoDevice, &control);
  if (res > 0) {
    GST_ERROR("Got error when setting control, code id: %d error message: %s", res, strerror(res));
    return FALSE;
  }
  else if (res < 0) {
    GST_ERROR("Got error when setting control");
    return FALSE;
  }
  return TRUE;
}

/*** EXTENDED CONTROL HELPER FUNCTIONS ***/


static gboolean
sizebased_minmax_check(GArray *arr, __s64 min, __s64 max,
 guint arrlen, guint elesize) {

  guint i;

  if (elesize==1) {
    for (i=0; i<arrlen; i++) {
      if ((g_array_index(arr, gint8, i)<min) ||
          (g_array_index(arr, gint8, i)>max))
        return FALSE;
    }
    return TRUE;
  }
  else if (elesize==2){
    for (i=0; i<arrlen; i++) {
      if ((g_array_index(arr, gint16, i)<min) ||
          (g_array_index(arr, gint16, i)>max))
        return FALSE;
    }
    return TRUE;
      }
  else if (elesize==4){
    for (i=0; i<arrlen; i++) {
      if ((g_array_index(arr, gint32, i)<min) ||
          (g_array_index(arr, gint32, i)>max))
        return FALSE;
    }
    return TRUE;
  }
  else if (elesize==8){
    for (i=0; i<arrlen; i++) {
      if ((g_array_index(arr, gint64, i)<min) ||
          (g_array_index(arr, gint64, i)>max))
        return FALSE;
    }
    return TRUE;
  }
  else {
    GST_DEBUG("Unhandled element size of %d", elesize);
    return FALSE;
  }
  return FALSE;
}


static gboolean
checkExtValueRange(struct v4l2_query_ext_ctrl *qctrl, const GValue *val) {

  union {
    GArray   *arr;
    GString  *str;
    gchar    *chr;
    gpointer *ptr;
  } uval;
  guint elesize;
  GType type = G_VALUE_TYPE(val);

  if (qctrl->nr_of_dims != 1) {
    GST_DEBUG("Extended controls cannot handle multi-dimentional arrays");
    return FALSE;
  }

  uval.ptr = gst_v4l2_get_boxed_from_value((GValue *)val);
  if (type == G_TYPE_GSTRING) {
    if ((uval.str->len >= qctrl->minimum) && (uval.str->len <= qctrl->minimum)
          && (((uval.str->len%qctrl->step) - qctrl->minimum) == 0))
        return TRUE;
    return FALSE;
  }
  else if (type == G_TYPE_ARRAY) {
    /* check array size */
    if (uval.arr->len != qctrl->elems) {
      GST_DEBUG("Array size was incorrect, expecting %d, got %d", uval.arr->len, qctrl->elems);
      return FALSE;
    }
    /* then check the array type by using the element size as criteria */
    elesize = g_array_get_element_size (uval.arr);
    if (qctrl->elem_size != elesize){
      GST_DEBUG("Element size was incorrect, expecting %d, got %d", qctrl->elem_size, elesize);
      return FALSE;
    }
    /* then check if values are in range */
    return sizebased_minmax_check(uval.arr, qctrl->minimum, qctrl->maximum, uval.arr->len, elesize);
  }
  else {
    GST_DEBUG("Unhandled type for extended controls %s", g_type_name(type));
    return FALSE;
  }

  return FALSE;
}

static gboolean
setExtv4l2control(GstV4l2Control *v4l2control, struct v4l2_query_ext_ctrl *qctrl, const GValue *val) {

  union {
    GArray   *arr;
    GString  *str;
    gchar    *chr;
    gpointer *ptr;
  } uval;
  GType gvalType = G_VALUE_TYPE(val);
  uint datasize = 0;
  gpointer source_addr = NULL;

  struct v4l2_ext_controls *ctrls;
  /* first, retrieve the control and the gst structure */
  int res = getExtControl(v4l2control->videoDevice, qctrl->id, &ctrls);
  if(res!=0){
    GST_ERROR("Failed to retrieve extended control for id %d", qctrl->id);
    return FALSE;
  }

  /* next, the boxed value */
  uval.ptr = gst_v4l2_get_boxed_from_value((GValue *)val);

  /* and figure out its size and the address of the data */
  if (gvalType == G_TYPE_GSTRING) {
    source_addr = uval.str->str;
    datasize = uval.str->len * qctrl->elem_size;
  }
  else if (gvalType == G_TYPE_ARRAY) {
    source_addr = uval.arr->data;
    datasize = uval.arr->len * qctrl->elem_size;
  }
  else {
    GST_ERROR("Unsupported data type in setExtv4l2control");
    return FALSE;
  }

  GST_DEBUG("Copying data to extcontrol, target addr:%p, source addr:%p, length:%d", ctrls->controls->ptr, source_addr, datasize);
  /* next, copy the new data into the control */
  memcpy(ctrls->controls->ptr, source_addr, datasize);

  /* finally, set the control to the new values */
  res = setExtControl(v4l2control->videoDevice, ctrls);
  if (res > 0) {
    GST_ERROR("Got error when setting control, code id: %d error message: %s", res, strerror(res));
    cleanExtControls(ctrls);
    return FALSE;
  }
  else if (res < 0) {
    GST_ERROR("Got error when setting control");
    cleanExtControls(ctrls);
    return FALSE;
  }
  GST_DEBUG("Successfully Set extended control %s", qctrl->name);
  cleanExtControls(ctrls);
  return TRUE;
}

/*** EVENT HANDLERS ***/

/* handler for set control events */
static gboolean
handle_set_control_event(GstV4l2Control *v4l2control, GstEvent *event)
{
  const gchar *name = NULL;
  gint id = 0;
  const GValue *val = NULL;
  gboolean flush;
  //char * str = NULL;
  struct v4l2_queryctrl qctrl;
  struct v4l2_query_ext_ctrl qextctrls;
  req_query_type qtype;

  if(!gst_v4l2_events_parse_reciever_set_control(event, &name, &id, &val, &flush))
  {
    GST_DEBUG("Unable to parse a set control event");
    return FALSE;
  }


  /*
  str = gst_value_serialize (val);
  GST_DEBUG ("Got v4l2_set_control event, name: %s, id: %d, value: %s", name, id, str);
  free(str);*/

  if (id == 0) {
    id = getControlId(&v4l2control->v4l2ControlList, (char *) name);
    if (id == -1) {
      GST_DEBUG("Unable to get the control with name %s", name);
      return FALSE;
    }
  }

  if(queryControl(v4l2control->videoDevice, id, &qctrl)!=V4L2_INTERFACE_OK) {
    GST_DEBUG("Unable to query the control with id %d", id);
    return FALSE;
  }

  /* check if control is disabled */
  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    GST_DEBUG("Control %d is disabled", id);
    return FALSE;
  }

  /* check if control is read only */
  if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
    GST_DEBUG("Control %d is read only", id);
    return FALSE;
  }

  /*check if types match */
  qtype = checkV4L2andGValueTypes(qctrl.type, qctrl.flags, val);
  if (qtype == V4L2_CONTROL_SIMPLE) {
    /* then check if value is within the limits of the control */
    if (!checkValueRange(&qctrl, val)) {
      GST_DEBUG("Value range for control is incorrect");
      return FALSE;
    }
    /* if flushing is set, start flushing before control is set to ensure */
    /* no buffers are generated with the new value */
    if (flush && v4l2control->drop_on_update) {
      GstStructure *s = gst_structure_new ("qtec-flush-struct",
        "name", G_TYPE_STRING, "qtec-flush", NULL);
        GST_DEBUG("Starting flush");
      gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD(v4l2control),gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));
      //gst_pad_send_event ( GST_BASE_TRANSFORM_SINK_PAD(v4l2control), gst_event_new_flush_start ());
      v4l2control->flushing = TRUE;
    }

    /* finally try to set the control */
    if (setv4l2control(v4l2control, &qctrl, val)) {
      set_last_update(v4l2control);
      return TRUE;
    }
    return FALSE;
  }
  else if (qtype == V4L2_CONTROL_EXTENDED)   {
    /* first get the extended controls */
    if(queryExtControl(v4l2control->videoDevice, id, &qextctrls)!=V4L2_INTERFACE_OK) {
      GST_DEBUG("Unable to query the extended control with id %d", id);
      return FALSE;
    }
    /* check if values in control array are within limit */
    if (!checkExtValueRange(&qextctrls, val)) {
      GST_DEBUG("Value range for extended control is incorrect");
      return FALSE;
    }
    /* if flushing is set, start flushing before control is set to ensure */
    /* no buffers are generated with the new value */
    if (flush && v4l2control->drop_on_update) {
      GstStructure *s = gst_structure_new ("qtec-flush-struct",
        "name", G_TYPE_STRING, "qtec-flush",
        NULL);
        GST_DEBUG("Starting flush");
      gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD(v4l2control),gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));
      //gst_pad_send_event ( GST_BASE_TRANSFORM_SINK_PAD(v4l2control), gst_event_new_flush_start ());
      v4l2control->flushing = TRUE;
    }
    /* finally, try to set the control */
    if (setExtv4l2control(v4l2control, &qextctrls, val)) {
      set_last_update(v4l2control);
      return TRUE;
    }
    return FALSE;

  }
  else {
    GST_DEBUG("Types of control and value do not match");
    return FALSE;
  }


}


/*** QUERY HANDLERS ***/

static gboolean
handle_set_control_query(GstV4l2Control *v4l2control, GstQuery *query)
{
  const gchar *name = NULL;
  gint id = 0;
  const GValue *val = NULL;
  //char * str = NULL;
  struct v4l2_queryctrl qctrl;
  gboolean res = FALSE;
  gboolean flush = FALSE;
  int ctrlvalue;
  struct v4l2_query_ext_ctrl qextctrls;
  req_query_type qtype;

  if(!gst_v4l2_queries_parse_reciever_set_control(query, &name, &id, &val))
  {
    GST_DEBUG("Unable to parse a set control event");
    return FALSE;
  }

  flush = gst_v4l2_queries_set_control_is_flushing(query);

  /*str = gst_value_serialize (val);
  GST_DEBUG ("Got v4l2_set_control query, name: %s, id: %d, value: %s", name, id, str);
  free(str); */

  if (id == 0) {
    id = getControlId(&v4l2control->v4l2ControlList, (char *) name);
    if (id == -1) {
      GST_DEBUG("Unable to get the control with name %s", name);
      return FALSE;
    }
  }

  if(queryControl(v4l2control->videoDevice, id, &qctrl)!=V4L2_INTERFACE_OK) {
    GST_DEBUG("Unable to query the control with id %d", id);
    return FALSE;
  }

  /* check if control is disabled */
  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    GST_DEBUG("Control %d is disabled", id);
    return FALSE;
  }

  /* check if control is read only */
  if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
    GST_DEBUG("Control %d is read only", id);
    return FALSE;
  }

  /*check if types match */
  qtype = checkV4L2andGValueTypes(qctrl.type, qctrl.flags, val);

  if (qtype == V4L2_CONTROL_SIMPLE) {
    /* then check if value is within the limits of the control */
    if (!checkValueRange(&qctrl, val)) {
      GST_DEBUG("Value range for control is incorrect");
      return FALSE;
    }

    /* if flushing is set, start flushing before control is set to ensure */
    /* no buffers are generated with the new value */
    if (flush && v4l2control->drop_on_update) {
      GstStructure *s = gst_structure_new ("qtec-flush-struct",
        "name", G_TYPE_STRING, "qtec-flush",
        NULL);
      GST_DEBUG("Starting flush");
      gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD(v4l2control),gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));
      GST_DEBUG("flush sent");
      //gst_pad_send_event ( GST_BASE_TRANSFORM_SINK_PAD(v4l2control), gst_event_new_flush_start ());
      v4l2control->flushing = TRUE;
    }
    /* finally try to set the control */
    res =  setv4l2control(v4l2control, &qctrl, val);
    if (res)
      set_last_update(v4l2control);

    /* set the return value to that of the control */
    /* if value cant be set leave it as is */
    if (getControl(v4l2control->videoDevice, id, &ctrlvalue)==V4L2_INTERFACE_OK) {
      switch (qctrl.type) {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_INTEGER_MENU:
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_BUTTON:
          g_value_set_int((GValue *)val, ctrlvalue);
          break;
        default:
          break;
      }
    }
    gst_v4l2_queries_set_set_control(query, val);
  }
  else if (qtype == V4L2_CONTROL_EXTENDED) {
    /* first get the extended controls */
    if(queryExtControl(v4l2control->videoDevice, id, &qextctrls)!=V4L2_INTERFACE_OK) {
      GST_DEBUG("Unable to query extended control with id %d", id);
      return FALSE;
    }
    /* check if values in control array are within limit */
    if (!checkExtValueRange(&qextctrls, val)) {
      GST_DEBUG("Value range for extended control is incorrect");
      return FALSE;
    }
    /* if flushing is set, start flushing before control is set to ensure */
    /* no buffers are generated with the new value */
    if (flush && v4l2control->drop_on_update) {
      GstStructure *s = gst_structure_new ("qtec-flush-struct",
        "name", G_TYPE_STRING, "qtec-flush",
        NULL);
        GST_DEBUG("Starting flush");
      gst_pad_send_event (GST_BASE_TRANSFORM_SINK_PAD(v4l2control),gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));
      //gst_pad_send_event ( GST_BASE_TRANSFORM_SINK_PAD(v4l2control), gst_event_new_flush_start ());
      v4l2control->flushing = TRUE;
    }
    /* finally, try to set the control */
    if (setExtv4l2control(v4l2control, &qextctrls, val)) {
      set_last_update(v4l2control);
      return TRUE;
    }
    return FALSE;
  }
  else {
    GST_DEBUG("Types of control and value do not match");
    return FALSE;
  }

  return res;
}

static gboolean
handle_get_control_query(GstV4l2Control *v4l2control, GstQuery *query)
{
  const gchar *name = NULL;
  gint id = 0;
  GValue val = G_VALUE_INIT;
  //char * str = NULL;
  struct v4l2_queryctrl qctrl;
  struct v4l2_query_ext_ctrl exqctrl;
  int ctrlvalue;
  GArray *arr;
  GString *str;
  struct v4l2_ext_controls *ctrls;
  if(!gst_v4l2_queries_parse_reciever_get_control(query, &name, &id, NULL))
  {
    GST_DEBUG("Unable to parse a get control event");
    return FALSE;
  }

  /*str = gst_value_serialize (val);
  GST_DEBUG ("Got v4l2_set_control query, name: %s, id: %d, value: %s", name, id, str);
  free(str); */

  if (id == 0) {
    id = getControlId(&v4l2control->v4l2ControlList, (char *) name);
    if (id == -1) {
      GST_DEBUG("Unable to get the control with name %s", name);
      return FALSE;
    }
  }

  if(queryControl(v4l2control->videoDevice, id, &qctrl)!=V4L2_INTERFACE_OK) {
    GST_DEBUG("Unable to query the control with id %d", id);
    return FALSE;
  }

  if (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
    if(queryExtControl(v4l2control->videoDevice, id, &exqctrl)!=V4L2_INTERFACE_OK) {
      GST_DEBUG("Unable to query extended controls for control with id %d", id);
      return TRUE;
    }
      /* check if control is disabled */
    if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
      GST_DEBUG("Control %d is disabled", id);
      return FALSE;
    }

    /* check if control is write only */
    if (qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) {
      GST_DEBUG("Control %d is write only", id);
      return FALSE;
    }

    if (getExtControl(v4l2control->videoDevice, id, &ctrls) == V4L2_INTERFACE_OK) {
      switch (qctrl.type) {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_BOOLEAN:
          arr = g_array_sized_new (TRUE, FALSE, exqctrl.elem_size, exqctrl.elems);
          g_array_append_vals(arr, ctrls->controls->ptr, exqctrl.elems);
          g_value_init (&val, G_TYPE_ARRAY);
          g_value_take_boxed(&val, arr);
          gst_v4l2_queries_set_get_control(query, exqctrl.name, exqctrl.id, &val);
          return TRUE;
        case V4L2_CTRL_TYPE_STRING:
          str = g_string_new(ctrls->controls->string);
          g_value_init (&val, G_TYPE_STRING);
          g_value_take_boxed(&val, str);
          gst_v4l2_queries_set_get_control(query, exqctrl.name, exqctrl.id, &val);
        default:
          GST_ERROR("Unsupported V4L2 get control type %d", qctrl.type);
          break;
      }
      cleanExtControls(ctrls);
    }
    else {
      GST_DEBUG("unable to retrieve control with id %d", id);
    }
    return FALSE;
  }
  /* check if control is disabled */
  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    GST_DEBUG("Control %d is disabled", id);
    return FALSE;
  }

  /* check if control is write only */
  if (qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) {
    GST_DEBUG("Control %d is write only", id);
    return FALSE;
  }

  /* set the return value to that of the control */
  if (getControl(v4l2control->videoDevice, id, &ctrlvalue) == V4L2_INTERFACE_OK) {
    switch (qctrl.type) {
      case V4L2_CTRL_TYPE_INTEGER:
      case V4L2_CTRL_TYPE_INTEGER_MENU:
      case V4L2_CTRL_TYPE_BOOLEAN:
      case V4L2_CTRL_TYPE_BUTTON:
        g_value_init (&val, G_TYPE_INT);
        g_value_set_int(&val, ctrlvalue);
        gst_v4l2_queries_set_get_control(query, (gchar*) qctrl.name, qctrl.id, &val);
        return TRUE;
      default:
        GST_ERROR("Unsupported V4L2 get control type %d", qctrl.type);
        break;
    }
  }
  else {
    GST_DEBUG("unable to retrieve control with id %d", id);
  }

  return FALSE;
}


static gboolean
handle_control_info_query(GstV4l2Control *v4l2control, GstQuery *query)
{

  const gchar *name = NULL;
  gint id = 0;
  //char * str = NULL;
  struct v4l2_queryctrl qctrl;
  struct v4l2_query_ext_ctrl exqctrl;
  GArray * arr;
  GValue val = G_VALUE_INIT;

  if(!gst_v4l2_queries_parse_reciever_control_info(query, &name, &id, NULL, NULL, NULL, NULL, NULL))
  {
    GST_DEBUG("Unable to parse a control query event");
    return FALSE;
  }

  /*str = gst_value_serialize (val);
  GST_DEBUG ("Got v4l2_set_control query, name: %s, id: %d, value: %s", name, id, str);
  free(str);*/

  if (id == 0) {
    id = getControlId(&v4l2control->v4l2ControlList, (char *) name);
    if (id == -1) {
      GST_DEBUG("Unable to get the control with name %s", name);
      return FALSE;
    }
  }

  if(queryExtControl(v4l2control->videoDevice, id, &exqctrl)!=V4L2_INTERFACE_OK) {
    GST_DEBUG("Device cannot handle extended controls, trying original");

    if(queryControl(v4l2control->videoDevice, id, &qctrl)!=V4L2_INTERFACE_OK) {
      GST_DEBUG("Unable to query the control with id %d", id);
      return FALSE;
    }

    gst_v4l2_queries_set_control_info(query, (gchar *) &qctrl.name, qctrl.id, qctrl.minimum,
        qctrl.maximum, qctrl.flags, qctrl.default_value, exqctrl.step);

    return TRUE;
  }

  arr = g_array_sized_new (TRUE, FALSE, sizeof(__u32), V4L2_CTRL_MAX_DIMS);
  g_array_append_vals(arr, exqctrl.dims, V4L2_CTRL_MAX_DIMS);

  //val = g_value_init (g_new0 (GValue, 1), G_TYPE_ARRAY);
  g_value_init (&val, G_TYPE_ARRAY);
  g_value_take_boxed(&val, arr);

  gst_v4l2_queries_set_control_extended_info(query, (gchar *) &exqctrl.name, exqctrl.id,
        exqctrl.minimum, exqctrl.maximum, exqctrl.flags, exqctrl.default_value, exqctrl.step,
        exqctrl.elem_size, exqctrl.elems, exqctrl.nr_of_dims, &val);

  g_value_unset(&val);

  return TRUE;
}

static GstFlowReturn
gst_v4l2_control_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);

  GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buf);
  if (v4l2control->drop_on_update && timestamp < v4l2control->last_update) {
    GST_DEBUG("timestamp:%" GST_TIME_FORMAT " < last_update:%" GST_TIME_FORMAT ", dropping frame",
          GST_TIME_ARGS(timestamp), GST_TIME_ARGS(v4l2control->last_update));
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  } else if (v4l2control->perfrom_extra_frame_drop && v4l2control->drop_extra_frame) {
    GST_DEBUG("Dropping extra frame");
    v4l2control->drop_extra_frame = FALSE;
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  return GST_FLOW_OK;
}

static void
gst_v4l2_control_init (GstV4l2Control *v4l2control)
{
  strncpy(v4l2control->videoDevice, V4L2_DEVICE_DEF, 32);
  v4l2control->drop_on_update = DROP_ON_UPDATE_DEFAULT;
  v4l2control->v4l2ControlList = getControlList(v4l2control->videoDevice);
  v4l2control->flushing = FALSE;
  v4l2control->perfrom_extra_frame_drop = DEFAULT_PERFORM_EXTRA_FRAME_DROP;
}

static gboolean
gst_v4l2_control_start (GstBaseTransform * trans)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);

  cleanControlList(&v4l2control->v4l2ControlList);
  v4l2control->v4l2ControlList = getControlList(v4l2control->videoDevice);

  v4l2control->last_update = 0;

  if (v4l2control->v4l2ControlList.nrControls == 0) {
    GST_ERROR("Controls were not detected, make sure that \"%s\" is an actual v4l2 device",  v4l2control->videoDevice);
    return FALSE;
  }

  v4l2control->drop_extra_frame = FALSE;

  return TRUE;
}

static gboolean
gst_v4l2_control_stop (GstBaseTransform * trans)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);

  cleanControlList(&v4l2control->v4l2ControlList);

  return TRUE;
}

static gboolean
gst_v4l2_control_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);
  gboolean ret = FALSE;
  switch (gst_v4l2_queries_get_type (query)) {
    case GST_V4L2_QUERY_SET_CONTROL:
      ret = handle_set_control_query(v4l2control, query);
      break;
    case GST_V4L2_QUERY_GET_CONTROL:
      ret = handle_get_control_query(v4l2control, query);
      break;
    case GST_V4L2_QUERY_CONTROL_INFO:
      ret = handle_control_info_query(v4l2control, query);
      break;
    default:
      ret = GST_BASE_TRANSFORM_CLASS (gst_v4l2_control_parent_class)->query (
              trans, direction, query);
      break;
  }

  return ret;
}

static gboolean
gst_v4l2_control_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstV4l2Control *v4l2control = GST_V4L2_CONTROL (trans);
  gboolean ret = TRUE;

  switch (gst_v4l2_events_get_type (event)) {
    case GST_V4L2_EVENT_SET_CONTROL:
      ret = handle_set_control_event(v4l2control, event);
      break;
    default:
      ret = GST_BASE_TRANSFORM_CLASS (gst_v4l2_control_parent_class)->src_event (
              trans, event);
      break;
  }
  return ret;
}


gboolean
gst_v4l2_control_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "v4l2control", GST_RANK_NONE,
      GST_TYPE_V4L2_CONTROL);
}

