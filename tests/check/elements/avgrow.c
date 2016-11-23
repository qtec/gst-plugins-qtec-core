/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-05-05 10:56:27
*/

#include <gst/check/gstcheck.h>
#include <time.h>
#include <stdlib.h>

/* NOTE ABOUT avgrow:
 * Do not forget that frames that are getting averaged are being dropped
 * and thus unreffed. Either increase the ref count so they dont get destroyed
 * or do not unref them afterwards!
 */

/* helper data */

/* random data for building buffers with */
guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff,
                       0x0a, 0x00, 0x03, 0x01, 0x04, 0xff, 0x00, 0xff,
                       0x05, 0x7f, 0x02, 0x02, 0x04, 0xfe, 0x00, 0xff };

int negotiated_height = 0;

static gboolean
sink_event_func (GstPad * pad, GstObject * parent,
    GstEvent * event) {

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstStructure *structure;
      gst_event_parse_caps (event, &caps);
      /* clear any pending reconfigure flag */
      structure = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (structure, "height", &negotiated_height);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

GST_START_TEST (test_avgrow_negotiation)
{
  gint i, frameheight, resheight, rowavg;
  GstElement *filter;
  GstCaps *in_caps, *out_caps;
  GstPad *pad_src_peer, *pad_sink_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "avgrow";
  gint maxavg = 5;
  gint heightmulti = 50;

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

  srand(time(NULL));

  /* try 10 different random frameheights an check if caps are negotiated correctly*/
  for (i=0; i<10; i++)
  {
    /* avging one to 5 rows */
    rowavg = rand() % maxavg + 1;
    /* set a height that is a multiplyer of the number of rows */
    resheight = rand() % heightmulti + 1;
    frameheight = resheight * rowavg;
    g_object_set(filter, "norows", rowavg, NULL);

    /*caps init */
    in_caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 500,
          "height", G_TYPE_INT, frameheight,
          "framerate", GST_TYPE_FRACTION, 1, 1,
          "format", G_TYPE_STRING, "GRAY8",
        NULL);
    ck_assert_msg (GST_IS_CAPS (in_caps));
    out_caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 500,
          "height", G_TYPE_INT, frameheight,
          "framerate", GST_TYPE_FRACTION, 1, 1,
          "format", G_TYPE_STRING, "GRAY8",
        NULL);
    ck_assert_msg (GST_IS_CAPS (out_caps));

    /* create and link the pads with the element */

    /* link our "source" to the averager */
    src_pad = gst_pad_new ("src", GST_PAD_SRC);
    ck_assert_msg (GST_IS_PAD (src_pad));
    gst_pad_set_active (src_pad, TRUE);
    GST_DEBUG ("src pad created");
    gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
    /* get the corresponding sink and link them together */
    pad_src_peer = gst_element_get_static_pad (filter, "sink");
    ck_assert_msg (GST_IS_PAD (pad_src_peer));
    ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));


    /* link our "sink" to the averager */
    sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
    ck_assert_msg (GST_IS_PAD (sink_pad));
    gst_pad_set_caps(sink_pad, out_caps);
    gst_pad_set_event_function(sink_pad, sink_event_func);
    gst_pad_set_active (sink_pad, TRUE);
    pad_sink_peer = gst_element_get_static_pad (filter, "src");
    ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
        "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));

    ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

    ck_assert_msg (gst_pad_push_event (src_pad, gst_event_new_caps (in_caps)));

    ck_assert_msg(negotiated_height == frameheight / rowavg, "Height was not negotiated, got %d when expecting %d",
        negotiated_height, frameheight / rowavg);

    ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

    ck_assert_msg(gst_pad_unlink (src_pad, pad_src_peer), "Unable to source pads");
    ck_assert_msg(gst_pad_unlink (pad_sink_peer, sink_pad), "Unable to unlink pads");

    gst_caps_unref(in_caps);
    gst_caps_unref(out_caps);
    g_object_unref (src_pad);
    g_object_unref (sink_pad);
    /* try with target having int range */
