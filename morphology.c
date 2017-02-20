/*
* Intel Corp
* Copyright (C) 2017 Sudip Jain <sudip.jain@intel.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

#define TILE_WIDTH 64
#define TILE_HEIGHT (TILE_WIDTH)
#define TILE_SIZE (TILE_WIDTH * TILE_HEIGHT)
#define MAX_THREADS 1024

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef long int64;

pthread_mutex_t lock;

#define dimof(x) (sizeof(x)/sizeof(x[0]))

typedef enum
{
  FILTER_DILATE = 0,
  FILTER_ERODE = 1
} filter;

struct Str
{
  void *src_ptr;
  void *dst_ptr;
  uint32 w;
  uint32 h;
  uint32 tile_w;
  uint32 tile_h;
  uint32 tid;
  filter f;
};


static int64
getTickCount (void)
{
  struct timeval tv;
  struct timezone tz;
  gettimeofday (&tv, &tz);
  return (int64) tv.tv_sec * 1000000 + tv.tv_usec;
}

static uint8_t
min_op (uint8_t a, uint8_t b)
{
  return a < b ? a : b;
}

static uint8
max_op (uint8 a, uint8 b)
{
  return a > b ? a : b;
}

static void
swap (void **a, void **b)
{
  void *tmp;

  tmp = *a;
  *a = *b;
  *b = tmp;
}

static void
GetNbhdPixels (uint8_t * src, int x, int y, int w, int h, uint8_t * p)
{
  int l_nh, r_nh, t_nh, b_nh;

  int8_t nbhd_filter[] = { -1, -1, 1, 1 };

  // apply filter
  l_nh = y + nbhd_filter[0];
  t_nh = x + nbhd_filter[1];

  r_nh = y + nbhd_filter[2];
  b_nh = x + nbhd_filter[3];

  // check boundaries
  l_nh = (l_nh < 0) ? y : l_nh;
  t_nh = (t_nh < 0) ? x : t_nh;
  r_nh = (r_nh >= w) ? y : r_nh;
  b_nh = (b_nh >= h) ? x : b_nh;

  //copy pixel values
  p[0] = *(src + (x * w) + y);
  p[1] = *((src + (x * w) + l_nh));     //left pixel
  p[2] = *((src + (t_nh * w) + y));     //top pixel
  p[3] = *((src + (x * w) + r_nh));     //right pixel
  p[4] = *((src + (b_nh * w) + y));     //bottom pixel

}

static void
Tile_MxN (uint8_t * src, uint8_t * dst, int w, int h,
    int tile_w, int tile_h, int m, int n, filter f)
{
  uint8 pixels[5];
  uint8 M;
  uint32 count = 0;
  uint32 offset = 0;

  if (tile_w == 0)
    tile_w = w;
  if (tile_h == 0)
    tile_h = h;

  offset = m * w + n;

  for (int i = m; i < (m + tile_h); i++) {
    for (int j = n; j < (n + tile_w); j++) {
      GetNbhdPixels (src, i, j, w, h, pixels);
      M = pixels[0];
      for (int k = 1; k < dimof (pixels); k++) {
        M = (f) ? min_op (M, pixels[k]) : max_op (M, pixels[k]);
      }

      *(dst + (i * w + j)) = M;
      offset++;
      count++;
    }
  }
}

void *
FilterThread (void *args)
{
  struct Str *s = (struct Str *) args;
  uint32 horz_tiles, vert_tiles;
  int x_off, y_off;

  pthread_mutex_lock (&lock);
  int tile_id = s->tid++;
  pthread_mutex_unlock (&lock);

  horz_tiles = s->w / s->tile_w;
  vert_tiles = s->h / s->tile_h;

  x_off = (tile_id / horz_tiles) * s->tile_h;
  y_off = (tile_id % horz_tiles) * s->tile_w;

  Tile_MxN (s->src_ptr, s->dst_ptr, s->w, s->h, s->tile_w,
      s->tile_h, x_off, y_off, s->f);

  pthread_exit (NULL);
  //return NULL;
}

int
main (int argc, char *argv[])
{
  FILE *fp_r = NULL;
  FILE *fp_w = NULL;
  char *input_file = NULL;
  char output_file[256];
  char name[256] = { 0 };
  int w = 0, h = 0, size = 0;
  uint32 tile_w = 0, tile_h = 0;
  uint32 nb_tiles, nb_threads;
  int n_dilate = 1, n_erode = 1;
  int opt;
  int64 t0, t1;
  struct Str *info;
  pthread_t threads[MAX_THREADS];
  pthread_mutex_t lock;

  if (argc < 2) {
    fprintf (stderr, "error insufficient arguments\n");
    fprintf (stderr,
        "Usage: ./morphology <filename> -d <dilations> -e <erosion>\n");
    exit (EXIT_FAILURE);
  }

  input_file = argv[1];
  sprintf (output_file, "%s%s", "out_", input_file);

  fp_r = fopen (input_file, "r");
  fp_w = fopen (output_file, "w");

  if ((fp_r == NULL) || (fp_w == NULL)) {
    fprintf (stderr, "error opening file\n");
    fprintf (stderr,
        "Note:file name must be in format <shortname>_<width>_<height>.<ext>\n");
    exit (EXIT_FAILURE);
  }

  sscanf (input_file, "%256[a-zA-Z0-9]_%ux%u", name, &w, &h);

  int index = 1;
  while ((opt = getopt (argc, argv, "det")) != -1) {
    switch (opt) {
      case 'd':
        index += 2;
        n_dilate = atoi (argv[index]);
        break;
      case 'e':
        index += 2;
        n_erode = atoi (argv[index]);
        break;
      case 't':
        index += 2;
        tile_w = atoi (argv[index]);
        tile_w = (tile_w > w) ? w : tile_w;
        tile_h = tile_w;
        break;
      default:
        fprintf (stderr,
            "Usage: ./morphology <filename> -d <dilations> -e <erosion>\n");
        fprintf (stderr, "e.g: ./morphology pentagram_64x64 .raw -d 2 -e 1\n");
        exit (EXIT_FAILURE);
    }
  }

  size = w * h;
  if (size % 64 != 0) {
    fprintf (stderr, "Incorrect dimensions \n");
    exit (EXIT_FAILURE);
  }

  info = malloc (sizeof (struct Str));
  info->src_ptr = (uint8_t *) malloc (sizeof (uint8_t) * size);
  fread (info->src_ptr, 1, size, fp_r);
  info->dst_ptr = (uint8_t *) malloc (sizeof (uint8_t) * size);

  info->w = w;
  info->h = h;

  info->tile_w = ((tile_w == 0) || (tile_w % 16 != 0)) ? TILE_WIDTH : tile_w;
  info->tile_h = ((tile_h == 0) || (tile_h % 16 != 0)) ? TILE_HEIGHT : tile_h;

  nb_tiles = size / (info->tile_w * info->tile_h);
  nb_threads = (nb_tiles <= MAX_THREADS) ? nb_tiles : MAX_THREADS;

  //print info before processing
  fprintf (stderr, "Input file: %s\n", input_file);
  fprintf (stderr, "Output file: %s\n", output_file);
  fprintf (stderr, "Width: %d, Height: %d\n", w, h);
  fprintf (stderr, "Num dilations: %d\n", n_dilate);
  fprintf (stderr, "Num erosions: %d\n", n_erode);
  fprintf (stderr, "Page size: %d\n", getpagesize ());
  fprintf (stderr, "Tile %ux%u\n", info->tile_w, info->tile_h);
  fprintf (stderr, "Tiles %u\n", nb_tiles);
  fprintf (stderr, "Threads: %u\n", nb_threads);

  // start timer
  t0 = getTickCount ();

  if (pthread_mutex_init (&lock, NULL) != 0)
    fprintf (stderr, "failed mutex init \n");

  if (n_erode) {
    info->f = FILTER_ERODE;
    do {
      info->tid = 0;

      for (int t = 0; t < nb_threads; t++)
        if (pthread_create (&threads[t], NULL, FilterThread,
                (void *) info) != 0)
          fprintf (stderr, "failed thread create \n");

      for (int t = 0; t < nb_threads; t++)
        pthread_join (threads[t], NULL);

      /* no need to keep ref of src ptr, swap ptrs to recycle, */
      swap (&info->src_ptr, &info->dst_ptr);

    } while (--n_erode);
  }

  if (n_dilate) {
    info->f = FILTER_DILATE;
    do {
      info->tid = 0;

      for (int t = 0; t < nb_threads; t++)
        if (pthread_create (&threads[t], NULL, FilterThread,
                (void *) info) != 0)
          fprintf (stderr, "failed thread create \n");

      for (int t = 0; t < nb_threads; t++)
        pthread_join (threads[t], NULL);

      /* no need to keep ref of src ptr, swap ptrs to recycle, */
      swap (&info->src_ptr, &info->dst_ptr);

    } while (--n_dilate);
  }

  fwrite (info->src_ptr, 1, size, fp_w);

  // end timer
  t1 = getTickCount ();
  fprintf (stderr, "Total latency in ms: %g\n", (t1 - t0) / 1e3);

  free (info->src_ptr);
  free (info->dst_ptr);

  pthread_mutex_destroy (&lock);

exit:
  return 0;
}
