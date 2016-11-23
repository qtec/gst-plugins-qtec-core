/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-05-05 17:48:33
*/


#include <stdlib.h>
#include <gst/check/gstcheck.h>
#include <gst/histogram/gsthistmeta.h>

guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x01, 0xff};

GST_START_TEST (test_histogram_metadata)
{
  GstElement *filter;
  GstCaps *caps;
  GstPad *pad_src_peer, *pad_sink_peer, *src_pad,  *sink_pad;
  GstBuffer *buffer, *outp_buffer;
  int i;

  filter = gst_check_setup_element ("vhist");

  /*caps init */
  caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 8,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY8",
      NULL);
  ck_assert (GST_IS_CAPS (caps));

  /* set the bin no */
  g_object_set(filter, "binno", 0, NULL);

  /* create a buffer */
  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);

  /* create the source pad */
  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  GST_DEBUG ("src pad created");
  gst_check_setup_events (src_pad, filter, caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (filter, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (filter));

  sink_pad = gst_pad_new ("sink", GST_PAD_SINK);
  ck_assert_msg (GST_IS_PAD (sink_pad));
  gst_pad_set_chain_function (sink_pad, gst_check_chain_func);
  gst_pad_set_active (sink_pad, TRUE);
  pad_sink_peer = gst_element_get_static_pad (filter, "src");
  ck_assert_msg (gst_pad_link (pad_sink_peer, sink_pad) == GST_PAD_LINK_OK,
      "Could not link sink and %s source pads", GST_ELEMENT_NAME (filter));

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
    "could not set to playing");

  /* push a buffer */
  ck_assert_msg (gst_pad_push (src_pad, buffer) == GST_FLOW_OK,
      "Failed to push buffer");

  ck_assert_msg (gst_element_set_state (filter,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* check that the histogram was calculated correctly
     the buffer should be unchanged */
  outp_buffer = GST_BUFFER (buffers->data);
  gst_check_buffer_data(outp_buffer, junk_data, sizeof(junk_data));

  /* get the metadata */
  GstHistMeta *meta =
      (GstHistMeta *) gst_buffer_get_gst_hist_meta (outp_buffer);

  ck_assert_msg(meta != NULL, "Could not retrive fpncmagic metadata");

  ck_assert_msg(meta->bin_no == 2, "Bin number was not correct,"
    " expecting %d, got %d", 2, meta->bin_no);

  ck_assert_msg(meta->minval == 0x00, "Bin minval was not correct,"
    " expecting %d, got %d", 0x00, meta->minval);
  ck_assert_msg(meta->maxval == 0xff, "Bin maxval was not correct,"
    " expecting %d, got %d", 0xff, meta->maxval);

  ck_assert_msg(meta->avgval == 96, "Bin avgval was not correct,"
    " expecting %d, got %d", 96, meta->avgval);

  ck_assert_msg(meta->medianid == 0, "Bin medianid was not correct,"
    " expecting %d, got %d", 0, meta->medianid);

  ck_assert_msg(meta->modeid == 0, "Bin modeid was not correct,"
    " expecting %d, got %d", 0, meta->modeid);

  ck_assert_msg(meta->bins[0] == 5, "Bin %d should have a value of %d, got %d",
    0, 5, meta->bins[0]);

  ck_assert_msg(meta->bins[1] == 3, "Bin %d should have a value of %d, got %d",
    1, 3, meta->bins[1]);

  for (i=meta->bin_no; i<256; i++) {
    ck_assert_msg(meta->bins[i] == 0, "Histogram bins should have a value"
      " of 0 after bin_no");
  }


  g_object_unref (src_pad);
  g_object_unref (sink_pad);
  gst_buffer_unref(buffer);
  gst_object_unref (pad_src_peer);
  gst_object_unref (pad_sink_peer);
  gst_caps_unref(caps);
  gst_check_teardown_element(filter);
}
GST_END_TEST;

static Suite *
histogram_suite (void)
{
  Suite *s = suite_create ("histogram");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_histogram_metadata);

  return s;
}

GST_CHECK_MAIN (histogram);