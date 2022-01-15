/*************************************************************************
	> File Name: camera.c
	> Author: 
	> Mail: 
	> Created Time: 2018年06月05日 星期二 17时34分57秒
 ************************************************************************/

#include<stdio.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<sys/ioctl.h>
#include<linux/videodev2.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<stdlib.h>
#include "camera.h"
#include <jpeglib.h>
#define REQBUFS_COUNT 4
#define ROUND_0_255(v)((v) < 0 ? 0 : ((v) > 255 ? 255 : (v)))
struct cam_buf{
    void *start;
    size_t length;
};

struct v4l2_requestbuffers reqbufs;
struct cam_buf bufs[REQBUFS_COUNT];

int camera_init(char *devpath, unsigned int *width, unsigned *height, unsigned int *size, unsigned int *ismjpeg)
{
    int i;
    int fd;
    int ret;
    struct v4l2_buffer vbuf;
    struct v4l2_format format;
    struct v4l2_capability capability;

    fd = open(devpath, O_RDWR);
    if(-1 == fd)
    {
        perror("camera->init");
        return -1;
    }
    ret = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if(-1 == ret)
    {
        perror("camera->init");
        return -1;
    }
    if(!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "cameara->init:device can not suporrt V4L2_CAP_VIDEO_CAPTURE");
        close(fd);
        return -1;
    }
    if(!(capability.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "camera->init: device can not suporrt v4l2_CAP_STREAMING");
        close(fd);
        return -1;
    }
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = *width;
    format.fmt.pix.height = *height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if(-1 == ret)
        perror("camera init");
    else
    {
        fprintf(stdout, "camera->init: picture format is mjpeg\n");
        *ismjpeg = 1;
        goto get_fmt;
    }
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.width = *width;
    format.fmt.pix.height = *height;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd, VIDIOC_S_FMT,&format);
    if(ret == -1)
    {
        perror("camera init");
        return -1;
    }
    else
    {
        *ismjpeg = 0;
        fprintf(stdout,"camera->init: picture format is yuyv\n");
    }

get_fmt:
    ret = ioctl(fd, VIDIOC_G_FMT,&format);
    if(ret == -1)
    {
        perror("camera init");
        return -1;
    }
    memset(&reqbufs, 0, sizeof(struct v4l2_requestbuffers));
    reqbufs.count    =REQBUFS_COUNT;
    reqbufs.type     =V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory   =V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
    if(ret == -1)
    {
        perror("camera init");
        close(fd);
        return -1;
    }

    for(i = 0; i < reqbufs.count; i++)
    {
        memset(&vbuf, 0, sizeof(struct v4l2_buffer));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &vbuf);
        if(-1 == ret)
        {
            perror("camera init");
            close(fd);
            return -1;
        }

        bufs[i].length = vbuf.length;
        bufs[i].start = mmap(NULL, vbuf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, vbuf.m.offset);
    
        if(bufs[i].start == MAP_FAILED)
        {
            perror("camera init");
            close(fd);
            return -1;
        }
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
        if (ret == -1) 
        {
            perror("camera init");
            close(fd);
            return -1;     
        }
    }
    *width = format.fmt.pix.width;
	*height = format.fmt.pix.height;
	*size = bufs[0].length;

	return fd;
}

int camera_start(int fd)
{
    int ret;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if(ret ==-1)
    {
        perror("camera -> start");
        return -1;
    }
    fprintf(stdout, "camera->start: start capture\n");
    return 0;
}

int camera_dqbuf(int fd,void **buf, unsigned int *size, unsigned int *index)
{
    int ret;
    fd_set fds;
    struct timeval timeout;
    struct v4l2_buffer vbuf;
    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(fd+1, &fds, NULL, NULL, &timeout);
        if(ret == -1)
        {
            perror("camera->dqbuf");
            if(errno == EINTR)
                continue;
            else
                return -1;
        }
        else if(ret == 0)
        {
            fprintf(stderr, "camera->:dqbuf:");
            continue;
        }
        else
        {
            vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vbuf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
            if (ret == -1) 
            {
                perror("camera->dqbuf");
                return -1;            
            }
            *buf = bufs[vbuf.index].start;
            *size = vbuf.bytesused;
            *index = vbuf.index;
            return 0;
        }
    }
}

int camera_eqbuf(int fd, unsigned int index)
{
    int ret;
    struct v4l2_buffer vbuf;

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = index;
    ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
    if (ret == -1) 
    {
        perror("camera->eqbuf");
        return -1;
    }

        return 0;
}

