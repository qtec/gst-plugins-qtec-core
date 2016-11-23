  /*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2016-09-07 11:46:09
*/

#include <getopt.h>
#include <stdio.h>
#include <gst/check/gstcheck.h>
#include <gst/v4l2/gstv4l2commonutils.h>
#include <gst/v4l2/gstv4l2Queries.h>
#include <gst/v4l2/gstv4l2Events.h>
#include "v4l2utils/v4l2utils.h"

guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff };

gchar device[128] = { '\0' };

gboolean got_flush = FALSE;

#define INAME_SZ 32
struct ignore_data {
  gchar name[INAME_SZ];
};

static struct ignore_data ctr_ignore_list[] = {
  {"Gain"},
};
#define IGNORE_NO (sizeof (ctr_ignore_list) / sizeof ((ctr_ignore_list)[0]))

static gboolean
src_event_func (GstPad * pad, GstObject * parent,
    GstEvent * event) {

  GST_DEBUG("GOT EVENT %s", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
      const GstStructure *structure;
      const gchar *name;
      structure = gst_event_get_structure (event);
      name = gst_structure_get_string(structure, "name");
      if (name != NULL && strncmp(name, "qtec-flush", 10)==0){
        GST_DEBUG("GOT FLUSH");
        got_flush = TRUE;
      }

      break;
    }
    default:
      break;
  }
  return TRUE;
}

gboolean
get_simple_control(V4l2Control * ctrl, int* currval, int* targetval)
{
  V4l2ControlList list;
  struct v4l2_queryctrl qctrl;
  gint i, j, tempval;
  list = getControlList(device);
  struct v4l2_control v4l2ctrl;
  gboolean pass = FALSE;

  for (i=0; i<list.nrControls; i++) {
    /* find a basic integer control */
    if (!(queryControl(device, list.controls[i].id, &qctrl) == V4L2_INTERFACE_OK))
      continue;

    pass = FALSE;
    for (j=0; j<IGNORE_NO; j++)
      if (strncmp (ctr_ignore_list[j].name, (gchar*) qctrl.name, INAME_SZ) == 0) {
        pass = TRUE;
        break;
      }

    if (pass)
      continue;

    if (qctrl.type == V4L2_CTRL_TYPE_INTEGER &&
         !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED) &&
         !(qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) &&
         !(qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) &&
         !(qctrl.flags & V4L2_CTRL_FLAG_GRABBED) &&
         !(qctrl.flags & V4L2_CTRL_FLAG_INACTIVE))
    {
      if (getControl(device, list.controls[i].id, currval) == V4L2_INTERFACE_OK) {
        /* find a new value for the control */
        if (targetval != NULL) {
          GST_DEBUG("control %s, min:%d, max:%d", qctrl.name, qctrl.minimum, qctrl.maximum);
          for (j=qctrl.minimum; j<qctrl.maximum; j++) {
            if (j != *currval) {
              GST_DEBUG("TRYING VAL %d", j);
              /* try setting the value to the new value. If the value can change
                 the control keep it. This is done for cases of rounding.
               */
              v4l2ctrl.id = list.controls[i].id;
              v4l2ctrl.value = j;
              if(setControlUnchecked(device, &v4l2ctrl) != 0) {
                GST_ERROR("Could not set control '%s' to value '%d' that is within control range of [%d,%d]",
                  list.controls[i].name, j, qctrl.minimum, qctrl.maximum);
                cleanControlList(&list);
                return FALSE;
              }
              if(getControl(device, list.controls[i].id, &tempval) != V4L2_INTERFACE_OK) {
                GST_ERROR("Could not get the value of control '%s'",
                  list.controls[i].name);
                cleanControlList(&list);
                return FALSE;
              }
              if (tempval == *currval)
                continue;
              /* found a new value for the control, set it back to currval*/
              v4l2ctrl.value = *currval;
              if(setControlUnchecked(device, &v4l2ctrl) != 0) {
                GST_ERROR("Could not set control '%s' back to default value '%d'",
                  list.controls[i].name, *currval);
                cleanControlList(&list);
                return FALSE;
              }

              *ctrl = list.controls[i];
              *targetval = j;
              cleanControlList(&list);
              return TRUE;
            }
          }
        } else {
          *ctrl = list.controls[i];
          cleanControlList(&list);
          return TRUE;
        }
      }
    }
  }
  cleanControlList(&list);

  return FALSE;
}


