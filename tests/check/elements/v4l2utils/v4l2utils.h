
#ifndef __GST_UTILS_V4L2__
#define __GST_UTILS_V4L2__


#include <linux/videodev2.h>

#include <gst/gst.h>

enum {
    V4L2_INTERFACE_OK  = 0,
    V4L2_INTERFACE_ERR = 1,
    V4L2_INTERFACE_ERR_VAL,
    V4L2_INTERFACE_ERR_BUSY
};

typedef struct V4l2Control
{
    char name[32];
    int id;
}V4l2Control;

typedef struct V4l2ControlList
{
    V4l2Control* controls;
    int nrControls;
}V4l2ControlList;

typedef struct v4l2formatlist
{
    struct v4l2_fmtdesc* formats;
    int nrFormats;
}V4l2FormatList;

V4l2ControlList getControlList(char* device);
int setControl(char* device, int control_id, int control_value);
int setControlUnchecked(char* device, struct v4l2_control *control);
int setControlByName(char* device, V4l2ControlList* control_list, char* control_name, int control_value);
int getControlId(V4l2ControlList* control_list, char* control_name);
int getControl(char* device, int control_id, int* control_value);
int queryControl(char* device, int control_id, struct v4l2_queryctrl * qctrl);

int getFormatList(char* device, V4l2FormatList *fmtlist);
int tryfmt(char* device, struct v4l2_format *try_fmt);

int getcropcap(char* device, struct v4l2_cropcap *cc);

/* extended controls */
int queryExtControl(char* device, int control_id, struct v4l2_query_ext_ctrl * qctrl);
int getExtControl(char* device, int control_id, struct v4l2_ext_controls** controls);
int setExtControl(char* device, struct v4l2_ext_controls *controls);
void cleanExtControls(struct v4l2_ext_controls *controls);

void cleanControlList(V4l2ControlList* list);
void cleanFmtList(V4l2FormatList* list);

#endif