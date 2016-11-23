/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-05-05 10:57:46
*/

#include <gst/check/gstcheck.h>
#include <time.h>
#include <stdlib.h>

/* NOTE ABOUT AVGFRAMES:
 * Do not forget that frames that are getting averaged are being dropped
 * and thus unreffed. Either increase the ref count so they dont get destroyed
 * or do not unref them afterwards!
 */

/* helper data */

/* random data for building buffers with */
guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff };
guint8 junk_data2[] = { 0x0a, 0x00, 0x03, 0x01, 0x04, 0xff, 0x00, 0xff };
guint8 junk_data3[] = { 0x05, 0x7f, 0x02, 0x02, 0x04, 0xfe, 0x00, 0xff };

GST_START_TEST (test_avgframes_frameavgno)
{
  gint i, randno, frametot;
  GstElement *filter;
  GstCaps *caps;
  GstBuffer *buffer;
  GstPad *pad_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "avgframes";
  GList *test_buffers = NULL;

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

  /* initialize and get a random number, with a minimum of 1 */
  srand(time(NULL));
  randno = rand() % 10 + 1;
  /* 7 because the static tests add up to 7 */
  frametot = randno + 7;

  /*caps init */
  caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, sizeof(junk_data),
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (caps));

  /* create a rudementary buffer that will be pushed */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
  test_buffers = g_list_append(test_buffers, buffer);
  /* copy buffer for test */
  /* -1 since we already made one */
  for (i=0; i<frametot-1; i++) {
    buffer = gst_buffer_copy (buffer);
    ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
    test_buffers = g_list_append(test_buffers, buffer);
  }
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


  /* test different frame averages */

  /* Push 2 buffers */
  g_object_set(filter, "frameno", 2, NULL);
  for (i=0; i<2; i++) {
     ck_assert_int_eq  (g_list_length (buffers), 0);
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }
   ck_assert_int_eq  (g_list_length (buffers), 1);

  /* Push only one buffer */
  g_object_set(filter, "frameno", 1, NULL);
  buffer = GST_BUFFER (test_buffers->data);
  test_buffers = g_list_remove (test_buffers, buffer);
  ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  /* check if we got something */
   ck_assert_int_eq  (g_list_length (buffers), 2);

  /* push 4 buffers */
  g_object_set(filter, "frameno", 4, NULL);
  for (i=0; i<4; i++) {
     ck_assert_int_eq  (g_list_length (buffers), 2);
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }
   ck_assert_int_eq  (g_list_length (buffers), 3);

  /* finally, push the rest of the buffers */
  g_object_set(filter, "frameno", randno, NULL);
  while (test_buffers != NULL) {
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
     ck_assert_int_eq  (g_list_length (buffers), 3);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }
   ck_assert_int_eq  (g_list_length (buffers), 4);


  /* cleanup */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");
  gst_check_drop_buffers();
  g_list_free (test_buffers);
  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  gst_caps_unref(caps);

  gst_check_teardown_element(filter);

}
GST_END_TEST;



