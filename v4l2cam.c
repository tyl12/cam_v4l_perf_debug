/*******************************************************************************
#	 	uvcview: Sdl video Usb Video Class grabber           .         #
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

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "v4l2cam.h"

#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define FOURCC_FORMAT		"%c%c%c%c"
#define FOURCC_ARGS(c)		(c) & 0xFF, ((c) >> 8) & 0xFF, ((c) >> 16) & 0xFF, ((c) >> 24) & 0xFF


static int debug = 0;

static int init_v4l2(struct camDev *cam);

static int float_to_fraction_recursive(double f, double p, int *num, int *den)
{
	int whole = (int)f;
	f = fabs(f - whole);

	if(f > p) {
		int n, d;
		int a = float_to_fraction_recursive(1 / f, p + p / f, &n, &d);
		*num = d;
		*den = d * a + n;
	}
	else {
		*num = 0;
		*den = 1;
	}
	return whole;
}

static void float_to_fraction(float f, int *num, int *den)
{
	int whole = float_to_fraction_recursive(f, FLT_EPSILON, num, den);
	*num += whole * *den;
}

int check_cam(struct camDev *cam, char *device)
{
	int ret;
	if (cam == NULL || device == NULL)
		return -1;
	cam->videodevice = (char *) calloc(1, 16 * sizeof(char));
	sprintf(cam->videodevice, "%s", device);
	printf("V4L2Dev: Device path:  %s\n", cam->videodevice);
	if ((cam->fd = open(cam->videodevice, O_RDWR)) == -1) {
		perror("V4L2Dev: ERROR opening V4L interface");
		exit(1);
	}
	memset(&cam->cap, 0, sizeof(struct v4l2_capability));
	ret = ioctl(cam->fd, VIDIOC_QUERYCAP, &cam->cap);
	if (ret < 0) {
		printf("V4L2Dev: Error opening device %s: unable to query device.\n",
				cam->videodevice);
		goto _fatal;
	}
	if ((cam->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
		printf("V4L2Dev: Error opening device %s: video capture not supported.\n",
				cam->videodevice);
	}
	if (!(cam->cap.capabilities & V4L2_CAP_STREAMING)) {
		printf("V4L2Dev: %s does not support streaming i/o\n", cam->videodevice);
	}
	if (!(cam->cap.capabilities & V4L2_CAP_READWRITE)) {
		printf("V4L2Dev: %s does not support read i/o\n", cam->videodevice);
	}
	enum_frame_formats(cam->fd, NULL, 0);
_fatal:    
	close(cam->fd);
	free(cam->videodevice);
	return 0;
}

int init_cam(struct camDev  *cam, char *device, int width, int height)
{
	if (cam == NULL || device == NULL)
		return -1;
	if (width == 0 || height == 0)
		return -1;
	cam->videodevice = NULL;
	cam->videodevice = (char *) calloc(1, 16 * sizeof(char));
	sprintf(cam->videodevice, "%s", device);
	printf("V4L2Dev:   Device path:  %s\n", cam->videodevice);
	cam->width = width;
	cam->height = height;
	cam->fps = 30;
	cam->format = V4L2_PIX_FMT_MJPEG;
	if (init_v4l2(cam) < 0) {
		printf("V4L2Dev:  Init v4L2 failed !! exit fatal, %s\n", cam->videodevice);
		goto error;;
	}
	return 0;

error:
	free(cam->videodevice);
	close(cam->fd);
	return -1;
}

void save_controls(int cam)
{ 
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control   control_s;
	FILE *configfile;
	memset (&queryctrl, 0, sizeof (queryctrl));
	memset (&control_s, 0, sizeof (control_s));
	configfile = fopen("luvcview.cfg", "w");
	if ( configfile == NULL) {
		perror("V4L2Dev: saving configfile luvcview.cfg failed");
	}
	else {
		fprintf(configfile, "id         value      # luvcview control settings configuration file\n");
		for (queryctrl.id = V4L2_CID_BASE;
				queryctrl.id < V4L2_CID_LASTP1;
				queryctrl.id++) {
			if (0 == ioctl (cam, VIDIOC_QUERYCTRL, &queryctrl)) {
				if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
					continue;
				control_s.id=queryctrl.id;
				ioctl(cam, VIDIOC_G_CTRL, &control_s);
				fprintf (configfile, "%-10d %-10d # name:%-32s type:%d min:%-5d max:%-5d step:%-5d def:%d\n",
						queryctrl.id, control_s.value, queryctrl.name, queryctrl.type, queryctrl.minimum,
						queryctrl.maximum, queryctrl.step, queryctrl.default_value);
				printf ("%-10d %-10d # name:%-32s type:%d min:%-5d max:%-5d step:%-5d def:%d\n",
						queryctrl.id, control_s.value, queryctrl.name, queryctrl.type, queryctrl.minimum,
						queryctrl.maximum, queryctrl.step, queryctrl.default_value);
			}
		}
		for (queryctrl.id = V4L2_CID_PRIVATE_BASE;;
				queryctrl.id++) {
			if (0 == ioctl (cam, VIDIOC_QUERYCTRL, &queryctrl)) {
				if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
					continue;
				if ((queryctrl.id==134217735) || (queryctrl.id==134217736))
					continue;
				control_s.id=queryctrl.id;
				ioctl(cam, VIDIOC_G_CTRL, &control_s);
				fprintf (configfile, "%-10d %-10d # name:%-32s type:%d min:%-5d max:%-5d step:%-5d def:%d\n",
						queryctrl.id, control_s.value, queryctrl.name, queryctrl.type, queryctrl.minimum,
						queryctrl.maximum, queryctrl.step, queryctrl.default_value);
				printf ("%-10d %-10d # name:%-32s type:%d min:%-5d max:%-5d step:%-5d def:%d\n",
						queryctrl.id, control_s.value, queryctrl.name, queryctrl.type, queryctrl.minimum,
						queryctrl.maximum, queryctrl.step, queryctrl.default_value);
			} else {
				if (errno == EINVAL)
					break;
			}
		}
		fflush(configfile);
		fclose(configfile);
	}
}

void load_controls(int cam, char *configfilestr)
{
	struct v4l2_control   control;
	FILE *configfile;
	memset (&control, 0, sizeof (control));
	//  configfile = fopen("luvcview.cfg", "r");
	configfile = fopen(configfilestr, "r");
	if ( configfile == NULL) {
		perror("V4L2Dev: configfile luvcview.cfg open failed");
	}
	else {
		printf("V4L2Dev: loading controls from luvcview.cfg\n");
		char buffer[512]; 
		fgets(buffer, sizeof(buffer), configfile);
		while (NULL !=fgets(buffer, sizeof(buffer), configfile) )
		{
			sscanf(buffer, "%i%i", &control.id, &control.value);
			if (ioctl(cam, VIDIOC_S_CTRL, &control))
				printf("V4L2Dev: ERROR id:%d val:%d\n", control.id, control.value);
			else
				printf("V4L2Dev: OK    id:%d val:%d\n", control.id, control.value);
		}   
		fclose(configfile);
	}
}

static int video_enable(struct camDev *cam)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = ioctl(cam->fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		perror("V4L2Dev: Unable to start capture");
		return ret;
	}
	return 0;
}

static int video_disable(struct camDev *cam)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = ioctl(cam->fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		perror("V4L2Dev: Unable to stop capture");
		return ret;
	}
	return 0;
}


static int init_v4l2(struct camDev *cam)
{
	int i;
	int ret = 0;
	struct v4l2_streamparm* setfps;  
	int n = 0, d = 0;
	unsigned int device_formats[16] = { 0 };	// Assume no device supports more than 16 formats
	int requested_format_found = 0, fallback_format = -1;

	printf("V4L2Dev: Init V4L2 %s\n", cam->videodevice);

	if ((cam->fd = open(cam->videodevice, O_RDWR)) == -1) {
		perror("V4L2Dev: ERROR opening V4L interface");
		exit(1);
	}
	memset(&cam->cap, 0, sizeof(struct v4l2_capability));
	ret = ioctl(cam->fd, VIDIOC_QUERYCAP, &cam->cap);
	if (ret < 0) {
		printf("V4L2Dev: Error opening device %s: unable to query device.\n",
				cam->videodevice);
		goto _fatal;
	}

	if ((cam->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
		printf("V4L2Dev: Error opening device %s: video capture not supported.\n",
				cam->videodevice);
		goto _fatal;
	}

	if (!(cam->cap.capabilities & V4L2_CAP_STREAMING)) {
		printf("V4L2Dev: %s does not support streaming i/o\n", cam->videodevice);
		goto _fatal;
	}

	// Enumerate the supported formats to check whether the requested one
	// is available. If not, we try to fall back to YUYV.
	if(enum_frame_formats(cam->fd, device_formats, ARRAY_SIZE(device_formats))) {
		printf("V4L2Dev: Unable to enumerate frame formats");
		goto _fatal;
	}
	for(i = 0; i < ARRAY_SIZE(device_formats) && device_formats[i]; i++) {
		if(device_formats[i] == cam->format) {
			requested_format_found = 1;
			break;
		}
		if(device_formats[i] == V4L2_PIX_FMT_MJPEG || device_formats[i] == V4L2_PIX_FMT_YUYV)
			fallback_format = i;
	}
	if(requested_format_found) {
		// The requested format is supported
		printf("V4L2Dev: Frame format: "FOURCC_FORMAT"\n", FOURCC_ARGS(cam->format));
	}
	else if(fallback_format >= 0) {
		// The requested format is not supported but there's a fallback format
		printf("V4L2Dev: Frame format: "FOURCC_FORMAT" ("FOURCC_FORMAT
				" is not supported by device)\n",
				FOURCC_ARGS(device_formats[0]), FOURCC_ARGS(cam->format));
		cam->format = device_formats[0];
	}
	else {
		// The requested format is not supported and no fallback format is available
		printf("V4L2Dev: ERROR: Requested frame format "FOURCC_FORMAT" is not available "
				"and no fallback format was found.\n", FOURCC_ARGS(cam->format));
		goto _fatal;
	}

	// Set pixel format and frame size
	memset(&cam->fmt, 0, sizeof(struct v4l2_format));
	cam->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->fmt.fmt.pix.width = cam->width;
	cam->fmt.fmt.pix.height = cam->height;
	cam->fmt.fmt.pix.pixelformat = cam->format;
	cam->fmt.fmt.pix.field = V4L2_FIELD_ANY;
	ret = ioctl(cam->fd, VIDIOC_S_FMT, &cam->fmt);
	if (ret < 0) {
		perror("V4L2Dev: Unable to set format");
		goto _fatal;
	}
	if ((cam->fmt.fmt.pix.width != cam->width) ||
			(cam->fmt.fmt.pix.height != cam->height)) {
		printf("V4L2Dev: Frame size:   %ux%u (requested size %ux%u is not supported by device)\n",
				cam->fmt.fmt.pix.width, cam->fmt.fmt.pix.height, cam->width, cam->height);
		cam->width = cam->fmt.fmt.pix.width;
		cam->height = cam->fmt.fmt.pix.height;
		/* look the format is not part of the deal ??? */
		//cam->format = cam->fmt.fmt.pix.pixelformat;
	}
	else {
		printf("V4L2Dev: Frame size:   %dx%d\n", cam->width, cam->height);
	}
