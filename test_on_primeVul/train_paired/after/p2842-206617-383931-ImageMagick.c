static int HistogramCompare(const void *x,const void *y)
{
  const PixelInfo
    *color_1,
    *color_2;

  color_1=(const PixelInfo *) x;
  color_2=(const PixelInfo *) y;
  if (color_2->red != color_1->red)
    return((int) ((ssize_t) color_1->red-(ssize_t) color_2->red));
  if (color_2->green != color_1->green)
    return((int) ((ssize_t) color_1->green-(ssize_t) color_2->green));
  if (color_2->blue != color_1->blue)
    return((int) ((ssize_t) color_1->blue-(ssize_t) color_2->blue));
  return((int) ((ssize_t) color_2->count-(ssize_t) color_1->count));
}