GST_START_TEST (test_avgframes_flush)
{
  gint i;
  GstElement *filter;
  GstCaps *caps;
  GstBuffer *buffer;
  GstPad *pad_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "avgframes";
  GList *test_buffers = NULL;
  GstStructure *s;

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

  /* every third frame should generate an average */
  g_object_set(filter, "frameno", 3, NULL);

  /*caps init */
  caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, sizeof(junk_data),
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (caps));

  /* create a rudementary buffer that will be pushed */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
  test_buffers = g_list_append(test_buffers, buffer);
  /* copy buffer for test */
  for (i=0; i<7; i++) {
    buffer = gst_buffer_copy (buffer);
    ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
    test_buffers = g_list_append(test_buffers, buffer);
  }

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


  /* test downstream flush */
  /* build the event */
  s = gst_structure_new ("qtec-flush-struct",
    "name", G_TYPE_STRING, "qtec-flush", NULL);
    GST_DEBUG("Starting flush");
  /* Push only one buffer */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  buffer = GST_BUFFER (test_buffers->data);
  test_buffers = g_list_remove (test_buffers, buffer);
  ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  /* check if we got something */
   ck_assert_int_eq  (g_list_length (buffers), 0);

  /* send a flush event */

  gst_pad_push_event (src_pad, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));

  /* now push some more buffers, the forth one should return a buffer */

  for (i=0; i<3; i++) {
     ck_assert_int_eq  (g_list_length (buffers), 0);
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }
  /* it should now be 1 */
   ck_assert_int_eq  (g_list_length (buffers), 1);

  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");



  /* test upstream flush */
  /* the buffers in 'buffers' should remain at 1 from before */
  s = gst_structure_new ("qtec-flush-struct",
    "name", G_TYPE_STRING, "qtec-flush", NULL);
    GST_DEBUG("Starting flush");
  /* Push only one buffer */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  buffer = GST_BUFFER (test_buffers->data);
  test_buffers = g_list_remove (test_buffers, buffer);
  ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  /* check if we got something */
   ck_assert_int_eq  (g_list_length (buffers), 1);

  /* send a flush event */

  gst_pad_push_event (sink_pad, gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s));

  /* now push some more buffers, the forth one should return a buffer */

  for (i=0; i<3; i++) {
     ck_assert_int_eq  (g_list_length (buffers), 1);
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }

  /* this should now be 2 */
   ck_assert_int_eq  (g_list_length (buffers), 2);

  /* cleanup */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_check_drop_buffers();
  g_list_free (test_buffers);

  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  gst_caps_unref (caps);

  gst_check_teardown_element(filter);

}
GST_END_TEST;

GST_START_TEST (test_avgframes_averaging)
{

  GstElement *filter;
  GstCaps *caps;
  GstBuffer *buffer, *outp_buffer;
  GstPad *pad_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "avgframes";
  GList *test_buffers = NULL;

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

  /* every third frame should generate an average */
  g_object_set(filter, "frameno", 3, NULL);

  /*caps init */
  caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, sizeof(junk_data),
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert_msg (GST_IS_CAPS (caps));

  /* create a rudementary buffers that will be pushed */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
  test_buffers = g_list_append(test_buffers, buffer);

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data2, sizeof(junk_data2),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data2));
  test_buffers = g_list_append(test_buffers, buffer);

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data3, sizeof(junk_data3),
        0, sizeof(junk_data), NULL, NULL);
  ck_assert_msg(GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data3));
  test_buffers = g_list_append(test_buffers, buffer);

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

  /* send the frames to be averaged */
  g_object_set(filter, "frameno", 3, NULL);
  while (test_buffers != NULL) {
    buffer = GST_BUFFER (test_buffers->data);
    test_buffers = g_list_remove (test_buffers, buffer);
     ck_assert_int_eq  (g_list_length (buffers), 0);
    ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");
  }
   ck_assert_int_eq  (g_list_length (buffers), 1);

  /* verify that the avg was calculated correctly */
  outp_buffer = GST_BUFFER (buffers->data);
  buffers = g_list_remove (buffers, outp_buffer);
  gst_check_buffer_data(outp_buffer, junk_data3, sizeof(junk_data3));

  /* cleanup */
  ck_assert_msg (gst_element_set_state (filter,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  gst_check_drop_buffers();
  g_list_free (test_buffers);
  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  gst_caps_unref(caps);

  gst_check_teardown_element(filter);

}
GST_END_TEST;

static Suite *
avgframes_suite (void)
{
  Suite *s = suite_create ("avgframes");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_avgframes_frameavgno);
  tcase_add_test (tc_chain, test_avgframes_flush);
  tcase_add_test (tc_chain, test_avgframes_averaging);

  return s;
}

GST_CHECK_MAIN (avgframes);