#if 1
	/* set framerate */
	setfps=(struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
	memset(setfps, 0, sizeof(struct v4l2_streamparm));
	setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	// Convert the frame rate into a fraction for V4L2
	float_to_fraction(cam->fps, &n, &d);
	setfps->parm.capture.timeperframe.numerator = d;
	setfps->parm.capture.timeperframe.denominator = n;

	ret = ioctl(cam->fd, VIDIOC_S_PARM, setfps);
	if(ret == -1) {
		perror("V4L2Dev: Unable to set frame rate");
		goto _fatal;
	}
	ret = ioctl(cam->fd, VIDIOC_G_PARM, setfps); 
	if(ret == 0) {
		float confirmed_fps = (float)setfps->parm.capture.timeperframe.denominator / (float)setfps->parm.capture.timeperframe.numerator;
		if (confirmed_fps != (float)n / (float)d) {
			printf("V4L2Dev:   Frame rate:   %g fps (requested frame rate %g fps is "
					"not supported by device)\n",
					confirmed_fps,
					cam->fps);
			cam->fps = confirmed_fps;
		}
		else {
			printf("V4L2Dev:   Frame rate:   %g fps\n", cam->fps);
		}
	}
	else {
		perror("V4L2Dev: Unable to read out current frame rate");
		goto _fatal;
	}
#endif
	/* request buffers */
	memset(&cam->rb, 0, sizeof(struct v4l2_requestbuffers));
	cam->rb.count = NB_BUFFER;
	cam->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(cam->fd, VIDIOC_REQBUFS, &cam->rb);
	if (ret < 0) {
		perror("V4L2Dev: Unable to allocate buffers");
		goto _fatal;
	}
	/* map the buffers */
	for (i = 0; i < NB_BUFFER; i++) {
		memset(&cam->buf, 0, sizeof(struct v4l2_buffer));
		cam->buf.index = i;
		cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(cam->fd, VIDIOC_QUERYBUF, &cam->buf);
		if (ret < 0) {
			perror("V4L2Dev: Unable to query buffer");
			goto _fatal;
		}
		if (debug)
			printf("V4L2Dev: length: %u offset: %u\n", cam->buf.length,
					cam->buf.m.offset);
		cam->mem[i] = mmap(0 /* start anywhere */ ,
				cam->buf.length, PROT_READ, MAP_SHARED, cam->fd,
				cam->buf.m.offset);
		if (cam->mem[i] == MAP_FAILED) {
			perror("V4L2Dev: Unable to map buffer");
			goto _fatal;
		}
		if (debug)
			printf("V4L2Dev: Buffer mapped at address %p.\n", cam->mem[i]);
	}
	/* Queue the buffers. */
	for (i = 0; i < NB_BUFFER; ++i) {
		memset(&cam->buf, 0, sizeof(struct v4l2_buffer));
		cam->buf.index = i;
		cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(cam->fd, VIDIOC_QBUF, &cam->buf);
		if (ret < 0) {
			perror("V4L2Dev: Unable to queue buffer");
			goto _fatal;;
		}
	}
	if (video_enable(cam))
		goto _fatal;

	return 0;

_fatal:
	return -1;

}

