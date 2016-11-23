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
 * SECTION:element-gstfpncsink
 *
 * The fpncsink element is used to calibrate the fpnc for a device.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v v4l2src device=/dev/qt5023_video1 ! v4l2control device=/dev/qt5023_video0 !
                  avgframes frameno=10 ! avgrow total-avg=1 ! fpncmagic ! fpncsink
 * ]|
 * This pipeline averages 10 frames, then creates a row with all the column averages, then calculates
 * the fpnc magic and finally calibrates the fpnc
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <gst/fpncmagic/gstfpncmagicmeta.h>
#include <gst/v4l2/gstv4l2commonutils.h>
#include <gst/v4l2/gstv4l2Queries.h>
#include "gstfpncsink.h"


GST_DEBUG_CATEGORY_STATIC (gst_fpnc_sink_debug_category);
#define GST_CAT_DEFAULT gst_fpnc_sink_debug_category

/* prototypes */


static void gst_fpnc_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_fpnc_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_fpnc_sink_dispose (GObject * object);
static void gst_fpnc_sink_finalize (GObject * object);

static GstFlowReturn gst_fpnc_sink_show_frame (GstVideoSink * video_sink,
    GstBuffer * buf);

static gboolean gst_fpnc_sink_set_caps (GstBaseSink * sink, GstCaps * caps);
static gboolean gst_fpnc_sink_start (GstBaseSink * sink);
static gboolean gst_fpnc_sink_stop  (GstBaseSink * sink);
static GstCaps *gst_fpnc_sink_fixate_caps (GstBaseSink *sink, GstCaps *caps);

enum
{
  PROP_0,
  PROP_THROW_EOS,
  PROP_WRITE_TO_FILE,
  PROP_FILE_PATH,
  PROP_FILE_NAME
};

/* signals and args */
enum
{
  SIGNAL_FPNC_CALCULATED,
  LAST_SIGNAL
};

static guint gst_fpnc_sink_signals[LAST_SIGNAL] = { 0 };

/* pad templates */

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ GRAY16_LE, GRAY16_BE }")


/* defaults */
#define DEFAULT_THROW_EOS TRUE
#define DEFAULT_WRITE_TO_FILE FALSE
#define DEFAULT_FILE_PATH "./"
#define DEFAULT_FILE_NAME "calced_fpnc"

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstFpncSink, gst_fpnc_sink, GST_TYPE_VIDEO_SINK,
  GST_DEBUG_CATEGORY_INIT (gst_fpnc_sink_debug_category, "fpncsink", 0,
  "debug category for fpncsink element"));

