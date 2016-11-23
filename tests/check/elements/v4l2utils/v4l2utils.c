
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "libv4l2.h"
#include "v4l2utils.h"


#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define v4l2_ioctl ioctl

static int
openDevice(char* device_file)
{
    struct stat st;

    //test file
    if (-1 == stat((char *)device_file, &st)) {
        GST_ERROR("Cannot identify '%s': %d, %s\n", (char *)device_file, errno, strerror(errno));
        return -1;
    }
    if (!S_ISCHR(st.st_mode)) {
        GST_ERROR("%s is no device\n", (char *)device_file);
        return -1;
    }

    //open device
    //do not use v4l2_open(), there is some bug which does not close the file
    int fd = open((char *)device_file, O_RDWR | O_NONBLOCK, 0);
    if(fd < 0){
        GST_ERROR("Failed to open [%s]: %s\n", (char *)device_file, strerror(errno));
        return -1;
    }
    //printf("Device [%s] open \n", (char *)device_file);

    return fd;
}


static void
closeDevice(int fd)
{
    close(fd);
}

int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = v4l2_ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

void cleanControlList(V4l2ControlList* list)
{
    if(list->controls != NULL)
    {
        free(list->controls);
        list->controls = NULL;
        list->nrControls = 0;
    }
}

void cleanFmtList(V4l2FormatList* list)
{
    if(list->formats != NULL)
    {
        free(list->formats);
        list->formats = NULL;
        list->nrFormats = 0;
    }
}

int getFormatList(char* device, V4l2FormatList *fmt_list)
{
    fmt_list->formats = NULL;
    fmt_list->nrFormats = 10;
    int n = 0;

    struct v4l2_fmtdesc fmtd;
    struct v4l2_fmtdesc *temp;

    fmt_list->formats = malloc(fmt_list->nrFormats * sizeof(struct v4l2_fmtdesc));
    if(fmt_list->formats == NULL)
    {
        g_print("Error: could not allocate format list array to size=%d\n", fmt_list->nrFormats);
        fmt_list->nrFormats = 0;
        return V4L2_INTERFACE_ERR;
    }


    int fd=openDevice(device);
    if(fd<0){
        g_print("Error: could not open device=%s\n", device);
        cleanFmtList(fmt_list);
        return V4L2_INTERFACE_ERR;
    }

    CLEAR(fmtd);
    fmtd.index = 0;
    n = 0;
    fmtd.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (TRUE) {
        if (xioctl(fd, VIDIOC_ENUM_FMT, &fmtd) != 0){
            if (errno == EINVAL) {
                /* completed reading formats */
                break;
            }

            g_print("Error with reading format from device");
            cleanFmtList(fmt_list);
            return V4L2_INTERFACE_ERR;
        }

        if(fmt_list->nrFormats == fmtd.index) {
            fmt_list->nrFormats += 10;
            temp = realloc(fmt_list->formats, fmt_list->nrFormats * sizeof(struct v4l2_fmtdesc));
            if (temp == NULL) {
                g_print("Error: could not reallocate format array of size=%d\n", fmt_list->nrFormats);
                cleanFmtList(fmt_list);
                return V4L2_INTERFACE_ERR;
            }
            fmt_list->formats = temp;
        }

        memcpy (&fmt_list->formats[n], &fmtd, sizeof(struct v4l2_fmtdesc));

        n++;
        fmtd.index = n;
    }

    if(n>0){
        fmt_list->nrFormats = n;
        temp = realloc(fmt_list->formats, fmt_list->nrFormats*sizeof(struct v4l2_fmtdesc));
        if (temp == NULL) {
            g_print("Error: could not shrink format array to size=%d\n", fmt_list->nrFormats);
            cleanFmtList(fmt_list);
            return V4L2_INTERFACE_ERR;
        }
        fmt_list->formats = temp;
    }else{
        cleanFmtList(fmt_list);
    }

    return V4L2_INTERFACE_OK;
}


int
tryfmt (char* device,
    struct v4l2_format *try_fmt)
{
    int fd=openDevice(device);
    if(fd<0){
        g_print("Error: could not open device=%s\n", device);
        return V4L2_INTERFACE_ERR;
    }
    struct v4l2_format fmt;
    int r;

    memcpy (&fmt, try_fmt, sizeof (fmt));
    r = xioctl (fd, VIDIOC_TRY_FMT, &fmt);