int grab_cam(struct camDev *cam, char *save_file)
{
	int ret;
	//	if (video_enable(cam))
	//	    goto err;
	memset(&cam->buf, 0, sizeof(struct v4l2_buffer));
	cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->buf.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(cam->fd, VIDIOC_DQBUF, &cam->buf);
	if (ret < 0) {
		perror("V4L2Dev: Unable to dequeue buffer");
		goto err;
	}

	/* Capture a single raw frame */
	if (save_file != NULL && cam->buf.bytesused > 0) {
		FILE *frame = NULL;
		char filename[15];
		int ret;

		frame = fopen(save_file, "wb");
		if(frame == NULL) {
			perror("V4L2Dev: Unable to open file for raw frame capturing");
			goto end_capture;
		}

		ret = fwrite(cam->mem[cam->buf.index], cam->buf.bytesused, 1, frame);
		if(ret < 1) {
			perror("V4L2Dev: Unable to write to file");
			goto end_capture;
		}
		printf("V4L2Dev: Saved raw frame to %s (%u bytes)\n", save_file, cam->buf.bytesused);

end_capture:
		if(frame)
			fclose(frame);
	}
	else if (save_file == NULL) {
		printf("V4L2Dev: skip the frame\n");
	}
	else {
		printf("V4L2Dev: get frame data failured, data size <= 0\n");
	}

	ret = ioctl(cam->fd, VIDIOC_QBUF, &cam->buf);
	if (ret < 0) {
		perror("V4L2Dev: Unable to requeue buffer");
		goto err;
	}

	return 0;

err:
	return -1;
}

