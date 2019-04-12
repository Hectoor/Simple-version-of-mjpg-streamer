/*
 *  name:v4l2.h
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#ifndef __V4L2_H
#define __V4L2_H

#include "streamer.h"

int v4l2_init(void);
int v4l2Grab(void);
int v4l2_close(void);
int v4l2_run(void);
#define CAM_DEV "/dev/video0"
#define   WIDTH   640                       // 图片的宽度
#define   HEIGHT  480                       // 图片的高度

int cam_fd;
#endif