GST_START_TEST (test_v4l2control_set_control_event)
{
  GstElement *filter;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *pad_sink_peer, *src_pad, *sink_pad;
  GstStructure *s;
  GValue val = G_VALUE_INIT;

  V4l2Control ctrl;
  gint oldval, targetval, newval;
  /* first, get a value from a countrol */
  ck_assert_msg(get_simple_control(&ctrl, &oldval, &targetval),
    "could not retrieve a basic integer control");

  filter = gst_check_setup_element ("v4l2control");
  g_object_set(filter, "device", device, NULL);

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
  ck_assert_msg (GST_IS_PAD (sink_pad));
  gst_pad_set_event_function(sink_pad, src_event_func);
  gst_pad_set_active (sink_pad, TRUE);
  pad_sink_peer = gst_element_get_static_pad (filter, "src");
  ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
      "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
    "could not set to playing");

  /* send an event that procs a flush */
  /* manually defining the event since we do not have a gstelement to use the api*/
  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, targetval);
  s = gst_structure_new (GST_V4L2_EVENT_NAME,
    "name", G_TYPE_STRING, ctrl.name,
    "id", G_TYPE_INT, 0,
    "type", G_TYPE_STRING, GST_V4L2_QUERY_SET_CONTROL_NAME,
    "flush", G_TYPE_BOOLEAN, TRUE,
    NULL);
  gst_structure_set_value (s, "value", &val);

  ck_assert_msg (gst_pad_push_event (sink_pad,
    gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s)));

  /* now check if the control changed */
  ck_assert_msg (getControl(device, ctrl.id, &newval)==V4L2_INTERFACE_OK,
    "Could not retrieve new value for control %s", ctrl.name);

  ck_assert_msg (targetval == newval, "Could not change control '%s', old val:%d"
    " targetval val:%d, new val: %d", ctrl.name, oldval, targetval, newval);


  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  setControl(device, ctrl.id, oldval);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(filter);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);
  g_object_unref (sink_pad);

}
GST_END_TEST;

/* tests for queries */
GST_START_TEST (test_v4l2control_set_control_query)
{
  GstElement *filter, *sink, *pipeline;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *src_pad;
  GValue val = G_VALUE_INIT;
  GstQuery *query;
  gboolean res;
  V4l2Control ctrl;
  gint oldval, targetval, newval;
  /* first, get a value from a countrol */
  ck_assert_msg(get_simple_control(&ctrl, &oldval, &targetval),
    "could not retrieve a basic integer control");

  pipeline = gst_pipeline_new ("pipeline");
  filter = gst_check_setup_element ("v4l2control");
  sink = gst_check_setup_element("fakesink");
  g_object_set(filter, "device", device, NULL);

  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), filter));
  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), sink));
  ck_assert_msg (gst_element_link (filter, sink));

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
    "could not set to playing");

  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, targetval);

  query = gst_v4l2_queries_new_set_control(ctrl.name, 0, &val);
  res = gst_element_query(GST_ELEMENT(sink), query);
  ck_assert_msg(res, "Unable to set %s to %d", ctrl.name, targetval);
  gst_query_unref (query);

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* now check if the control changed */
  ck_assert_msg (getControl(device, ctrl.id, &newval)==V4L2_INTERFACE_OK,
    "Could not retrieve new value for control %s", ctrl.name);
  ck_assert_msg (targetval == newval, "Could not change control '%s', old val:%d"
    " targetval val:%d, new val: %d", ctrl.name, oldval, targetval, newval);

  setControl(device, ctrl.id, oldval);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(pipeline);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);
}
GST_END_TEST;

GST_START_TEST (test_v4l2control_get_control_query)
{
  GstElement *filter, *sink, *pipeline;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *src_pad;
  GstQuery *query;
  const GValue *cval = NULL;
  const gchar *retname;
  gint val, retid;
  gboolean res;
  V4l2Control ctrl;
  gint orgval;
  /* first, get a value from a countrol */
  ck_assert_msg(get_simple_control(&ctrl, &orgval, NULL),
    "could not retrieve a basic integer control");

  pipeline = gst_pipeline_new ("pipeline");
  filter = gst_check_setup_element ("v4l2control");
  sink = gst_check_setup_element("fakesink");
  g_object_set(filter, "device", device, NULL);

  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), filter));
  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), sink));
  ck_assert_msg (gst_element_link (filter, sink));

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
    "could not set to playing");

  query = gst_v4l2_queries_new_get_control(ctrl.name, 0);
  res = gst_element_query(GST_ELEMENT(sink), query);
  ck_assert_msg(res, "Unable to get %s", ctrl.name);
  gst_v4l2_queries_parse_get_control(query, &retname, &retid, &cval);

  val = g_value_get_int (cval);

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* check that the recieved values match */

  ck_assert_msg(strncmp(retname, ctrl.name, 32) == 0, "Recieved names do not match, got %s when expecting %s",
    retname, ctrl.name);

  ck_assert_msg(retid == ctrl.id, "Recieved ids do not match, got %d when expecting %d",
    retid, ctrl.id);

  ck_assert_msg(val == orgval, "Recieved values do not match, got %d when expecting %d",
    val, orgval);

  gst_query_unref (query);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(pipeline);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);
}
GST_END_TEST;