void close_cam(struct camDev *cam)
{
	free(cam->videodevice);
	cam->videodevice = NULL;
	for (int i = 0; i < NB_BUFFER; i++) {
		munmap(cam->mem[i], cam->buf.length);
	}
	if(cam->fd > 0)
		close(cam->fd);
}

/* return >= 0 ok otherwhise -1 */
static int isv4l2Control(struct camDev *cam, int control,
		struct v4l2_queryctrl *queryctrl)
{
	int err =0;
	queryctrl->id = control;
	if ((err= ioctl(cam->fd, VIDIOC_QUERYCTRL, queryctrl)) < 0) {
		perror("V4L2Dev: ioctl querycontrol error");
	} else if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("V4L2Dev: control %s disabled\n", (char *) queryctrl->name);
	} else if (queryctrl->flags & V4L2_CTRL_TYPE_BOOLEAN) {
		return 1;
	} else if (queryctrl->type & V4L2_CTRL_TYPE_INTEGER) {
		return 0;
	} else {
		printf("V4L2Dev: contol %s unsupported\n", (char *) queryctrl->name);
	}
	return -1;
}

int v4l2GetControl(struct camDev *cam, int control)
{
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control_s;
	int err;
	if (isv4l2Control(cam, control, &queryctrl) < 0)
		return -1;
	control_s.id = control;
	if ((err = ioctl(cam->fd, VIDIOC_G_CTRL, &control_s)) < 0) {
		printf("V4L2Dev: ioctl get control error\n");
		return -1;
	}
	return control_s.value;
}