    if (r < 0 && errno == ENOTTY) {
    /* The driver might not implement TRY_FMT, in which case we will try
       S_FMT to probe */
        memcpy (&fmt, try_fmt, sizeof (fmt));
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) != 0){
            return V4L2_INTERFACE_ERR;
        }
    }
    memcpy (try_fmt, &fmt, sizeof (fmt));
    return V4L2_INTERFACE_OK;
}


V4l2ControlList getControlList(char* device)
{
    //g_print("getControlList %s\n", device);

    V4l2ControlList control_list = {NULL, 10};
    int n = 0;
    control_list.controls = malloc(control_list.nrControls * sizeof(V4l2Control));
    if(control_list.controls == NULL)
    {
        g_print("Error: could not allocate control list array to size=%d\n", control_list.nrControls);
        control_list.nrControls = 0;
        return control_list;
    }

    struct v4l2_queryctrl qctrl;
    int id;

    int fd=openDevice(device);
    if(fd<0){
        g_print("Error: could not open device=%s\n", device);
        cleanControlList(&control_list);
        return control_list;
    }

    CLEAR(qctrl);
    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
        if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED) && !(qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS)){
            V4l2Control control = {.id = qctrl.id};
            //g_print("qctrl id=%d name=%s\n", qctrl.id, qctrl.name);
            strncpy(control.name, (char*)qctrl.name, 32);
            //g_print("control id=%d name=%s\n", control.id, control.name);
            if(n >= control_list.nrControls){
                control_list.nrControls += 10;
                V4l2Control* realloced_array = realloc(control_list.controls, control_list.nrControls * sizeof(V4l2Control));
                if (realloced_array) {
                    control_list.controls = realloced_array;
                }else{
                    g_print("Error: could not reallocate control list array to size=%d\n", control_list.nrControls);
                    closeDevice(fd);
                    cleanControlList(&control_list);
                    return control_list;
                }
            }
            control_list.controls[n] = control;
            n++;
        }
        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
    if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL){
        closeDevice(fd);
        if(n>0){
            control_list.nrControls = n;
            V4l2Control* realloced_array = realloc(control_list.controls, control_list.nrControls * sizeof(V4l2Control));
            if (realloced_array) {
                control_list.controls = realloced_array;
            }else{
                g_print("Error: could not reallocate control list array to size=%d\n", control_list.nrControls);
                cleanControlList(&control_list);
                return control_list;
            }
        }else{
            cleanControlList(&control_list);
        }

        /*int i;
        for(i=0; i<control_list.nrControls; i++){
            g_print("control_list[%d] %s\n", i, control_list.controls[i].name);
        }*/

        return control_list;
    }
    for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
        qctrl.id = id;
        if (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0){
            if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)){
                V4l2Control control = {.id = qctrl.id};
                strncpy(control.name, (char*)qctrl.name, 32);
                if(n >= control_list.nrControls){
                    control_list.nrControls += 10;
                    V4l2Control* realloced_array = realloc(control_list.controls, control_list.nrControls * sizeof(V4l2Control));
                    if (realloced_array) {
                        control_list.controls = realloced_array;
                    }else{
                        g_print("Error: could not reallocate control list array to size=%d\n", control_list.nrControls);
                        closeDevice(fd);
                        cleanControlList(&control_list);
                        return control_list;
                    }
                }
                control_list.controls[n] = control;
                n++;
            }
        }
    }
    for (qctrl.id = V4L2_CID_PRIVATE_BASE;
            xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
        if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)){
            V4l2Control control = {.id = qctrl.id};
            strncpy(control.name, (char*)qctrl.name, 32);
            if(n >= control_list.nrControls){
                control_list.nrControls += 10;
                V4l2Control* realloced_array = realloc(control_list.controls, control_list.nrControls * sizeof(V4l2Control));
                if (realloced_array) {
                    control_list.controls = realloced_array;
                }else{
                    g_print("Error: could not reallocate control list array to size=%d\n", control_list.nrControls);
                    closeDevice(fd);
                    cleanControlList(&control_list);
                    return control_list;
                }
            }
            control_list.controls[n] = control;
            n++;
        }
    }

    closeDevice(fd);

    if(n>0){
        control_list.nrControls = n;
        V4l2Control* realloced_array = realloc(control_list.controls, control_list.nrControls * sizeof(V4l2Control));
        if (realloced_array) {
            control_list.controls = realloced_array;
        }else{
            g_print("Error: could not reallocate control list array to size=%d\n", control_list.nrControls);
            cleanControlList(&control_list);
            return control_list;
        }
    }else{
        cleanControlList(&control_list);
    }

    return control_list;
}