#if 0
    /* commented out since set_caps only allows for fixed caps (how are ranges set?!) */
    out_caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 500,
          "height", GST_TYPE_INT_RANGE, 1, 5 * 50,
          "framerate", GST_TYPE_FRACTION, 1, 1,
          "format", G_TYPE_STRING, "GRAY8",
        NULL);
    ck_assert_msg (GST_IS_CAPS (out_caps));

    sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
    ck_assert_msg (GST_IS_PAD (sink_pad));
    caps = gst_pad_get_current_caps (sink_pad);
    GST_WARNING("NEW SINK PADS %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps(sink_pad, caps)
    //structure = gst_caps_get_structure (caps, 0);

    gst_pad_set_event_function(sink_pad, sink_event_func);
    gst_pad_set_active (sink_pad, TRUE);
    pad_sink_peer = gst_element_get_static_pad (filter, "src");
    ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
        "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));
#endif
    /* finally check that for total-avg == TRUE the height is 1 */

    /*caps init */
    in_caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 500,
          "height", G_TYPE_INT, frameheight,
          "framerate", GST_TYPE_FRACTION, 1, 1,
          "format", G_TYPE_STRING, "GRAY8",
        NULL);
    ck_assert_msg (GST_IS_CAPS (in_caps));
    out_caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 500,
          "height", G_TYPE_INT, frameheight,
          "framerate", GST_TYPE_FRACTION, 1, 1,
          "format", G_TYPE_STRING, "GRAY8",
        NULL);
    ck_assert_msg (GST_IS_CAPS (out_caps));

    g_object_set(filter, "total-avg", 1, NULL);

    src_pad = gst_pad_new ("src", GST_PAD_SRC);
    ck_assert_msg (GST_IS_PAD (src_pad));
    gst_pad_set_active (src_pad, TRUE);
    GST_DEBUG ("src pad created");
    gst_check_setup_events (src_pad, filter, in_caps, GST_FORMAT_BYTES);
    /* get the corresponding sink and link them together */
    ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));


    sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
    ck_assert_msg (GST_IS_PAD (sink_pad));
    gst_pad_set_caps(sink_pad, out_caps);
    gst_pad_set_event_function(sink_pad, sink_event_func);
    gst_pad_set_active (sink_pad, TRUE);

    ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
        "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));
    ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

    ck_assert_msg (gst_pad_push_event (src_pad, gst_event_new_caps (in_caps)));

    ck_assert_msg(negotiated_height == 1, "Height was not negotiated, got %d when expecting %d",
        negotiated_height, 1);

    ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

    g_object_set(filter, "total-avg", 0, NULL);
    ck_assert_msg(gst_pad_unlink (src_pad, pad_src_peer), "Unable to source pads");
    ck_assert_msg(gst_pad_unlink (pad_sink_peer, sink_pad), "Unable to unlink sink pads");

    g_object_unref (src_pad);
    g_object_unref (sink_pad);
    gst_object_unref (pad_src_peer);
    gst_object_unref (pad_sink_peer);
    gst_caps_unref(in_caps);
    gst_caps_unref(out_caps);
  }

  gst_check_teardown_element(filter);

}
GST_END_TEST;


GST_START_TEST (test_avgrow_averaging)
{
  GstElement *filter;
  GstCaps *caps;
  GstBuffer *buffer, *outp_buffer;
  GstPad *pad_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "avgrow";

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

  /* every third frame should generate an average */
  g_object_set(filter, "total-avg", 1, NULL);

  /*caps init */
  caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, sizeof(junk_data)/3,
        "height", G_TYPE_INT, 3,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (caps));

  /* create a rudementary buffer that will be pushed */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));

  /* create and link the pads with the element */

  /* link our "source" to the averager */
  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  GST_DEBUG ("src pad created and activated");
  gst_check_setup_events (src_pad, filter, caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));
  gst_object_unref (pad_peer);

  /* link our "sink" to the averager */
  sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
  ck_assert_msg (GST_IS_PAD (sink_pad));
  gst_pad_set_chain_function (sink_pad, gst_check_chain_func);
  gst_pad_set_active (sink_pad, TRUE);
  pad_peer = gst_element_get_static_pad (filter, "src");
  ck_assert_msg (gst_pad_link (pad_peer, sink_pad) == GST_PAD_LINK_OK,
      "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));
  gst_object_unref (pad_peer);


  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* send the buffer to be averaged */

  ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");

  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* check that we got the result */
  ck_assert_int_eq (g_list_length (buffers), 1);

  /* check that the data was averaged correctly*/
  outp_buffer = GST_BUFFER (buffers->data);
  gst_check_buffer_data(outp_buffer, &junk_data[16], sizeof(junk_data)/3);



  /* cleanup */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_check_drop_buffers();

  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  gst_caps_unref(caps);

  gst_check_teardown_element(filter);


}
GST_END_TEST;

static Suite *
avgrow_suite (void)
{
  Suite *s = suite_create ("avgrow");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_avgrow_negotiation);
  tcase_add_test (tc_chain, test_avgrow_averaging);
  //tcase_add_test (tc_chain, test_avgrow_flush);

  return s;
}

GST_CHECK_MAIN (avgrow);