int v4l2SetControl(struct camDev *cam, int control, int value)
{
	struct v4l2_control control_s;
	struct v4l2_queryctrl queryctrl;
	int min, max, step, val_def;
	int err;
	if (isv4l2Control(cam, control, &queryctrl) < 0)
		return -1;
	min = queryctrl.minimum;
	max = queryctrl.maximum;
	step = queryctrl.step;
	val_def = queryctrl.default_value;
	if ((value >= min) && (value <= max)) {
		control_s.id = control;
		control_s.value = value;
		if ((err = ioctl(cam->fd, VIDIOC_S_CTRL, &control_s)) < 0) {
			printf("V4L2Dev: ioctl set control error\n");
			return -1;
		}
	}
	return 0;
}

int enum_frame_intervals(int dev, __u32 pixfmt, __u32 width, __u32 height)
{
	int ret;
	struct v4l2_frmivalenum fival;

	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = pixfmt;
	fival.width = width;
	fival.height = height;
	printf("V4L2Dev: \tTime interval between frame: ");
	while ((ret = ioctl(dev, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
		if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
			printf("%u/%u, ",
					fival.discrete.numerator, fival.discrete.denominator);
		} else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
			printf("{min { %u/%u } .. max { %u/%u } }, ",
					fival.stepwise.min.numerator, fival.stepwise.min.numerator,
					fival.stepwise.max.denominator, fival.stepwise.max.denominator);
			break;
		} else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
			printf("{min { %u/%u } .. max { %u/%u } / "
					"stepsize { %u/%u } }, ",
					fival.stepwise.min.numerator, fival.stepwise.min.denominator,
					fival.stepwise.max.numerator, fival.stepwise.max.denominator,
					fival.stepwise.step.numerator, fival.stepwise.step.denominator);
			break;
		}
		fival.index++;
	}
	printf("\n");
	if (ret != 0 && errno != EINVAL) {
		perror("V4L2Dev: ERROR enumerating frame intervals");
		return errno;
	}

	return 0;
}

int enum_frame_sizes(int dev, __u32 pixfmt)
{
	int ret;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = pixfmt;
	while ((ret = ioctl(dev, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
		if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			printf("V4L2Dev: { discrete: width = %u, height = %u }\n",
					fsize.discrete.width, fsize.discrete.height);
			ret = enum_frame_intervals(dev, pixfmt,
					fsize.discrete.width, fsize.discrete.height);
			if (ret != 0)
				printf("V4L2Dev:   Unable to enumerate frame sizes.\n");
		} else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
			printf("V4L2Dev: { continuous: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height);
			printf("V4L2Dev:   Refusing to enumerate frame intervals.\n");
			break;
		} else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
			printf("V4L2Dev: { stepwise: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } / "
					"stepsize { width = %u, height = %u } }\n",
					fsize.stepwise.min_width, fsize.stepwise.min_height,
					fsize.stepwise.max_width, fsize.stepwise.max_height,
					fsize.stepwise.step_width, fsize.stepwise.step_height);
			printf("V4L2Dev:   Refusing to enumerate frame intervals.\n");
			break;
		}
		fsize.index++;
	}
	if (ret != 0 && errno != EINVAL) {
		perror("V4L2Dev: ERROR enumerating frame sizes");
		return errno;
	}

	return 0;
}

int enum_frame_formats(int dev, unsigned int *supported_formats, unsigned int max_formats)
{
	int ret;
	struct v4l2_fmtdesc fmt;

	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = ioctl(dev, VIDIOC_ENUM_FMT, &fmt)) == 0) {
		if(supported_formats == NULL) {
			printf("V4L2Dev: { pixelformat = '%c%c%c%c', description = '%s' }\n",
					fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
					(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
					fmt.description);
			ret = enum_frame_sizes(dev, fmt.pixelformat);
			if(ret != 0)
				printf("V4L2Dev: Unable to enumerate frame sizes.\n");
		}
		else if(fmt.index < max_formats) {
			supported_formats[fmt.index] = fmt.pixelformat;
		}

		fmt.index++;
	}
	if (errno != EINVAL) {
		perror("V4L2Dev: ERROR enumerating frame formats");
		return errno;
	}

	return 0;
}
