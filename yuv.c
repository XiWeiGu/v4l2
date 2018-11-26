/*************************************************************************
        > File Name: yuv.c
        > Author: gxw
        > Mail: 2414434710@qq.com
        > Created Time: 2018年06月21日 星期四 15时34分37秒
 ************************************************************************/
#include "yuv.h"

/**
 *  * Convert YUYV(packed YUV 4:2:2 16bpp) file to RGB24 file
 *  * @param width        Width of input YUYV file.
 *  * @param height       Height of input YUYV file.
 *  * @param src          YUYV data.
 *  * @param dst          output RGB24 data.
 *  */
void YUYVtoRGB24(int width, int height, unsigned char* src,
                 unsigned char* dst) {
  int nLength = width * height;
  unsigned char YUYV[4], RGB[6];
  nLength <<= 1;
  int i, j, Location;
  j = 0;
  for (i = 0; i < nLength; i += 4) {
    /* Y0 */
    YUYV[0] = src[i];
    /* U */
    YUYV[1] = src[i + 1];
    /* Y1 */
    YUYV[2] = src[i + 2];
    /* V */
    YUYV[3] = src[i + 3];

    RGB[0] = 1.164 * (YUYV[0] - 16) + 1.596 * (YUYV[3] - 128);
    RGB[1] = 1.164 * (YUYV[0] - 16) - 0.813 * (YUYV[3] - 128) -
             0.394 * (YUYV[1] - 128);
    RGB[2] = 1.164 * (YUYV[0] - 16) + 2.018 * (YUYV[1] - 128);

    RGB[3] = 1.164 * (YUYV[2] - 16) + 1.596 * (YUYV[3] - 128);
    RGB[4] = 1.164 * (YUYV[2] - 16) - 0.813 * (YUYV[3] - 128) -
             0.394 * (YUYV[1] - 128);
    RGB[5] = 1.164 * (YUYV[2] - 16) + 2.018 * (YUYV[1] - 128);

    int k = 0;
    for (k = 0; k < 6; ++k) {
      if (RGB[k] < 0) RGB[k] = 0;
      if (RGB[k] > 255) RGB[k] = 255;
    }

    memcpy(dst + j, RGB, 6);
    j += 6;
  }
}

/**
 *  * Convert RGB24 file to BMP file
 *  * @param rgb24path    RGB24 data.
 *  * @param width        Width of input RGB file.
 *  * @param height       Height of input RGB file.
 *  * @param url_out      Location of Output BMP file.
 *  */
int RGB24toBMP(int width, int height, unsigned char* src, const char* bmppath) {
#pragma pack(1)
  typedef struct {
    int imageSize;
    int blank;
    int startPosition;
  } BmpHead;
#pragma pack()

#pragma pack(1)
  typedef struct {
    int Length;
    int width;
    int height;
    unsigned short colorPlane;
    unsigned short bitColor;
    int zipFormat;
    int realSize;
    int xPels;
    int yPels;
    int colorUse;
    int colorImportant;
  } InfoHead;
#pragma pack()

  int i = 0, j = 0;
  BmpHead m_BMPHeader = {0};
  InfoHead m_BMPInfoHeader = {0};
  char bfType[2] = {'B', 'M'};
  /* the size of bmp header is 54Bit */
  int header_size = sizeof(bfType) + sizeof(BmpHead) + sizeof(InfoHead);
  FILE* fp_bmp = NULL;

  if ((fp_bmp = fopen(bmppath, "wb")) == NULL) {
    printf("Error: Cannot open output BMP file.\n");
    return -1;
  }

  m_BMPHeader.imageSize = 3 * width * height + header_size;
  m_BMPHeader.startPosition = header_size;

  m_BMPInfoHeader.Length = sizeof(InfoHead);
  m_BMPInfoHeader.width = width;
  /* BMP storage pixel data in opposite direction of Y-axis (from bottom to
   * top). */
  m_BMPInfoHeader.height = -height;
  m_BMPInfoHeader.colorPlane = 1;
  m_BMPInfoHeader.bitColor = 24;
  m_BMPInfoHeader.realSize = 3 * width * height;

  fwrite(bfType, 1, sizeof(bfType), fp_bmp);
  fwrite(&m_BMPHeader, 1, sizeof(m_BMPHeader), fp_bmp);
  fwrite(&m_BMPInfoHeader, 1, sizeof(m_BMPInfoHeader), fp_bmp);

  /* BMP save R1|G1|B1,R2|G2|B2 as B1|G1|R1,B2|G2|R2
   * It saves pixel data in Little Endian
   * So we change 'R' and 'B'
   */
  for (j = 0; j < height; j++) {
    for (i = 0; i < width; i++) {
      char temp = src[(j * width + i) * 3 + 2];
      src[(j * width + i) * 3 + 2] = src[(j * width + i) * 3 + 0];
      src[(j * width + i) * 3 + 0] = temp;
    }
  }
  fwrite(src, 3 * width * height, 1, fp_bmp);
  fclose(fp_bmp);
  printf("Finish generate %s!\n", bmppath);
  return 0;
}

/**
 *  * Convert YUYV(packed YUV 4:2:2 16bpp) file to YUV420P(planar YUV 4:2:0 12bpp) file
 *  * @param width        Width of input YUYV file.
 *  * @param height       Height of input YUYV file.
 *  * @param src          YUYV data.
 *  * @param dst          output YUV420P data.
 *  */
void YUYVtoYUV420P(int width, int height, const unsigned char* src,
                   unsigned char* dst) {
  if (!dst) return;

  const int s1 = width * height * 2;
  const int s2 = width * 2;

  unsigned char* pY = dst;
  unsigned char* pU = pY + width * height;
  unsigned char* pV = pU + width * height / 4;
  bool uFlags = true;

  int i;
  for (i = 0; i < s1; ++i) {
    if (i % 2 == 0) {
      *(pY++) = src[i];
    } else if ((i / s2) % 2 == 0) {
      if (uFlags)
        *(pU++) = src[i];
      else
        *(pV++) = src[i];
      uFlags = !uFlags;
    }
  }
}