int camera_stop(int fd)
{
    int ret;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret == -1) 
    {
        perror("camera->stop");
        return -1;        
    }
    fprintf(stdout, "camera->stop: stop capture\n");
    return 0;
}

int camera_exit(int fd)
{
    int i;
    int ret;
    struct v4l2_buffer vbuf;

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;

    for (i = 0; i < reqbufs.count; i++) 
    {
        ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
        if (ret == -1)
        break;    
    }
    for (i = 0; i < reqbufs.count; i++)
        munmap(bufs[i].start, bufs[i].length);

    fprintf(stdout, "camera->exit: camera exit\n");

        return close(fd);

}



typedef struct {
        struct jpeg_destination_mgr pub;
        JOCTET *buffer;
        unsigned char *outbuffer;
        int outbuffer_size;
        unsigned char *outbuffer_cursor;
        int *written;

} jpeg_dest_mgr, *jpeg_dest_mgr_ptr;

struct jpeg_mgr_info {
        unsigned long written;
        JSAMPROW row_pointer[1];
        struct jpeg_error_mgr jerr;
        struct jpeg_compress_struct cinfo;

};

static struct jpeg_mgr_info jinfo;

static short radj[] = {
        -175, -174, -172, -171, -169, -168, -167, -165, 
        -164, -163, -161, -160, -159, -157, -156, -154, 
        -153, -152, -150, -149, -148, -146, -145, -143, 
        -142, -141, -139, -138, -137, -135, -134, -132, 
        -131, -130, -128, -127, -126, -124, -123, -121, 
        -120, -119, -117, -116, -115, -113, -112, -111, 
        -109, -108, -106, -105, -104, -102, -101, -100, 
        -98,  -97,  -95,  -94,  -93,  -91,  -90,  -89, 
        -87,  -86,  -84,  -83,  -82,  -80,  -79,  -78, 
        -76,  -75,  -74,  -72,  -71,  -69,  -68,  -67, 
        -65,  -64,  -63,  -61,  -60,  -58,  -57,  -56, 
        -54,  -53,  -52,  -50,  -49,  -47,  -46,  -45, 
        -43,  -42,  -41,  -39,  -38,  -37,  -35,  -34, 
        -32,  -31,  -30,  -28,  -27,  -26,  -24,  -23, 
        -21,  -20,  -19,  -17,  -16,  -15,  -13,  -12, 
        -10,   -9,   -8,   -6,   -5,   -4,   -2,   -1, 
        0,    1,    2,    4,    5,    6,    8,    9, 
        10,   12,   13,   15,   16,   17,   19,   20, 
        21,   23,   24,   26,   27,   28,   30,   31, 
        32,   34,   35,   37,   38,   39,   41,   42, 
        43,   45,   46,   47,   49,   50,   52,   53, 
        54,   56,   57,   58,   60,   61,   63,   64, 
        65,   67,   68,   69,   71,   72,   74,   75, 
        76,   78,   79,   80,   82,   83,   84,   86, 
        87,   89,   90,   91,   93,   94,   95,   97, 
        98,  100,  101,  102,  104,  105,  106,  108, 
        109,  111,  112,  113,  115,  116,  117,  119, 
        120,  121,  123,  124,  126,  127,  128,  130, 
        131,  132,  134,  135,  137,  138,  139,  141, 
        142,  143,  145,  146,  148,  149,  150,  152, 
        153,  154,  156,  157,  159,  160,  161,  163, 
        164,  165,  167,  168,  169,  171,  172,  174,

};

