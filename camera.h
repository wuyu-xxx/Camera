/*************************************************************************
	> File Name: camera.h
	> Author: 
	> Mail: 
	> Created Time: 2018年06月10日 星期日 19时55分23秒
 ************************************************************************/

#ifndef _CAMERA_H
#define _CAMERA_H

int camera_init(char *devpath, unsigned int *width, unsigned int *height, unsigned int *size, unsigned int *ismjpeg);
int camera_start(int fd);
int camera_dqbuf(int fd, void **buf, unsigned int *size, unsigned int *index);
int camera_eqbuf(int fd, unsigned int index);
int camera_stop(int fd);
int camera_exit(int fd);
void convert_rgb_to_jpg_init(void);
void convert_yuv_to_rgb(void *yuv, void *rgb, unsigned int width, unsigned int height, unsigned int bps);
int convert_rgb_to_jpg_work(void *rgb, void *jpeg, unsigned int width, unsigned int height, unsigned int bpp, int quality);
void convert_rgb_to_jpg_exit(void);

#endif