GST_START_TEST (test_v4l2control_control_info_query)
{
  GstElement *filter, *sink, *pipeline;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *src_pad;
  GstQuery *query;
  GArray *arr = NULL;
  const GValue *val = NULL;
  const gchar *name;
  gint id, min, max, default_val, step;
  guint flags, elem_size, elems, nr_of_dims;
  gboolean res;
  V4l2Control ctrl;
  gint orgval;

  /* first, select a control */
  ck_assert_msg(get_simple_control(&ctrl, &orgval, NULL),
    "could not retrieve a basic integer control");


  pipeline = gst_pipeline_new ("pipeline");
  filter = gst_check_setup_element ("v4l2control");
  sink = gst_check_setup_element("fakesink");
  g_object_set(filter, "device", device, NULL);

  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), filter));
  ck_assert_msg (gst_bin_add (GST_BIN (pipeline), sink));
  ck_assert_msg (gst_element_link (filter, sink));

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
    "could not set to playing");

  query = gst_v4l2_queries_new_control_info(ctrl.name, 0);
  res = gst_element_query(GST_ELEMENT(sink), query);
  ck_assert_msg(res, "Unable to get %s", ctrl.name);

  res = gst_v4l2_queires_is_extended_control_info(query);
  if(res) {
    ck_assert_msg(gst_v4l2_queries_parse_control_extended_info(query, NULL, &id, &min,
      &max, &flags, &default_val, &step, &elem_size, &elems,
      &nr_of_dims, &val), "Unable to parse extended control info");
  } else {
    ck_assert_msg(gst_v4l2_queries_parse_control_info(query, &name, &id, &min,
      &max, &flags, &default_val, &step), "Unable to parse control info");
  }

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* now check if all info matches */
  if (res) {
    struct v4l2_query_ext_ctrl qctrl;
    ck_assert_msg(queryExtControl(device, ctrl.id, &qctrl) == V4L2_INTERFACE_OK,
      "Could not query controls '%s' from device '%s'", ctrl.name, device);
    ck_assert_msg(qctrl.id == id, "Ids don't match %d != %d", qctrl.id, id);
    ck_assert_msg(qctrl.minimum == min, "Minimums don't match %d != %d", qctrl.minimum, min);
    ck_assert_msg(qctrl.maximum == max, "Mmaximums don't match %d != %d", qctrl.maximum, max);
    ck_assert_msg(qctrl.step == step, "Steps don't match %d != %d", qctrl.step, step);
    ck_assert_msg(qctrl.default_value == default_val, "Default values don't match %d != %d", qctrl.default_value, default_val);
    ck_assert_msg(qctrl.flags == flags, "Flags don't match %d != %d", qctrl.flags, flags);
    ck_assert_msg(qctrl.elem_size == elem_size, "Elem sizes don't match %d != %d", qctrl.elem_size, elem_size);
    ck_assert_msg(qctrl.elems == elems, "Elems don't match %d != %d", qctrl.elems, elems);
    ck_assert_msg(qctrl.nr_of_dims == nr_of_dims, "'nr_of_dims' don't match %d != %d", qctrl.nr_of_dims, nr_of_dims);
    arr = (GArray*) gst_v4l2_get_boxed_from_value((GValue*)val);
    guint * arr1 = (guint *) arr->data;
    int i;
    for (i=0; i<4; i++)
      ck_assert_msg(qctrl.dims[i] == arr1[i], "Dimentions don't match for index %d: %d != %d", i, qctrl.dims[i], arr1[i]);
  }
  else {
    struct v4l2_queryctrl qctrl;
    ck_assert_msg(queryControl(device, ctrl.id, &qctrl) == V4L2_INTERFACE_OK,
      "Could not query controls '%s' from device '%s'", ctrl.name, device);
    ck_assert_msg(qctrl.id == id, "Ids don't match %d != %d", qctrl.id, id);
    ck_assert_msg(qctrl.minimum == min, "Minimums don't match %d != %d", qctrl.minimum, min);
    ck_assert_msg(qctrl.maximum == max, "Mmaximums don't match %d != %d", qctrl.maximum, max);
    ck_assert_msg(qctrl.step == step, "Steps don't match %d != %d", qctrl.step, step);
    ck_assert_msg(qctrl.default_value == default_val, "Default values don't match %d != %d", qctrl.default_value, default_val);
    ck_assert_msg(qctrl.flags == flags, "Flags don't match %d != %d", qctrl.flags, flags);
  }

  gst_query_unref (query);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(pipeline);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);

}
GST_END_TEST;

