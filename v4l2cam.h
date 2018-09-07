/*******************************************************************************
#	 	luvcview: Sdl video Usb Video Class grabber          .         #
#This package work with the Logitech UVC based webcams with the mjpeg feature. #
#All the decoding is in user space with the embedded jpeg decoder              #
#.                                                                             #
# 		Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard     #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/
#ifndef __XM_V4LCAM_H
#define __XM_V4LCAM_H

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

#define NB_BUFFER 4

#if defined(__cplusplus)
extern "C"
{
#endif   /* __cplusplus */

struct camDev {
    char *videodevice;
    int width;
    int height;

    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    float fps;
    int format;
};

int init_cam(struct camDev *cam, char *device, int width, int height);
int check_cam(struct camDev *cam, char *device);
int grab_cam(struct camDev *cam, char *save_file);
void close_cam(struct camDev *cam);

int enum_controls(int cam);
void save_controls(int cam);
void load_controls(int cam, char *confignamestr);
	     

int enum_frame_intervals(int dev, __u32 pixfmt, __u32 width, __u32 height);
int enum_frame_sizes(int dev, __u32 pixfmt);
int enum_frame_formats(int dev, unsigned int *supported_formats, unsigned int max_formats);

#if defined(__cplusplus)
}
#endif   /* __cplusplus */

#endif
