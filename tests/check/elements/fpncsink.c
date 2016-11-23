/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-05-06 11:23:26
*/

#include <getopt.h>
#include <stdio.h>
#include <gst/check/gstcheck.h>
#include "v4l2utils/v4l2utils.h"

gchar device[128] = { '\0' };
guint8 junk_data[] = { 0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff,
                       0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff,
                       0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff,
                       0x00, 0xfe, 0x01, 0x03, 0x04, 0xfd, 0x00, 0xff,
                       };

void
increment_junk () {
  int i;
  for (i=0; i<(sizeof(junk_data)); i=i+2) {
    junk_data[i] += 0x01;
  }
}

#define FPNC "Fixed Pattern Noise Correction"

gboolean signal_not_emitted;
typedef struct _fpnc_data {
  union {
    guint8 * d8;
    guint16 * d16;
    guint32 * d32;
    gpointer ptr;
  };
  guint data_size;
  guint elem_size;
  guint elems;
  gboolean valid;
} FpncData;

/* check to see if the fpnc is provided by the device */
gboolean
is_fpnc_available() {
  V4l2ControlList list;
  int id;
  list = getControlList(device);
  id = getControlId(&list, FPNC);

  if(id == -1) {
    GST_ERROR("Was unable to find fpnc in this device");
    return FALSE;
  }
  return TRUE;
}

gboolean
compare_fpnc(FpncData *data) {
  V4l2ControlList list;
  struct v4l2_ext_controls* controls;
  struct v4l2_query_ext_ctrl qctrl;
  int id, i;
  union {
    guint8 * d8;
    guint16 * d16;
    guint32 * d32;
    gpointer ptr;
  } dptr;

  list = getControlList(device);
  id = getControlId(&list, FPNC);
  if (id == -1) {
    GST_ERROR("Unable to find FPNC for device '%s'", device);
    cleanControlList(&list);
    return FALSE;
  }

  /* need to perform a query to get elem size for device */
  if (queryExtControl(device, id, &qctrl) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to query FPNC control");
    cleanControlList(&list);
    return FALSE;
  }

  if(getExtControl(device, id, &controls) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to get FPNC control");
    cleanControlList(&list);
    return FALSE;
  }

  /* check that fpnc sizes match */
  if (qctrl.elems != data->elems) {
    GST_ERROR("FPNC from device and gstreamer do not have the "
      "same number of elements, device: %d, gstreamer:%d",
      qctrl.elems, data->elems);
    return FALSE;
  }

  dptr.ptr = controls->controls->ptr;

  if (qctrl.elem_size == 1 && data->elem_size == 1) {
    for (i=0; i<data->elems; i++) {
      if (dptr.d8[i] != data->d8[i]) {
        GST_ERROR("Uneven fpnc detected for index %d, %d != %d",
          i, dptr.d8[i], data->d8[i]);
        return FALSE;
      }
    }
  }
  else if (qctrl.elem_size == 2 && data->elem_size == 2) {
    for (i=0; i<data->elems; i++) {
      if (dptr.d16[i] != data->d16[i]) {
        GST_ERROR("Uneven fpnc detected for index %d, %d != %d",
          i, dptr.d16[i], data->d16[i]);
        return FALSE;
      }
    }
  }
  else if (qctrl.elem_size == 4 && data->elem_size == 4) {
    for (i=0; i<data->elems; i++) {
      if (dptr.d32[i] != data->d32[i]) {
        GST_ERROR("Uneven fpnc detected for index %d, %d != %d",
          i, dptr.d32[i], data->d32[i]);
        return FALSE;
      }
    }
  }
  else {
    GST_ERROR("Incompatible element sizes for fpnc, device gave size '%d' "
      "when fpncsink gave size of '%d'", qctrl.elem_size, data->elem_size);
  }
  for (i=0; i<data->elems; i++) {

  }

  if (setExtControl(device, controls) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to get FPNC control");
    cleanExtControls(controls);
    cleanControlList(&list);
    return FALSE;
  }

  cleanExtControls(controls);
  cleanControlList(&list);
  return TRUE;
}

gboolean
reset_fpnc(FpncData *data) {
  V4l2ControlList list;
  struct v4l2_ext_controls* controls;
  struct v4l2_query_ext_ctrl qctrl;
  int id, i;
  union {
    guint8 * d8;
    guint16 * d16;
    guint32 * d32;
    gpointer ptr;
  } dptr;

  list = getControlList(device);
  id = getControlId(&list, FPNC);
  if (id == -1) {
    GST_ERROR("Unable to find FPNC for device '%s'", device);
    cleanControlList(&list);
    return FALSE;
  }

  /* need to perform a query to see what is the default value */
  if (queryExtControl(device, id, &qctrl) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to query FPNC control");
    cleanControlList(&list);
    return FALSE;
  }

  if(getExtControl(device, id, &controls) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to get FPNC control");
    cleanControlList(&list);
    return FALSE;
  }
  dptr.ptr = controls->controls->ptr;
  for (i=0; i<data->elems; i++) {
      if (qctrl.elem_size == 1)
        dptr.d8[i] = qctrl.default_value;
      else if (qctrl.elem_size == 2)
        dptr.d16[i] = qctrl.default_value;
      else if (qctrl.elem_size == 4)
        dptr.d32[i] = qctrl.default_value;
  }

  if (setExtControl(device, controls) != V4L2_INTERFACE_OK) {
    GST_ERROR("Unable to get FPNC control");
    cleanExtControls(controls);
    cleanControlList(&list);
    return FALSE;
  }

  cleanExtControls(controls);
  cleanControlList(&list);
  return TRUE;
}