int setControl(char* device, int control_id, int control_value)
{
    int success = V4L2_INTERFACE_OK;

    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    CLEAR(queryctrl);

    int fd = openDevice(device);
    if(fd < 0) return V4L2_INTERFACE_ERR;

    queryctrl.id = control_id;
    if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (errno != EINVAL) {
            perror ("VIDIOC_QUERYCTRL");
        } else {
            printf ("ID not supported\n");
        }
        success = V4L2_INTERFACE_ERR;
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf ("ID disabled\n");
        success = V4L2_INTERFACE_ERR;
    } else {
        CLEAR(control);
        control.id = control_id;
        control.value = control_value;

        if (-1 == xioctl (fd, VIDIOC_S_CTRL, &control)) {
            perror ("VIDIOC_S_CTRL");
            if(errno == EBUSY)
                success = V4L2_INTERFACE_ERR_BUSY;
            else
                success = V4L2_INTERFACE_ERR;
        }
    }

    closeDevice(fd);

    return success;
}

int getControl(char* device, int control_id, int* control_value)
{
    int success = V4L2_INTERFACE_OK;

    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    CLEAR(queryctrl);

    int fd = openDevice(device);
    if(fd < 0) return V4L2_INTERFACE_ERR;

    queryctrl.id = control_id;
    if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (errno != EINVAL) {
            perror ("VIDIOC_QUERYCTRL");
        } else {
            printf ("ID not supported\n");
        }
        success = V4L2_INTERFACE_ERR;
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf ("ID disabled\n");
        success = V4L2_INTERFACE_ERR;
    } else {

        CLEAR(control);
        control.id = control_id;
        if (-1 == xioctl (fd, VIDIOC_G_CTRL, &control)) {
            perror ("VIDIOC_G_CTRL");
            if(errno == EBUSY)
                success = V4L2_INTERFACE_ERR_BUSY;
            else
                success = V4L2_INTERFACE_ERR;
        }else{
            *control_value = control.value;
        }
    }

    closeDevice(fd);

    return success;
}


int queryControl(char* device, int control_id, struct v4l2_queryctrl * qctrl)
{
    int success = V4L2_INTERFACE_OK;

    struct v4l2_queryctrl queryctrl;
    CLEAR(queryctrl);

    int fd = openDevice(device);
    if(fd < 0) return V4L2_INTERFACE_ERR;

    queryctrl.id = control_id;
    if (-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        success = V4L2_INTERFACE_ERR;
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf ("ID disabled\n");
        success = V4L2_INTERFACE_ERR;
    } else {
        *qctrl = queryctrl;
    }

    closeDevice(fd);

    return success;
}

int setControlByName(char* device, V4l2ControlList* control_list, char* control_name, int control_value)
{
    int i;
    for(i=0; i<control_list->nrControls; i++){
        //g_print("control_list[%d] %s\n", i, control_list->controls[i].name);
        if(strcasecmp(control_list->controls[i].name, control_name) == 0){
            //g_print("found\n");
            return setControl(device, control_list->controls[i].id, control_value);
        }
    }

    //g_print("not found\n");
    return V4L2_INTERFACE_ERR;
}

int getControlId(V4l2ControlList* control_list, char* control_name)
{
    int i;

    if (control_name == NULL)
        return -1;
    for(i=0; i<control_list->nrControls; i++){
        if(strcasecmp(control_list->controls[i].name, control_name) == 0){
            return control_list->controls[i].id;
        }
    }

    return -1;
}

int setControlUnchecked(char* device, struct v4l2_control *control)
{

    int fd = openDevice(device);
    if(fd < 0) return fd;
    if (-1 == xioctl (fd, VIDIOC_S_CTRL, control)) {
        return errno;
    }

    closeDevice(fd);

    return 0;
}


/* Extended controls */

void cleanExtControls(struct v4l2_ext_controls *ctrls) {

    if (ctrls) {
        if(ctrls->controls) {
            if(ctrls->controls->ptr)
                free (ctrls->controls->ptr);
            free(ctrls->controls);
        }
        free(ctrls);
    }

}


