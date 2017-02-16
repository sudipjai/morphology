/*
 *  Copyright (C) 2016 Intel Corp
 *  Author: Sudip Jain <sudip.jain@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef long int64;

#define dimof(x) (sizeof(x)/sizeof(x[0]))

static
int64 getTickCount(void)
{
   struct timeval tv;
   struct timezone tz;
   gettimeofday(&tv, &tz);
   return (int64)tv.tv_sec * 1000000 + tv.tv_usec;
}

static uint8_t min_op(uint8_t a, uint8_t b)
{
   return a < b ? a : b;
}

static uint8 max_op(uint8 a, uint8 b)
{
   return a > b ? a : b;
}

static
void GetNbhdPixels(uint8_t *src, int x, int y, int x_str, int y_str, uint8_t *p)
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
   r_nh = (r_nh >= x_str) ? y : r_nh;
   b_nh = (b_nh >= y_str) ? x : b_nh;

   //copy pixel values
   p[0] = *(src +  (x * x_str) + y);
   p[1] = *((src + (x * x_str) + l_nh)); //left pixel
   p[2] = *((src + (t_nh * x_str) + y)); //top pixel
   p[3] = *((src + (x * x_str) + r_nh)); //right pixel
   p[4] = *((src + (b_nh * x_str) + y)); //right pixel

}

static
void dilate(uint8_t *src, uint8_t *dst, int cols,  int rows)
{
   uint8 pixels[5];
   uint8 m;

   for (int i = 0; i < rows; i++)
   {
      for (int j = 0; j < cols; j++)
      {
         GetNbhdPixels(src, i, j, cols, rows, pixels);
         m = pixels[0];
         for (int k = 1; k < dimof(pixels); k++)
         {
            m = max_op(m, pixels[k]);
         }
         *dst++ = m;
      }
   }
}

static
void erode(uint8_t *src, uint8_t *dst, int cols,  int rows)
{
   uint8 pixels[5];
   uint8 m;

   for (int i = 0; i < rows; i++)
   {
      for (int j = 0; j < cols; j++)
      {
         GetNbhdPixels(src, i, j, cols, rows, pixels);
         m = pixels[0];
         for (int k = 1; k < dimof(pixels); k++)
         {
            m = min_op(m, pixels[k]);
         }
         *dst++ = m;
      }
   }
}

int main(int argc, char *argv[])
{
   FILE *fp_r = NULL;
   FILE *fp_w = NULL;
   char *input_file = NULL;
   char output_file[256];
   char name[256] = { 0 };
   int w = 0; int h = 0; int size = 0;
   uint8_t * src_ptr,*dst_ptr,*tmp_ptr;
   int n_dilate = 1, n_erode = 1;
   int opt;
   int64 t0, t1;

   if (argc < 2) {
       fprintf(stderr, "error insufficient arguments\n");
       fprintf(stderr, "Usage: ./morphology <filename> -d <dilations> -e <erosion>\n");
       exit(EXIT_FAILURE);
   }

   input_file = argv[1];
   sprintf(output_file, "%s%s", "out_", input_file);

   fp_r = fopen(input_file, "r");
   fp_w = fopen(output_file, "w");

   if ((fp_r == NULL) || (fp_w == NULL))
   {
      fprintf(stderr,"error opening file\n");
      fprintf(stderr,"Note:file name must be in format <shortname>_<width>_<height>.<ext>\n");
      exit(EXIT_FAILURE);
   }

   int index = 1;
   while ((opt = getopt(argc, argv, "de")) != -1)
   {
      switch (opt)
      {
      case 'd':
         index += 2;
         n_dilate = atoi(argv[index]);
         break;
      case 'e':
         index += 2;
         n_erode = atoi(argv[index]);
         break;
      default:
         fprintf(stderr, "Usage: ./morphology <filename> -d <dilations> -e <erosion>\n");
         fprintf(stderr, "e.g: ./morphology pentagram_64x64.raw -d 2 -e 1\n");
         exit(EXIT_FAILURE);
      }
   }

   sscanf(input_file, "%256[a-zA-Z0-9]_%ux%u", name, &w, &h);

   fprintf(stderr, "input file: %s\n", input_file);
   fprintf(stderr, "input file: %s\n", output_file);
   fprintf(stderr, "width: %d, height: %d\n", w, h);
   fprintf(stderr, "num dilations: %d\n", n_dilate);
   fprintf(stderr, "num erosions: %d\n", n_erode);

   size = w * h;
   if (size % 64 != 0)
   {
      fprintf(stderr, "Incorrect dimensions \n");
      exit(EXIT_FAILURE);
   }

   src_ptr = (uint8_t *)malloc(sizeof(uint8_t) * size);
   fread(src_ptr, 1, size, fp_r);

   dst_ptr = (uint8_t *)malloc(sizeof(uint8_t) * size);

   // start timer
   t0 = getTickCount();

   for (int e = 0; e < n_erode; e++)
   {
      erode(src_ptr, dst_ptr, w, h);

      /*  toggle src and dst to avoid copy, don't need src ptr anymore, */
      tmp_ptr = src_ptr;
      src_ptr = dst_ptr;
      dst_ptr = tmp_ptr;
   }

   for (int d = 0; d < n_dilate; d++)
   {
      dilate(src_ptr, dst_ptr, w, h);

      /*  toggle src and dst to avoid copy, don't need src ptr anymore, */
      tmp_ptr = src_ptr;
      src_ptr = dst_ptr;
      dst_ptr = tmp_ptr;
   }

   // end timer
   t1 = getTickCount();
   fprintf(stderr, "Total latency in ms: %g\n", (t1 - t0) / 1e3);

   // output to file, note src_ptr is swapped with dst_ptr
   fwrite(src_ptr, 1, size, fp_w);

   free(src_ptr);
   free(dst_ptr);

exit:
   return 0;
}
