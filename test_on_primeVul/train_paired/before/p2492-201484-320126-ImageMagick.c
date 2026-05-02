static inline Quantum GetPixelChannel(const Image *magick_restrict image,
  const PixelChannel channel,const Quantum *magick_restrict pixel)
{
  if (image->channel_map[channel].traits == UndefinedPixelTrait)
    return((Quantum) 0);
  return(pixel[image->channel_map[channel].offset]);
}