static struct v4l2_ext_controls*
init_ext_controls(struct v4l2_query_ext_ctrl *qctrl) {


    struct v4l2_ext_controls * ctrls;
    struct v4l2_ext_control  * ctrl_array;

    ctrls = malloc(sizeof(struct v4l2_ext_controls));
    if(ctrls == NULL)
    {
        GST_ERROR("Could not allocate v4l2_ext_controls structure");
        return NULL;
    }
    memset(ctrls, 0, sizeof(struct v4l2_ext_controls));

    /* assuming that we will be handling a single control */
    ctrl_array = malloc(sizeof(struct v4l2_ext_control));
    if(ctrl_array == NULL)
    {
        GST_ERROR("Could not allocate v4l2_ext_control array structure of size %lu", sizeof(struct v4l2_ext_control));
        cleanExtControls (ctrls);
        return NULL;
    }
    memset(ctrl_array, 0, sizeof(struct v4l2_ext_control));

    ctrls->controls = ctrl_array;
    ctrls->count = 1;
    ctrl_array->ptr = malloc(qctrl->elems * qctrl->elem_size);
    if(ctrl_array->ptr == NULL)
    {
        GST_ERROR("Could not allocate extended control data of size %d", qctrl->elems * qctrl->elem_size);
        cleanExtControls (ctrls);
        return NULL;
    }
    memset(ctrl_array->ptr, 0, (qctrl->elems * qctrl->elem_size));

    ctrl_array->id = qctrl->id;
    ctrl_array->size = qctrl->elems * qctrl->elem_size;

    return ctrls;
}



int queryExtControl(char* device, int control_id, struct v4l2_query_ext_ctrl * qctrl)
{
    int success = V4L2_INTERFACE_OK;

    struct v4l2_query_ext_ctrl queryctrl;
    CLEAR(queryctrl);

    int fd = openDevice(device);
    if(fd < 0) return V4L2_INTERFACE_ERR;

    queryctrl.id = control_id;
    if (-1 == xioctl (fd, VIDIOC_QUERY_EXT_CTRL, &queryctrl)) {
        success = errno;
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf ("ID disabled\n");
        success = V4L2_INTERFACE_ERR;
    } else {
        *qctrl = queryctrl;
    }

    closeDevice(fd);

    return success;
}

int getExtControl(char* device, int control_id, struct v4l2_ext_controls** ret_controls) {

    int success = V4L2_INTERFACE_OK;

    struct v4l2_query_ext_ctrl queryctrl;
    struct v4l2_ext_controls *controls;
    CLEAR(queryctrl);

    int fd = openDevice(device);
    if(fd < 0) return V4L2_INTERFACE_ERR;

    queryctrl.id = control_id;
    if (-1 == xioctl (fd, VIDIOC_QUERY_EXT_CTRL, &queryctrl)) {
        if (errno != EINVAL) {
            perror ("VIDIOC_QUERY_EXT_CTRL");
        } else {
            printf ("ID not supported\n");
        }
        success = V4L2_INTERFACE_ERR;
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf ("ID disabled\n");
        success = V4L2_INTERFACE_ERR;
    } else {
        controls = init_ext_controls(&queryctrl);
        if (!controls) {
            return V4L2_INTERFACE_ERR;
        }
        if (-1 == xioctl (fd, VIDIOC_G_EXT_CTRLS, controls)) {
            perror ("VIDIOC_G_EXT_CTRLS");
            cleanExtControls(controls);
            if(errno == EBUSY)
                success = V4L2_INTERFACE_ERR_BUSY;
            else
                success = V4L2_INTERFACE_ERR;
        }else{
            *ret_controls = controls;
        }
    }

    closeDevice(fd);

    return success;
}

int setExtControl(char* device, struct v4l2_ext_controls *controls) {

    int fd = openDevice(device);
    if(fd < 0) return fd;
    if (-1 == xioctl (fd, VIDIOC_S_EXT_CTRLS, controls)) {
        return errno;
    }

    closeDevice(fd);

    return V4L2_INTERFACE_OK;
}

int getcropcap(char* device, struct v4l2_cropcap * cc) {

    int fd = openDevice(device);
    if(fd < 0) return fd;
    if (-1 == xioctl (fd, VIDIOC_CROPCAP, cc)) {
        return errno;
    }

    closeDevice(fd);

    return V4L2_INTERFACE_OK;
}