static short gadj1[] = {
        -89,  -88,  -87,  -87,  -86,  -85,  -85,  -84, 
        -83,  -83,  -82,  -81,  -80,  -80,  -79,  -78, 
        -78,  -77,  -76,  -76,  -75,  -74,  -73,  -73, 
        -72,  -71,  -71,  -70,  -69,  -69,  -68,  -67, 
        -67,  -66,  -65,  -64,  -64,  -63,  -62,  -62, 
        -61,  -60,  -60,  -59,  -58,  -57,  -57,  -56, 
        -55,  -55,  -54,  -53,  -53,  -52,  -51,  -50, 
        -50,  -49,  -48,  -48,  -47,  -46,  -46,  -45, 
        -44,  -43,  -43,  -42,  -41,  -41,  -40,  -39, 
        -39,  -38,  -37,  -36,  -36,  -35,  -34,  -34, 
        -33,  -32,  -32,  -31,  -30,  -30,  -29,  -28, 
        -27,  -27,  -26,  -25,  -25,  -24,  -23,  -23, 
        -22,  -21,  -20,  -20,  -19,  -18,  -18,  -17, 
        -16,  -16,  -15,  -14,  -13,  -13,  -12,  -11, 
        -11,  -10,   -9,   -9,   -8,   -7,   -6,   -6, 
        -5,   -4,   -4,   -3,   -2,   -2,   -1,    0, 
        0,    0,    1,    2,    2,    3,    4,    4, 
        5,    6,    6,    7,    8,    9,    9,   10, 
        11,   11,   12,   13,   13,   14,   15,   16, 
        16,   17,   18,   18,   19,   20,   20,   21, 
        22,   23,   23,   24,   25,   25,   26,   27, 
        27,   28,   29,   30,   30,   31,   32,   32, 
        33,   34,   34,   35,   36,   36,   37,   38, 
        39,   39,   40,   41,   41,   42,   43,   43, 
        44,   45,   46,   46,   47,   48,   48,   49, 
        50,   50,   51,   52,   53,   53,   54,   55, 
        55,   56,   57,   57,   58,   59,   60,   60, 
        61,   62,   62,   63,   64,   64,   65,   66, 
        67,   67,   68,   69,   69,   70,   71,   71, 
        72,   73,   73,   74,   75,   76,   76,   77, 
        78,   78,   79,   80,   80,   81,   82,   83, 
        83,   84,   85,   85,   86,   87,   87,   88, 

};

static short gadj2[] = {
        -43,  -42,  -42,  -42,  -41,  -41,  -41,  -40, 
        -40,  -40,  -39,  -39,  -39,  -38,  -38,  -38, 
        -37,  -37,  -37,  -36,  -36,  -36,  -35,  -35, 
        -35,  -34,  -34,  -34,  -33,  -33,  -33,  -32, 
        -32,  -32,  -31,  -31,  -31,  -30,  -30,  -30, 
        -29,  -29,  -29,  -28,  -28,  -28,  -27,  -27, 
        -27,  -26,  -26,  -25,  -25,  -25,  -24,  -24, 
        -24,  -23,  -23,  -23,  -22,  -22,  -22,  -21, 
        -21,  -21,  -20,  -20,  -20,  -19,  -19,  -19, 
        -18,  -18,  -18,  -17,  -17,  -17,  -16,  -16, 
        -16,  -15,  -15,  -15,  -14,  -14,  -14,  -13, 
        -13,  -13,  -12,  -12,  -12,  -11,  -11,  -11, 
        -10,  -10,  -10,   -9,   -9,   -9,   -8,   -8, 
        -8,   -7,   -7,   -7,   -6,   -6,   -6,   -5, 
        -5,   -5,   -4,   -4,   -4,   -3,   -3,   -3, 
        -2,   -2,   -2,   -1,   -1,   -1,    0,    0, 
        0,    0,    0,    1,    1,    1,    2,    2, 
        2,    3,    3,    3,    4,    4,    4,    5, 
        5,    5,    6,    6,    6,    7,    7,    7, 
        8,    8,    8,    9,    9,    9,   10,   10, 
        10,   11,   11,   11,   12,   12,   12,   13, 
        13,   13,   14,   14,   14,   15,   15,   15, 
        16,   16,   16,   17,   17,   17,   18,   18, 
        18,   19,   19,   19,   20,   20,   20,   21, 
        21,   21,   22,   22,   22,   23,   23,   23, 
        24,   24,   24,   25,   25,   25,   26,   26, 
        27,   27,   27,   28,   28,   28,   29,   29, 
        29,   30,   30,   30,   31,   31,   31,   32, 
        32,   32,   33,   33,   33,   34,   34,   34, 
        35,   35,   35,   36,   36,   36,   37,   37, 
        37,   38,   38,   38,   39,   39,   39,   40, 
        40,   40,   41,   41,   41,   42,   42,   42,

};

