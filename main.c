/*************************************************************************
        > File Name: main.c
        > Author: gxw
        > Mail: 2414434710@qq.com
        > Created Time: 2018年06月18日 星期一 13时55分22秒
 ************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <x264.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "yuv.h"


struct buffer {
  void* start;
  size_t length;
};
struct buffer* buffers = NULL;
static int cameraFD;
FILE* fp_yuyv;


// x264 param
x264_param_t param;
x264_picture_t pic;
x264_picture_t pic_out;
x264_t *h;
x264_nal_t *nal;
int i_frame;
int i_frame_size;
int i_nal;

static int read_frame(void) {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (-1 == ioctl(cameraFD, VIDIOC_DQBUF, &buf)) return -1;
#if 0
  unsigned char* rgb24 = malloc(640 * 480 * 3 * sizeof(unsigned char));
  const char* bmppath = "rgb24.bmp";
  if (!rgb24) return -1;
  YUYVtoRGB24(640, 480, buffers[buf.index].start, rgb24);
  RGB24toBMP(640, 480, rgb24, bmppath);
  free(rgb24);
#else
  unsigned char* yuv = malloc(640 * 480 * 3 * sizeof(unsigned char) / 2);
  if (!yuv) return -1;
  YUYVtoYUV420P(640, 480, buffers[buf.index].start, yuv);
  //if ( !fwrite(buffers[buf.index].start, 640 * 480 * 2, 1, fp_yuyv))
    //return -1;
  //if ( !fwrite(yuv, 640 * 480 * 3 / 2, 1, fp_yuyv))
    //return -1;
  pic.img.plane[0] = yuv;
  pic.img.plane[1] = yuv + 640 * 480 * sizeof(unsigned char);
  pic.img.plane[2] = pic.img.plane[1] + 640 * 480 / 4 * sizeof(unsigned char);  
  pic.i_pts = i_frame++;
  i_frame_size = x264_encoder_encode( h, &nal, &i_nal, &pic, &pic_out );
  printf("%d\n", i_frame_size);
  if( i_frame_size < 0 )
      return -1;
  else if( i_frame_size )
  {
      if( !fwrite( nal->p_payload, i_frame_size, 1, fp_yuyv ) )
          return -1;
  }
  free(yuv);
#endif
  if (-1 == ioctl(cameraFD, VIDIOC_QBUF, &buf)) return -1;
  return 0;
}

int main() {
  cameraFD = open("/dev/video0", O_RDWR);
  if (cameraFD == -1)
    return -1;
  else
    printf("success!\n");
  v4l2_std_id std;
  int ret;
  struct v4l2_capability cap;
  if (ioctl(cameraFD, VIDIOC_QUERYCAP, &cap) == -1) printf("error\n");
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) printf("error\n");
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    printf("error\n");
    return -1;
  };
  struct v4l2_fmtdesc fmt1;
  memset(&fmt1, 0, sizeof(fmt1));
  fmt1.index = 0;
  fmt1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  while ((ioctl(cameraFD, VIDIOC_ENUM_FMT, &fmt1)) ==
         0)  //获取当前视频设备的帧格式 比如：YUYV
  {
    fmt1.index++;
    printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
           fmt1.pixelformat & 0xFF, (fmt1.pixelformat >> 8) & 0xFF,
           (fmt1.pixelformat >> 16) & 0xFF, (fmt1.pixelformat >> 24) & 0xFF,
           fmt1.description);
  };

  struct v4l2_format fmt;
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 640;
  fmt.fmt.pix.height = 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  if (-1 == ioctl(cameraFD, VIDIOC_S_FMT, &fmt)) {
    printf("VIDIOC_S_FMT error\n");
    return -1;
  }
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
    printf("pixelformat error\n");
    return -1;
  }

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (-1 == ioctl(cameraFD, VIDIOC_REQBUFS, &req)) {
    printf("VIDIOC_REQBUFS error!\n");
    return -1;
  }
  if (req.count < 2) {
    printf("Insufficient buffer memory error\n");
    return -2;
  }
  buffers = calloc(req.count, sizeof(*buffers));
  if (!buffers) {
    printf("calloc buffers failed!\n");
    return -1;
  }
  int n_buffers;
  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;
    if (-1 == ioctl(cameraFD, VIDIOC_QUERYBUF, &buf)) {
      printf("VIDIOC_QUERYBUF\n");
      return -1;
    }
    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(NULL,  // start anywhere
                                    buf.length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, cameraFD, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start) {
      printf("mmap error\n");
      return -1;
    }
  }
  int i = 0;
  for (; i < n_buffers; ++i) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (-1 == ioctl(cameraFD, VIDIOC_QBUF, &buf)) {
      printf("VIDIOC_QBUF error\n");
      return -1;
    }
  }
  if ((fp_yuyv = fopen("out.h264", "wb")) == NULL) {
    printf("Error: Cannot open output yuv file.\n");
    goto fail;
  }
  // 开启摄像头
  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == ioctl(cameraFD, VIDIOC_STREAMON, &type)) {
    printf("VIDIOC_STREAMON error\n");
    return -1;
  }
  // init x264
  if (x264_param_default_preset(&param, "medium", NULL) < 0)
    goto fail;
  param.i_bitdepth = 8;
  param.i_csp = X264_CSP_I420;
  param.i_width  = 640;
  param.i_height = 480;
  param.b_vfr_input = 0;
  param.b_repeat_headers = 1;
  param.b_annexb = 1;
  /* Apply profile restrictions. */
  if( x264_param_apply_profile( &param, "high" ) < 0 )
      goto fail;

  if( x264_picture_alloc( &pic, param.i_csp, param.i_width, param.i_height ) < 0 )
      goto fail;
  h = x264_encoder_open( &param );
  if( !h )
      goto fail;
  i_frame = 0;
  // 采集循环
  uint32_t count = 2000;
  while (count--) {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;
      FD_ZERO(&fds);
      FD_SET(cameraFD, &fds);
      /* timeout */
      tv.tv_sec = 4;
      tv.tv_usec = 0;
      r = select(cameraFD + 1, &fds, NULL, NULL, &tv);
      if (r == -1) {
        if (EINTR == errno) continue;
        return -1;
      }
      if (r == 0) {
        printf("select timeout!\n");
        return -1;
      }
      if (read_frame() == -1)
        return -1;
      else
        break;
    }
  }
  /* Flush delayed frames */
  while( x264_encoder_delayed_frames( h ) )
  {
      i_frame_size = x264_encoder_encode( h, &nal, &i_nal, NULL, &pic_out );
      if( i_frame_size < 0 )
          goto fail;
      else if( i_frame_size )
      {
          if( !fwrite( nal->p_payload, i_frame_size, 1, stdout ) )
              goto fail;
      }
  }

  /* stop capture */
  x264_encoder_close( h );
  x264_picture_clean( &pic );
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == ioctl(cameraFD, VIDIOC_STREAMOFF, &type)) return -1;
  /* munmap */
  for (i = 0; i < n_buffers; ++i)
    if (-1 == munmap(buffers[i].start, buffers[i].length)) return -1;
  free(buffers);
  /* close device */
  if (-1 == close(cameraFD)) return -1;
  fclose(fp_yuyv);
  return 0;

fail:
  return -1;
}
