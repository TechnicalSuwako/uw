#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "dep/stb_image.h"

const char *sofname = "uw";
const char *version = "0.1.0";
const char *avalopt = "cft";

typedef unsigned char uchar;
typedef unsigned long ulong;

extern void XDestroyImage(XImage *ximage);
extern void XPutPixel(XImage *scaled_image, int x, int y, ulong pixel);

Display *dp;
Window w;
Pixmap pm;

void wp_center(int sw, int sh, int iw, int ih) {
  GC gc = XCreateGC(dp, pm, 0, NULL);
  if (!gc) {
    fprintf(stderr, "GCの作成に失敗。\n");
    return;
  }

  int xoff = (sw - iw) / 2;
  int yoff = (sh - ih) / 2;

  XClearWindow(dp, w);
  XCopyArea(dp, pm, w, gc, 0, 0, iw, ih, xoff, yoff);
  XFreeGC(dp, gc);
}

void wp_fill(int sw, int sh, uchar *img_data, int iw, int ih, int depth) {
  Visual *v = DefaultVisual(dp, DefaultScreen(dp));
  XImage *scale = XCreateImage(dp, v, depth, ZPixmap, 0, NULL, sw, sh, 32, 0);

  if (!scale) {
    fprintf(stderr, "スケールする為にXImageの作成に失敗。\n");
    return;
  }

  scale->data = (char *)malloc(sw * sh * 4);
  if (!scale->data) {
    fprintf(stderr, "スケールしたデータにメモリの役割に失敗。\n");
    XDestroyImage(scale);
    return;
  }

  for (int y = 0; y < sh; y++) {
    for (int x = 0; x < sw; x++) {
      int srcx = (x * iw) / sw;
      int srcy = (y * ih) / sh;

      int sidx = (srcy * iw + srcx) * 4;
      if (sidx >= iw * ih * 4) continue;

      uchar blue = img_data[sidx];
      uchar green = img_data[sidx + 1];
      uchar red = img_data[sidx + 2];
      uchar alpha = img_data[sidx + 3];

      ulong pixel = (alpha << 24) | (red << 16) | (green << 8) | blue;
      XPutPixel(scale, x, y, pixel);
    }
  }

  GC gc = XCreateGC(dp, w, 0, NULL);
  if (!gc) {
    fprintf(stderr, "GCの作成に失敗。\n");
    free(scale->data);
    XDestroyImage(scale);
    return;
  }

  XClearWindow(dp, w);
  XPutImage(dp, w, gc, scale, 0, 0, 0, 0, sw, sh);
  XFreeGC(dp, gc);
  free(scale->data);
  /*XDestroyImage(scale);*/
}

void wp_tile(int sw, int sh, int iw, int ih) {
  GC gc = XCreateGC(dp, w, 0, NULL);
  if (!gc) {
    fprintf(stderr, "GCの作成に失敗。\n");
    return;
  }

  XClearWindow(dp, w);

  for (int y = 0; y < sh; y += ih) {
    for (int x = 0; x < sw; x += iw) {
      XCopyArea(dp, pm, w, gc, 0, 0, iw, ih, x, y);
    }
  }
  XFreeGC(dp, gc);
}

void usage() {
  printf("%s-%s\nusage: %s [-%s] <image>\n", sofname, version, sofname, avalopt);
}

int setmode(
  char *mode,
  uchar *data,
  int sw,
  int sh,
  int iw,
  int ih,
  int depth

) {
  // TODO: 今は全部「-t」みたいな結果になる・・・
  if (strncmp(mode, "-c", 2) == 0) {
    wp_center(sw, sh, iw, ih);
    return 0;
  } else if (strncmp(mode, "-f", 2) == 0) {
    wp_fill(sw, sh, data, iw, ih, depth);
    return 0;
  } else if (strncmp(mode, "-t", 2) == 0) {
    wp_tile(sw, sh, iw, ih);
    return 0;
  }

  usage();
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage();
    return -1;
  }

  int screen;
  GC gc;
  XImage *ximage = NULL;
  int iw, ih, chan;
  uchar *data = NULL;

  char *mode = argv[1];
  char *path = argv[2];

  dp = XOpenDisplay(NULL);
  if (dp == NULL) {
    fprintf(stderr, "X画面を開くに失敗。\n");
    return -1;
  }

  screen = DefaultScreen(dp);
  w = RootWindow(dp, screen);
  int sw = DisplayWidth(dp, screen);
  int sh = DisplayHeight(dp, screen);

  data = stbi_load(path, &iw, &ih, &chan, 4);
  if (!data) {
    fprintf(stderr, "画像の読み込みに失敗： %s\n", path);
    XCloseDisplay(dp);
    return -1;
  }

  Visual *v = DefaultVisual(dp, screen);
  int depth = DefaultDepth(dp, screen);

  if (depth != 24 && depth != 32) {
    fprintf(stderr, "「%d」の様な視覚の深さは未対応です。\n", depth);
    stbi_image_free(data);
    XCloseDisplay(dp);
    return -1;
  }

  pm = XCreatePixmap(dp, w, iw, ih, depth);
  uchar *cdata = NULL;

  if (depth == 24) {
    cdata = malloc(iw * ih * 4);
    if (cdata == NULL) {
      fprintf(stderr, "画像変更の為にメモリの役割に失敗。\n");
      stbi_image_free(data);
      XCloseDisplay(dp);
      return -1;
    }

    for (int i = 0; i < iw * ih; i++) {
      cdata[i * 4 + 0] = data[i * 4 + 2];
      cdata[i * 4 + 1] = data[i * 4 + 1];
      cdata[i * 4 + 2] = data[i * 4 + 0];
      cdata[i * 4 + 3] = data[i * 4 + 3];
    }

    ximage = XCreateImage(dp, v, depth, ZPixmap, 0, (char *)cdata, iw, ih, 32, 0);
    if (setmode(mode, cdata, sw, sh, iw, ih, depth) != 0) {
      XFreePixmap(dp, pm);
      XFree(ximage);
      free((void *)cdata);
      stbi_image_free(data);
      XCloseDisplay(dp);
      return 1;
    }
  } else {
    ximage = XCreateImage(dp, v, depth, ZPixmap, 0, (char *)data, iw, ih, 32, 0);
    if (setmode(mode, data, sw, sh, iw, ih, depth) != 0) {
      XFreePixmap(dp, pm);
      XFree(ximage);
      stbi_image_free(data);
      XCloseDisplay(dp);
      return 1;
    }
  }

  if (!ximage) {
    fprintf(stderr, "XImageの作成に失敗。\n");
    if (cdata) free((void *)cdata);
    stbi_image_free(data);
    XCloseDisplay(dp);
    return EXIT_FAILURE;
  }

  gc = XCreateGC(dp, pm, 0, NULL);
  if (!gc) {
    fprintf(stderr, "GCの作成に失敗。\n");
    if (cdata) free((void *)cdata);
    XFreePixmap(dp, pm);
    XFree(ximage);
    stbi_image_free(data);
    XCloseDisplay(dp);
    return EXIT_FAILURE;
  }

  XPutImage(dp, pm, gc, ximage, 0, 0, 0, 0, iw, ih);

  XSetWindowBackgroundPixmap(dp, w, pm);
  XClearWindow(dp, w);
  XFlush(dp);

  XFreeGC(dp, gc);
  XFreePixmap(dp, pm);
  XFree(ximage);
  stbi_image_free(data);
  if (cdata) free((void *)cdata);
  XCloseDisplay(dp);

  return 0;
}
