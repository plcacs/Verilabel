static int IntensityCompare(const void *x,const void *y)
{
  PixelPacket
    *color_1,
    *color_2;

  ssize_t
    intensity;

  color_1=(PixelPacket *) x;
  color_2=(PixelPacket *) y;
  intensity=(ssize_t) ConstrainPixelIntensity(PixelPacketIntensity(color_1))-
    (ssize_t) ConstrainPixelIntensity(PixelPacketIntensity(color_2));
  return((int) intensity);
}