static void
gst_fpnc_sink_class_init (GstFpncSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);


  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "FPNC calibration sink", "Generic", "A sink that calibrates the FPNC",
      "Dimitrios Katsaros <patcherwork@gmail.com>");

  gobject_class->set_property = gst_fpnc_sink_set_property;
  gobject_class->get_property = gst_fpnc_sink_get_property;
  gobject_class->dispose = gst_fpnc_sink_dispose;
  gobject_class->finalize = gst_fpnc_sink_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_fpnc_sink_show_frame);
  base_sink_class->set_caps = gst_fpnc_sink_set_caps;
  base_sink_class->start    = gst_fpnc_sink_start;
  base_sink_class->stop     = gst_fpnc_sink_stop;
  base_sink_class->fixate   = gst_fpnc_sink_fixate_caps;

  /* signal definition */
  gst_fpnc_sink_signals[SIGNAL_FPNC_CALCULATED] = g_signal_new ("fpncsink-fpnc-calculated",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 4, G_TYPE_BOOLEAN,  G_TYPE_POINTER,
      G_TYPE_UINT, G_TYPE_UINT);

  /* parameter definition */
  g_object_class_install_property (gobject_class, PROP_THROW_EOS,
    g_param_spec_boolean ("throw-eos", "Throw End Of Stream",
        "End of stream is thrown at the end of the calibration", DEFAULT_THROW_EOS,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WRITE_TO_FILE,
    g_param_spec_boolean ("writefpn", "Write FPNC",
        "Write calculated FPNC to file", DEFAULT_WRITE_TO_FILE,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FILE_PATH,
    g_param_spec_string ("filepath", "File Path",
        "The location where the fpn results file is written to", DEFAULT_FILE_PATH,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FILE_NAME,
    g_param_spec_string ("filename", "File Name",
        "The name of the file that the fpnc is written to", DEFAULT_FILE_NAME,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
}

static void
gst_fpnc_sink_init (GstFpncSink *fpncsink)
{
  fpncsink->exposure_range_min = 1000;
  fpncsink->exposure_range_max = 50001;
  fpncsink->exposure_range_step = 1000;
  fpncsink->calc_errors = NULL;
  fpncsink->rolling_medians = NULL;
  fpncsink->calibration_complete = FALSE;
  fpncsink->throw_eos_flag = DEFAULT_THROW_EOS;
  fpncsink->filepath = g_string_new(DEFAULT_FILE_PATH);
  fpncsink->filename = g_string_new(DEFAULT_FILE_NAME);
  fpncsink->write_to_file = DEFAULT_WRITE_TO_FILE;
}

void
gst_fpnc_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFpncSink *fpncsink = GST_FPNC_SINK (object);

  GST_DEBUG_OBJECT (fpncsink, "set_property");

  switch (property_id) {
    case PROP_THROW_EOS:
      fpncsink->throw_eos_flag = g_value_get_boolean (value);
      break;
    case PROP_WRITE_TO_FILE:
      fpncsink->write_to_file = g_value_get_boolean (value);
      break;
    case PROP_FILE_PATH:
      if (fpncsink->filepath)
        g_string_free(fpncsink->filepath, TRUE);
      fpncsink->filepath = g_string_new(g_value_get_string(value));
      break;
    case PROP_FILE_NAME:
      if (fpncsink->filename)
        g_string_free(fpncsink->filename, TRUE);
      fpncsink->filename = g_string_new(g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_fpnc_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFpncSink *fpncsink = GST_FPNC_SINK (object);

  GST_DEBUG_OBJECT (fpncsink, "get_property");

  switch (property_id) {
    case PROP_THROW_EOS:
      g_value_set_boolean(value, fpncsink->throw_eos_flag);
      break;
    case PROP_WRITE_TO_FILE:
      g_value_set_boolean(value, fpncsink->write_to_file);
      break;
    case PROP_FILE_PATH:
      g_value_set_string(value, fpncsink->filepath->str);
      break;
    case PROP_FILE_NAME:
      g_value_set_string(value, fpncsink->filename->str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_fpnc_sink_dispose (GObject * object)
{
  GstFpncSink *fpncsink = GST_FPNC_SINK (object);

  GST_DEBUG_OBJECT (fpncsink, "dispose");

  fpncsink->exposure_was_set = FALSE;
  fpncsink->max_fpnc_no = 0;

  fpncsink->fpnc_no = 0;
  fpncsink->skip_first = TRUE;
  fpncsink->calibration_complete = FALSE;

  if (fpncsink->calc_errors)
    free(fpncsink->calc_errors);
  fpncsink->calc_errors = NULL;
  if (fpncsink->rolling_medians)
    free(fpncsink->rolling_medians);
  fpncsink->rolling_medians = NULL;
  if (fpncsink->filepath)
    g_string_free(fpncsink->filepath, TRUE);
  fpncsink->filepath = NULL;
  if (fpncsink->filename)
    g_string_free(fpncsink->filename, TRUE);
  fpncsink->filename = NULL;

  G_OBJECT_CLASS (gst_fpnc_sink_parent_class)->dispose (object);
}

void
gst_fpnc_sink_finalize (GObject * object)
{
  GstFpncSink *fpncsink = GST_FPNC_SINK (object);

  GST_DEBUG_OBJECT (fpncsink, "finalize");

  if (fpncsink->calc_errors)
    free(fpncsink->calc_errors);
  fpncsink->calc_errors = NULL;
  if (fpncsink->rolling_medians)
    free(fpncsink->rolling_medians);
  fpncsink->rolling_medians = NULL;
  if (fpncsink->filepath)
    g_string_free(fpncsink->filepath, TRUE);
  fpncsink->filepath = NULL;
  if (fpncsink->filename)
    g_string_free(fpncsink->filename, TRUE);
  fpncsink->filename = NULL;

  G_OBJECT_CLASS (gst_fpnc_sink_parent_class)->finalize (object);
}

static GstCaps *
gst_fpnc_sink_fixate_caps (GstBaseSink *sink, GstCaps *caps)
{

  GstStructure *structure;
  GstCaps *newcaps;
  GstStructure *newstruct;

  structure = gst_caps_get_structure (caps, 0);
  newcaps = gst_caps_new_empty_simple ("video/x-raw");
  newstruct = gst_caps_get_structure (newcaps, 0);


  gst_structure_set_value (newstruct, "width",
      gst_structure_get_value (structure, "width"));

  gst_structure_set(newstruct, "height", G_TYPE_INT, 1, NULL);

  gst_structure_set_value (newstruct, "framerate",
      gst_structure_get_value (structure, "framerate"));

  gst_structure_set_value (newstruct, "format",
      gst_structure_get_value (structure, "format"));

  return newcaps;
}
static gboolean
gst_fpnc_sink_start (GstBaseSink * sink) {

  GstFpncSink *fpncsink = GST_FPNC_SINK (sink);

  fpncsink->exposure_was_set = FALSE;
  fpncsink->max_fpnc_no = ((fpncsink->exposure_range_max - fpncsink->exposure_range_min) /
    fpncsink->exposure_range_step);

  fpncsink->fpnc_no = 0;
  fpncsink->skip_first = TRUE;
  fpncsink->calibration_complete = FALSE;

  if (fpncsink->calc_errors)
    free(fpncsink->calc_errors);
  fpncsink->calc_errors = NULL;
  if (fpncsink->rolling_medians)
    free(fpncsink->rolling_medians);
  fpncsink->rolling_medians = NULL;

  if (fpncsink->filepath->str[fpncsink->filepath->len-1] != '/')
    g_string_append_c(fpncsink->filepath, '/');

  return TRUE;
}

static gboolean
gst_fpnc_sink_stop (GstBaseSink * sink) {

  GstFpncSink *fpncsink = GST_FPNC_SINK (sink);

  fpncsink->max_fpnc_no = 0;

  GST_DEBUG("Sink Stop");


  if (fpncsink->calc_errors)
    free(fpncsink->calc_errors);
  fpncsink->calc_errors = NULL;
  if (fpncsink->rolling_medians)
    free(fpncsink->rolling_medians);
  fpncsink->rolling_medians = NULL;

  return TRUE;
}

static gboolean
gst_fpnc_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{

  GstFpncSink *fpncsink = GST_FPNC_SINK (sink);
  GstVideoInfo info;

  GST_DEBUG("Caps info: %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  fpncsink->info = info;

  if (info.height != 1) {
    GST_ERROR("Height is not 1");
    return FALSE;
  }


  if (fpncsink->calc_errors)
    free(fpncsink->calc_errors);

  if (fpncsink->rolling_medians)
    free(fpncsink->rolling_medians);

  fpncsink->calc_errors = malloc(fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gint));

  if (!fpncsink->calc_errors) {
    GST_ERROR("Unable to allocate memory of size %lu", fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gint));
    return FALSE;
  }


  fpncsink->rolling_medians = malloc(fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gint));

  if (!fpncsink->rolling_medians) {
    GST_ERROR("Unable to allocate memory of size %lu", fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gint));
    free(fpncsink->calc_errors);
    return FALSE;
  }

  return TRUE;

invalid_caps:
  {
    GST_ERROR_OBJECT (fpncsink, "invalid caps");
    return FALSE;
  }
}

static gboolean
gst_write_to_file(GstFpncSink *fpncsink, gint *fpnc_val) {

  int i;
  FILE *fpncFile = NULL;
  gint filepath_len = fpncsink->filepath->len;
  GString *filepath = fpncsink->filepath;

  if (access(filepath->str, F_OK) == -1) {
    if (mkdir(filepath->str, 0700) == -1) {
      GST_ERROR("An error occured while trying to create dir %s", filepath->str);
      GST_ERROR("Reason: %s", g_strerror (errno));
      goto error;
    }
  }
  g_string_append(filepath, fpncsink->filename->str);
  if ((fpncFile = fopen(filepath->str, "w")) == NULL)  {
    GST_ERROR("An error occured while trying to create file %s", filepath->str);
    goto error;
  }

  for (i=0; i<fpncsink->fpnc_elems; i++)
    fprintf(fpncFile, "%d\n", fpnc_val[i]);

  fclose(fpncFile);
  g_string_truncate(filepath, filepath_len);
  return TRUE;

error:
  {
    if (fpncFile)
      fclose(fpncFile);
    g_string_truncate(filepath, filepath_len);
    return FALSE;
  }
}

#define EXPOSURE "Exposure Time, Absolute"
#define FPNC     "Fixed Pattern Noise Correction"

#define MINIMUM_DN 9766
#define MAXIMUM_DN 51400

static GstFlowReturn
gst_fpnc_sink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstFpncSink *fpncsink = GST_FPNC_SINK (sink);

  GstVideoFrame frame;
  GstQuery *query;
  gboolean res;
  gint i, j, image_avg;
  GValue sval = G_VALUE_INIT;
  GValue *val;
  const GValue *cval = NULL;
  GstMapFlags flags = GST_MAP_READ;
  gint *fpnc_ptr, *fpnc_ptr2;
  gint *fpnc_ptr3, *fpnc_ptr4;

  //Hack for T#643, discarding first buffer in case it is a prerolled buffer
  if (fpncsink->skip_first) {
    fpncsink->skip_first = FALSE;
    return GST_FLOW_OK;
  }

  /* flag for not running if calibratio has been completed */
  if (fpncsink->calibration_complete) {
    GST_DEBUG("Calibration has been completed, passing");
    return GST_FLOW_OK;
  }

  if (!gst_video_frame_map (&frame, &fpncsink->info, buf, flags))
      goto invalid_buffer;

  GstFpncMagicMeta *fpncmeta =
    (GstFpncMagicMeta *) gst_buffer_get_gst_fpnc_magic_meta (buf);

  if (!fpncmeta) {
    GST_ERROR("FPNC sink requires fpnc magic metadata to function correctly");
    return GST_FLOW_ERROR;
  }

  if (fpncmeta->data_size != fpncsink->info.width){
    GST_ERROR("Metatdata length is not equal to fpncsink width: %d != %d",
      fpncmeta->data_size, fpncsink->info.width);
    return GST_FLOW_ERROR;

  }

  /* this is the first time we are running */
  if (!fpncsink->exposure_was_set) {
    /* Get all required info from the device
     * query fpnc */
    query = gst_v4l2_queries_new_control_info(FPNC, 0);
    res = gst_element_query(GST_ELEMENT(sink), query);
    if (!res) {
      GST_ERROR("was unable to query control %s", FPNC);
      gst_query_unref (query);
      return GST_FLOW_ERROR;
    }
    gst_v4l2_queries_parse_control_extended_info(query, NULL, NULL, &fpncsink->fpnc_min,
      &fpncsink->fpnc_max, NULL, &fpncsink->fpnc_denominator, NULL, NULL, &fpncsink->fpnc_elems,
      NULL, NULL);
    gst_query_unref (query);

    GST_DEBUG("%s info: fpnc_min:%d, fpnc_max:%d, denominator:%d, row_no:%d", FPNC,
      fpncsink->fpnc_min, fpncsink->fpnc_max, fpncsink->fpnc_denominator, fpncsink->fpnc_elems);

    /* set the fpnc to default */
    GST_DEBUG("Setting fpnc to default");
    gint *fpnc_val  = malloc(fpncsink->fpnc_elems * sizeof(gint));
    for (j=0; j < fpncsink->fpnc_elems; j++)
      fpnc_val[j] = fpncsink->fpnc_denominator;
    val = gst_v4l2_new_value_array(&sval, fpnc_val, fpncsink->fpnc_elems, sizeof(gint));
    query = gst_v4l2_queries_new_set_control(FPNC, 0, val);
    res = gst_element_query(GST_ELEMENT(sink), query);
    free(fpnc_val);
    if(!res) {
      GST_ERROR("Unable to set FPN");
      gst_query_unref (query);
      return GST_FLOW_ERROR;
    }
    gst_query_unref (query);
    g_value_unset (&sval);

    /* query exposure */
    query = gst_v4l2_queries_new_control_info(EXPOSURE, 0);
    res = gst_element_query(GST_ELEMENT(sink), query);
    if (!res) {
      GST_ERROR("was unable to query control %s", EXPOSURE);
      gst_query_unref (query);
      return GST_FLOW_ERROR;
    }
    gst_v4l2_queries_parse_control_info(query, NULL, NULL, &fpncsink->exposure_min,
      &fpncsink->exposure_max, NULL, NULL, NULL);
    gst_query_unref (query);

    GST_DEBUG("%s info: min:%d, max:%d", EXPOSURE, fpncsink->exposure_min,
      fpncsink->exposure_max);

    query = gst_v4l2_queries_new_get_control(EXPOSURE, 0);
    res = gst_element_query(GST_ELEMENT(sink), query);
    if (!res) {
      GST_ERROR("was unable to get control %s", EXPOSURE);
      gst_query_unref (query);
      return GST_FLOW_ERROR;
    }

    gst_v4l2_queries_parse_get_control(query, NULL, NULL, &cval);
    fpncsink->starting_exposure = g_value_get_int (cval);
    gst_query_unref (query);
    GST_DEBUG("Starting exposure %d", fpncsink->starting_exposure);

    /* set exposure range */
    if (fpncsink->exposure_min > fpncsink->exposure_range_min)
      fpncsink->exposure_range_min = fpncsink->exposure_min;
    if (fpncsink->exposure_max < fpncsink->exposure_range_max)
      fpncsink->exposure_range_max = fpncsink->exposure_max;
    fpncsink->exposure_last_val = fpncsink->exposure_range_min;
    fpncsink->exposure_was_set = TRUE;
  }
  /* we got a new result frame for the previously set exposure. */
  else {
    image_avg = fpncmeta->avg;
    /* if the exposure is below the minimum or above the maximum, continue */
    if (image_avg < MINIMUM_DN && fpncsink->exposure_last_val < fpncsink->exposure_range_max )
      GST_DEBUG("DN is at %d < %d, ignoring", image_avg, 9766);
    /*else if(image_avg > MAXIMUM_DN && fpncsink->exposure_last_val < fpncsink->exposure_range_max)
      GST_DEBUG("DN is at %d > %d, ignoring", image_avg, 51400); */
    /* if the exposure is above the maximum or we reached the upper limit */
    else if (image_avg > MAXIMUM_DN || fpncsink->exposure_last_val >= fpncsink->exposure_range_max) {
      GST_DEBUG("Maximum device exposure reached");

      /* ensure that at least 2 errors were calculated */
      if (fpncsink->fpnc_no >= 2) {

        /* allocate memory for storing slopes and avg */
        guint fpncsize = fpncsink->fpnc_elems * sizeof(gint);
        gfloat *dmatrix = malloc(fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gfloat));
        memset(dmatrix, 0, fpncsink->info.width * fpncsink->max_fpnc_no * sizeof(gfloat));
        gint *fpnc_val  = malloc(fpncsize);
        memset(fpnc_val, 0, fpncsize);
        gfloat diff;

        /* calculate the response curve slopes. They are stored in the same memory as the first error */
        fpnc_ptr = fpncsink->calc_errors;
        fpnc_ptr3 = fpncsink->rolling_medians;
        for (i=1; i<fpncsink->fpnc_no; i++){
          fpnc_ptr2 = fpncsink->calc_errors + (i * fpncsink->info.width);
          fpnc_ptr4 = fpncsink->rolling_medians + (i * fpncsink->info.width);

          for (j=0; j<fpncsink->info.width; j++) {
            /*calc the difference */
            diff = (fpnc_ptr4[j] - fpnc_ptr3[j]);
            if (diff == 0)
              diff = 1;
            /* divide by the intensity */
            dmatrix[j+((i-1) * fpncsink->info.width)] = (fpnc_ptr2[j] - fpnc_ptr[j]) / diff;
          }

          fpnc_ptr = fpnc_ptr2;
          fpnc_ptr3 = fpnc_ptr4;
        }

        /* calculate the average fpn slope for every column it is stored in the first row */
        for (j=0; j<fpncsink->info.width; j++){
          for (i=1; i<fpncsink->fpnc_no; i++) {
            dmatrix[j] += dmatrix[j+(i * fpncsink->info.width)];
          }
          dmatrix[j] /= fpncsink->fpnc_no;
        }

        /* calculate the slope fractions */
        for (j=0; j<fpncsink->info.width; j++) {

          fpnc_val[j] = (1-dmatrix[j]) * fpncsink->fpnc_denominator;
          GST_DEBUG("fpnc column %d, fraction %f, value %d", j, dmatrix[j], fpnc_val[j]);
          if (fpnc_val[j] > fpncsink->fpnc_max) {
            GST_DEBUG("FPNC for column %d is above max with value: %d, setting to max: %d",
              j, fpnc_val[j], fpncsink->fpnc_max);
            fpnc_val[j] = fpncsink->fpnc_max;
          }
          else if (fpnc_val[j] < fpncsink->fpnc_min) {
            GST_DEBUG("FPNC for column %d is below min with value: %d, setting to min: %d",
              j, fpnc_val[j], fpncsink->fpnc_min);
            fpnc_val[j] = fpncsink->fpnc_min;
          }
        }

        if (fpncsink->info.width < fpncsink->fpnc_elems) {
          /* set the rest of the fpnc values to default */
          GST_DEBUG("Setting values from %d to %d to %d", fpncsink->info.width, fpncsink->fpnc_elems-1,
            fpncsink->fpnc_denominator);
          for (j=fpncsink->info.width; j < fpncsink->fpnc_elems; j++) {
            fpnc_val[j] = fpncsink->fpnc_denominator;
          }
        }
        /* set the fpnc v4l2 control */
        val = gst_v4l2_new_value_array(&sval, fpnc_val, fpncsink->fpnc_elems, sizeof(gint));
        query = gst_v4l2_queries_new_set_control(FPNC, 0, val);
        res = gst_element_query(GST_ELEMENT(sink), query);
        if(res) {
          GST_DEBUG("FPN successfully set");
        }
        else {
          GST_ERROR("Unable to set FPN");
        }
        gst_query_unref (query);

        g_signal_emit (sink, gst_fpnc_sink_signals[SIGNAL_FPNC_CALCULATED], 0, res, fpnc_val, fpncsize, fpncsink->fpnc_elems);

        g_value_unset (&sval);

        /* write fpn to file */
        if (fpncsink->write_to_file) {
          GST_DEBUG("Writing fpn to file");
          if (!gst_write_to_file(fpncsink, fpnc_val))
            GST_DEBUG("Failed to write file");
        }
        /* cleanup */

        free(dmatrix);
        free(fpnc_val);
      }
      else
        GST_WARNING("Only %d errors were calulated, at least 2 are required to calcualte the fpn", fpncsink->fpnc_no);

      /* set the exposure back to starting value */
      GST_DEBUG("Set exposure back to %d", fpncsink->starting_exposure);
      val = g_value_init (&sval, G_TYPE_INT);
      g_value_set_int(val, fpncsink->starting_exposure);
      query = gst_v4l2_queries_new_set_control(EXPOSURE, 0, val);
      res = gst_element_query(GST_ELEMENT(sink), query);
      if (!res) {
        GST_ERROR("was unable to set control %s", EXPOSURE);
        gst_query_unref (query);
        return GST_FLOW_ERROR;
      }
      gst_query_unref (query);

      GST_DEBUG("CALIBRATION COMPLETED");
      fpncsink->calibration_complete = TRUE;

      if (fpncsink->throw_eos_flag)
        return GST_FLOW_EOS;
      return GST_FLOW_OK;

    }
    /* if the exposure is within the dn limits */
    else {
      GST_DEBUG("DN is at %d", image_avg);
      /* store the error and the rolling medians */
      fpnc_ptr  = fpncsink->calc_errors + (fpncsink->fpnc_no * fpncsink->info.width);
      fpnc_ptr2 = fpncsink->rolling_medians + (fpncsink->fpnc_no * fpncsink->info.width);

      memcpy(fpnc_ptr, fpncmeta->error, fpncsink->info.width * sizeof(gint));
      memcpy(fpnc_ptr2, fpncmeta->rolling_median, fpncsink->info.width * sizeof(gint));
      /* increment the calculated fpncs */
      fpncsink->fpnc_no++;
    }
  }

  /* start a new exposure query */
  /* first, set exposure */
  GST_DEBUG("Set exposure to %d", fpncsink->exposure_last_val);
  val = g_value_init (&sval, G_TYPE_INT);
  g_value_set_int(val, fpncsink->exposure_last_val);
  fpncsink->exposure_last_val += fpncsink->exposure_range_step;

  query = gst_v4l2_queries_new_set_control(EXPOSURE, 0, val);
  gst_v4l2_queries_set_control_activate_flushing(query);
  res = gst_element_query(GST_ELEMENT(sink), query);
  if (!res) {
    GST_ERROR("was unable to set control %s", EXPOSURE);
    gst_query_unref (query);
    return GST_FLOW_ERROR;
  }
  gst_query_unref (query);

  /* finally, finish handling. The next frame should be under the new exposure */

  gst_video_frame_unmap (&frame);


    return GST_FLOW_OK;

invalid_buffer:
  {
    GST_ELEMENT_WARNING (fpncsink, CORE, NOT_IMPLEMENTED, (NULL),
        ("invalid video buffer received"));
    return GST_FLOW_OK;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "fpncsink", GST_RANK_NONE,
      GST_TYPE_FPNC_SINK);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "fpncsink"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "FPNC sink"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://qtec.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fpncsink,
    "Plugin for calibrating the FPNC",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