static void
on_calibration_completed (GstElement *element,
              gboolean success,
              gpointer fpncarray,
              guint    fpncdatasize,
              guint    fpncelems,
              gpointer fpncdata)
{
  GST_DEBUG("on_calibration_completed called");
  FpncData *data = fpncdata;
  signal_not_emitted = FALSE;
  if (success) {
    data->ptr = malloc(fpncdatasize);
    memcpy (data->ptr, fpncarray, fpncdatasize);
    data->data_size = fpncdatasize;
    data->elems = fpncelems;
    data->elem_size = fpncdatasize / fpncelems;
    data->valid = TRUE;
  }
  else {
    data->valid = FALSE;
  }
}

GST_START_TEST (test_fpncsink_verify_fpnc_change)
{
  GstElement *pipe, *fpncmagic, *fpncsink, *v4l2control;
  GstCaps *in_caps;
  GstPad *pad_src_peer, *src_pad;
  GstBuffer *progenitor_buffer, *buffer;
  GstFlowReturn res;
  FpncData data;
  pipe = gst_check_setup_element ("pipeline");

  fpncsink = gst_check_setup_element ("fpncsink");
  fpncmagic = gst_check_setup_element ("fpncmagic");
  v4l2control = gst_check_setup_element ("v4l2control");
  ck_assert_msg (gst_bin_add (GST_BIN (pipe), fpncmagic));
  ck_assert_msg (gst_bin_add (GST_BIN (pipe), fpncsink));
  ck_assert_msg (gst_bin_add (GST_BIN (pipe), v4l2control));
  g_object_set(v4l2control, "device", device, NULL);

  ck_assert_msg (gst_element_link_many (v4l2control, fpncmagic, fpncsink, NULL),
    "Unable to perform link");

  /* attatch signal to callback */
  g_signal_connect (fpncsink, "fpncsink-fpnc-calculated",
    G_CALLBACK (on_calibration_completed), &data);

  // Make sure to store default values, to restore camera after testing
  // or alterantive set fpnc to default

  /*caps init */
  in_caps = gst_caps_new_simple ("video/x-raw",
        "width", G_TYPE_INT, 10,
        "height", G_TYPE_INT, 1,
        "framerate", GST_TYPE_FRACTION, 1, 1,
        "format", G_TYPE_STRING, "GRAY16_BE",
      NULL);
  ck_assert_msg (GST_IS_CAPS (in_caps));

  progenitor_buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, junk_data, sizeof(junk_data),
        0, sizeof(junk_data), NULL, NULL);

  src_pad = gst_pad_new ("src", GST_PAD_SRC);
  ck_assert_msg (GST_IS_PAD (src_pad));
  gst_pad_set_active (src_pad, TRUE);
  gst_check_setup_events (src_pad, v4l2control, in_caps, GST_FORMAT_BYTES);
  /* get the corresponding sink and link them together */
  pad_src_peer = gst_element_get_static_pad (v4l2control, "sink");
  ck_assert_msg (GST_IS_PAD (pad_src_peer));
  ck_assert_msg (gst_pad_link (src_pad, pad_src_peer) == GST_PAD_LINK_OK,
    "Could not link source and %s sink pads", GST_ELEMENT_NAME (v4l2control));


  ck_assert_msg (gst_element_set_state (pipe, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
    "Unable to set state to playing");
  /*send buffers downstream until we get a calibration complete signal */
  signal_not_emitted = TRUE;
  while (signal_not_emitted) {
    buffer = gst_buffer_copy (progenitor_buffer);
     ck_assert_msg (GST_IS_BUFFER(buffer), "Unable to allocate buffer of size: %d", sizeof(junk_data));
    res = gst_pad_push (src_pad, buffer);
    if (res == GST_FLOW_EOS)
      GST_INFO("Got EOS from pipeline");
    else if (res == GST_FLOW_ERROR || res == GST_FLOW_CUSTOM_ERROR ||
        res == GST_FLOW_CUSTOM_ERROR_1 || res == GST_FLOW_CUSTOM_ERROR_2){
      ck_abort_msg ( "Unable to push buffer");
    }
    increment_junk();
  }
  /* check that signal emission was performed*/
  ck_assert_msg(!signal_not_emitted, "Pipe stopped running without signal emmision happening");
  /* verify that the values provided by the signal and the ones in the control are the same */
  /* First check that data is valid */
  ck_assert_msg(data.valid, "FPNC was not calibrated by fpncsink");

  /* next, compare the fpncs. */
  compare_fpnc(&data);
  /* */

  ck_assert_msg (gst_element_set_state (fpncsink,
        GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* finally, reset the fpnc */
  reset_fpnc(&data);
  free (data.ptr);
  gst_buffer_unref (progenitor_buffer);
  gst_object_unref (pad_src_peer);
  gst_check_teardown_element(pipe);
  gst_caps_unref (in_caps);
  g_object_unref (src_pad);
}

GST_END_TEST;



static Suite *
fpncsink_suite (void)
{
  Suite *s = suite_create ("fpncsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  if (is_fpnc_available()) {
    tcase_add_test (tc_chain, test_fpncsink_verify_fpnc_change);
  }

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
  s = fpncsink_suite ();
  return gst_check_run_suite (s, "fpncsink", __FILE__);
}
