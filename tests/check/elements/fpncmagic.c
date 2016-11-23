/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-05-05 10:57:07
*/

#include <gst/check/gstcheck.h>
#include <time.h>
#include <stdlib.h>
#include <gst/fpncmagic/gstfpncmagicmeta.h>

/* NOTE ABOUT fpncmagic:
 * Do not forget that frames that are getting averaged are being dropped
 * and thus unreffed. Either increase the ref count so they dont get destroyed
 * or do not unref them afterwards!
 */

/* helper data */

#define EXPECTED_AVG 96
#define EXPECTED_DSIZE 8
/* random data for building buffers with */
guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff };

gint rolling_median[] = { 3, 3, 3, 3, 3, 3, 3, 3 };
gint error[] = { -3, 251, -2, 0, 1, 250, -3, 252 };

GST_START_TEST (test_fpncmagic_meta)
{
  GstElement *filter;
  GstCaps *caps;
  GstBuffer *buffer, *outp_buffer;
  GstPad *pad_peer;
  GstPad *sink_pad = NULL;
  GstPad *src_pad;
  gchar element_name[] = "fpncmagic";

  gst_check_drop_buffers();
  filter = gst_check_setup_element (element_name);

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
  /* the buffer should be unchanged */
  outp_buffer = GST_BUFFER (buffers->data);
  gst_check_buffer_data(outp_buffer, junk_data, sizeof(junk_data));

  /* check for metadata */

  GstFpncMagicMeta *fpncmeta =
    (GstFpncMagicMeta *) gst_buffer_get_gst_fpnc_magic_meta (outp_buffer);

  ck_assert_msg(fpncmeta != NULL, "Could not retrive fpncmagic metadata");
  ck_assert_msg(fpncmeta->avg == EXPECTED_AVG,
    "Average was not correct, was expecting %d and got %d", EXPECTED_AVG, fpncmeta->avg);
  ck_assert_msg(fpncmeta->data_size == EXPECTED_DSIZE,
    "Data size was not correct, was expecting %d and got %d", EXPECTED_DSIZE, fpncmeta->data_size);

  if (memcmp (fpncmeta->rolling_median, rolling_median, sizeof(rolling_median)) != 0) {
    g_print ("\nConverted data:\n");
    gst_util_dump_mem (((const guchar *)fpncmeta->rolling_median), fpncmeta->data_size * sizeof(gint));
    g_print ("\nExpected data:\n");
    gst_util_dump_mem (((const guchar *)rolling_median), sizeof(rolling_median));
  }

  if (memcmp (fpncmeta->error, error, sizeof(error)) != 0) {
    g_print ("\nConverted data:\n");
    int i;
    for (i=0; i<fpncmeta->data_size; i++ )
      GST_WARNING("VAL: %d", fpncmeta->error[i]);
    //gst_util_dump_mem (fpncmeta->error, fpncmeta->data_size * sizeof(gint));
    g_print ("\nExpected data:\n");
    gst_util_dump_mem (((const guchar *)error), sizeof(error));
  }

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
fpncmagic_suite (void)
{
  Suite *s = suite_create ("fpncmagic");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_fpncmagic_meta);

  return s;
}

GST_CHECK_MAIN (fpncmagic);