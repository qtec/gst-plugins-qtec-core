

#ifndef __EXT_CONTROL_HEADERS_H__
#define __EXT_CONTROL_HEADERS_H__

/*Adding extended controls in case of old version of v4l2*/

#ifndef V4L2_CTRL_FLAG_HAS_PAYLOAD
#define V4L2_CTRL_FLAG_HAS_PAYLOAD 0x0100
#endif

#ifndef VIDIOC_QUERY_EXT_CTRL
#define VIDIOC_QUERY_EXT_CTRL   _IOWR('V', 103, struct v4l2_query_ext_ctrl)
#define V4L2_CTRL_MAX_DIMS    (4)

/*
struct v4l2_ext_control {
    __u32 id;
    __u32 size;
    __u32 reserved2[1];
    union {
        __s32 value;
        __s64 value64;
        char *string;
        __u8 *p_u8;
        __u16 *p_u16;
        __u32 *p_u32;
        void *ptr;
    };
} __attribute__ ((packed));

struct v4l2_ext_controls {
    __u32 ctrl_class;
    __u32 count;
    __u32 error_idx;
    __u32 reserved[2];
    struct v4l2_ext_control *controls;
};*/

struct v4l2_query_ext_ctrl {
    __u32            id;
    __u32            type;
    char             name[32];
    __s64            minimum;
    __s64            maximum;
    __u64            step;
    __s64            default_value;
    __u32                flags;
    __u32                elem_size;
    __u32                elems;
    __u32                nr_of_dims;
    __u32                dims[V4L2_CTRL_MAX_DIMS];
    __u32            reserved[32];
};

#endif

#ifndef VIDIOC_G_EXT_CTRLS
#define VIDIOC_G_EXT_CTRLS  _IOWR('V', 71, struct v4l2_ext_controls)
#endif

#ifndef VIDIOC_S_EXT_CTRLS
#define VIDIOC_S_EXT_CTRLS  _IOWR('V', 72, struct v4l2_ext_controls)
#endif

#ifndef VIDIOC_TRY_EXT_CTRLS
#define VIDIOC_TRY_EXT_CTRLS    _IOWR('V', 73, struct v4l2_ext_controls)
#endif

#endif /* __EXT_CONTROL_HEADERS_H__ */