GST_START_TEST (test_v4l2control_flush)
{
  GstElement *filter;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *pad_sink_peer, *src_pad, *sink_pad;
  GstStructure *s;
  GValue val = G_VALUE_INIT;

  V4l2Control ctrl;
  gint oldval, targetval;
  /* first, get a value from a countrol */
  ck_assert_msg(get_simple_control(&ctrl, &oldval, &targetval),
    "could not retrieve a basic integer control");

  filter = gst_check_setup_element ("v4l2control");
  g_object_set(filter, "device", device, NULL);

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
  ck_assert_msg (GST_IS_PAD (sink_pad));
  gst_pad_set_event_function(sink_pad, src_event_func);
  gst_pad_set_active (sink_pad, TRUE);
  pad_sink_peer = gst_element_get_static_pad (filter, "src");
  ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
      "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
    "could not set to playing");

  /* send an event that procs a flush */
  /* manually defining the event since we do not have a gstelement to use the api*/
  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, targetval);
  s = gst_structure_new (GST_V4L2_EVENT_NAME,
    "name", G_TYPE_STRING, ctrl.name,
    "id", G_TYPE_INT, 0,
    "type", G_TYPE_STRING, GST_V4L2_QUERY_SET_CONTROL_NAME,
    "flush", G_TYPE_BOOLEAN, TRUE,
    NULL);
  gst_structure_set_value (s, "value", &val);
  GST_DEBUG("Starting flush");

  ck_assert_msg (gst_pad_push_event (sink_pad,
    gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s)));

  ck_assert_msg(got_flush, "Flush event was not recieved downstream");

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  setControl(device, ctrl.id, oldval);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(filter);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);
  g_object_unref (sink_pad);
}
GST_END_TEST;

static Suite *
v4l2control_suite (void)
{
  Suite *s = suite_create ("v4l2control");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_v4l2control_set_control_event);
  tcase_add_test (tc_chain, test_v4l2control_set_control_query);
  tcase_add_test (tc_chain, test_v4l2control_get_control_query);
  tcase_add_test (tc_chain, test_v4l2control_control_info_query);
  tcase_add_test (tc_chain, test_v4l2control_flush);

  return s;
}

#define DEFAULT_DEVICE "/dev/qt5023_video0"

int main (int argc, char **argv)
{

  /* initialize device to default value */

  strcpy (device, DEFAULT_DEVICE );

  /* see for input option */

  int c;

  while (1) {
    static char long_options_desc[][64] = {
      {"Device location."},
      {0, 0, 0, 0}
    };
    static struct option long_options[] = {
      {"device", 1, 0, 'd'},
      {0, 0, 0, 0}
    };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, ":d:h", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1) {
      //printf ("tip: use -h to see help message.\n");
      break;
    }

    switch (c) {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0)
          break;
        printf ("option %s", long_options[option_index].name);
        if (optarg)
          printf (" with arg %s", optarg);
        printf ("\n");
        break;

      case 'd':
        memset (device, '\0', sizeof(device));
        strncpy (device, optarg, sizeof (device) / sizeof (device[0]));
        break;

      case 'h':
        printf ("Usage: v4l2src-test [OPTION]...\n");
        for (c = 0; long_options[c].name; ++c) {
          printf ("-%c, --%s\r\t\t\t\t%s\n", long_options[c].val,
              long_options[c].name, long_options_desc[c]);
        }
        exit (0);
        break;

      case '?':
        /* getopt_long already printed an error message. */
        printf ("Use -h to see help message.\n");
        break;

      default:
        abort ();
    }
  }

  printf ("Running test on device %s \n", device);

  /* Print any remaining command line arguments (not options). */
  if (optind < argc) {
    printf ("Use -h to see help message.\n" "non-option ARGV-elements: ");
    while (optind < argc)
      printf ("%s ", argv[optind++]);
    putchar ('\n');
  }

  Suite *s;
  gst_check_init (&argc, &argv);
  s = v4l2control_suite ();
  return gst_check_run_suite (s, "v4l2control", __FILE__);
}
