/*******************************************************************************
#	 	luvcview: Sdl video Usb Video Class grabber           .        #
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include "v4l2cam.h"

#include "Perf.h"

struct camDev *videoIn;


int main(int argc, char *argv[])
{
	const char *videodevice = NULL;
	int width = 1280;
	int height = 720;
	int count = 0;

	videodevice = "/dev/video0";
	videoIn = (struct camDev *) calloc(1, sizeof(struct camDev));
	//check_cam(videoIn, (char *) videodevice);

	int once=0;
	perf first("FirstFrameCost");
	perf f("open>>>");

	if (init_cam(videoIn, (char *) videodevice, width, height) < 0)
		exit(1);

	f.done();

	//load_controls(videoIn->fd, "luvcview.cfg");  //*******load_controls
	while(count < 100){
		/* main big loop */
		char filename[256];
		sprintf(filename, "%d.jpg", count);
		perf s("grab");
//		printf("grabbing %d\n", count);
//		if (grab_cam(videoIn, filename) < 0) {
		if (grab_cam(videoIn, 0) < 0) {
			printf("Error grabbing\n");
		}
if (!once){
first.done();
once=1;
}
//		printf("grabbing %d done\n", count);
		count ++;
	}

	close_cam(videoIn);
	free(videoIn);
	printf("Cleanup done. Exiting ...\n");
}