static short badj[] = {
        -221, -220, -218, -216, -214, -213, -211, -209, 
        -207, -206, -204, -202, -200, -199, -197, -195, 
        -194, -192, -190, -188, -187, -185, -183, -181, 
        -180, -178, -176, -174, -173, -171, -169, -168, 
        -166, -164, -162, -161, -159, -157, -155, -154, 
        -152, -150, -148, -147, -145, -143, -142, -140, 
        -138, -136, -135, -133, -131, -129, -128, -126, 
        -124, -123, -121, -119, -117, -116, -114, -112, 
        -110, -109, -107, -105, -103, -102, -100,  -98, 
        -97,  -95,  -93,  -91,  -90,  -88,  -86,  -84, 
        -83,  -81,  -79,  -77,  -76,  -74,  -72,  -71, 
        -69,  -67,  -65,  -64,  -62,  -60,  -58,  -57, 
        -55,  -53,  -51,  -50,  -48,  -46,  -45,  -43, 
        -41,  -39,  -38,  -36,  -34,  -32,  -31,  -29, 
        -27,  -25,  -24,  -22,  -20,  -19,  -17,  -15, 
        -13,  -12,  -10,   -8,   -6,   -5,   -3,   -1, 
        0,    1,    3,    5,    6,    8,   10,   12, 
        13,   15,   17,   19,   20,   22,   24,   25, 
        27,   29,   31,   32,   34,   36,   38,   39, 
        41,   43,   45,   46,   48,   50,   51,   53, 
        55,   57,   58,   60,   62,   64,   65,   67, 
        69,   71,   72,   74,   76,   77,   79,   81, 
        83,   84,   86,   88,   90,   91,   93,   95, 
        97,   98,  100,  102,  103,  105,  107,  109, 
        110,  112,  114,  116,  117,  119,  121,  123, 
        124,  126,  128,  129,  131,  133,  135,  136, 
        138,  140,  142,  143,  145,  147,  148,  150, 
        152,  154,  155,  157,  159,  161,  162,  164, 
        166,  168,  169,  171,  173,  174,  176,  178, 
        180,  181,  183,  185,  187,  188,  190,  192, 
        194,  195,  197,  199,  200,  202,  204,  206, 
        207,  209,  211,  213,  214,  216,  218,  220,

};

void convert_yuv_to_rgb(void *yuv, void *rgb, unsigned int width, unsigned int height, unsigned int bps)
{
        unsigned int i;
        int y1, y2, u, v;
        unsigned char *src = yuv;
        unsigned char *dst = rgb;
        unsigned int count = width * height / 2;

    switch (bps) {
            case 24:
        for (i = 0; i < count; i++) {
                        y1 = *src++;
                        u  = *src++;
                        y2 = *src++;
                        v  = *src++;

                        *dst++ = ROUND_0_255(y1 + radj[v]);
                        *dst++ = ROUND_0_255(y1 - gadj1[u] - gadj2[v]);
                        *dst++ = ROUND_0_255(y1 + badj[u]);

                        *dst++ = ROUND_0_255(y2 + radj[v]);
                        *dst++ = ROUND_0_255(y2 - gadj1[u] - gadj2[v]);
                        *dst++ = ROUND_0_255(y2 + badj[u]);
                    
        }
                break;
            
    }

}

void convert_rgb_to_jpg_init(void)
{
        memset(&jinfo, 0, sizeof(struct jpeg_mgr_info));
        jinfo.cinfo.err = jpeg_std_error(&jinfo.jerr);
        jpeg_create_compress(&jinfo.cinfo);

}

int convert_rgb_to_jpg_work(void *rgb, void *jpeg, unsigned int width, unsigned int height, unsigned int bpp, int quality)
{
        jinfo.written = width * height * bpp / 3;
        jpeg_mem_dest(&jinfo.cinfo, (unsigned char **)&jpeg, &jinfo.written);
        jinfo.cinfo.image_width = width;
        jinfo.cinfo.image_height = height;
        jinfo.cinfo.input_components = bpp / 8;
        jinfo.cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&jinfo.cinfo);
        jpeg_set_quality(&jinfo.cinfo, quality, TRUE);

        jpeg_start_compress(&jinfo.cinfo, TRUE);

    while(jinfo.cinfo.next_scanline < height)
    {
        
        jinfo.row_pointer[0] = rgb + jinfo.cinfo.next_scanline * width * bpp / 8;
        jpeg_write_scanlines(&jinfo.cinfo, jinfo.row_pointer, 1);
            
    }

        jpeg_finish_compress(&jinfo.cinfo);

        return (jinfo.written);

}

void convert_rgb_to_jpg_exit(void)
{
        jpeg_destroy_compress(&jinfo.cinfo);

}

