MagickExport MagickStatusType ParseMetaGeometry(const char *geometry,ssize_t *x,
  ssize_t *y,size_t *width,size_t *height)
{
  GeometryInfo
    geometry_info;

  MagickStatusType
    flags;

  size_t
    former_height,
    former_width;

  /*
    Ensure the image geometry is valid.
  */
  assert(x != (ssize_t *) NULL);
  assert(y != (ssize_t *) NULL);
  assert(width != (size_t *) NULL);
  assert(height != (size_t *) NULL);
  if ((geometry == (char *) NULL) || (*geometry == '\0'))
    return(NoValue);
  (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",geometry);
  /*
    Parse geometry using GetGeometry.
  */
  SetGeometryInfo(&geometry_info);
  former_width=(*width);
  former_height=(*height);
  flags=GetGeometry(geometry,x,y,width,height);
  if ((flags & PercentValue) != 0)
    {
      MagickStatusType
        percent_flags;

      PointInfo
        scale;

      /*
        Geometry is a percentage of the image size.
      */
      percent_flags=ParseGeometry(geometry,&geometry_info);
      scale.x=geometry_info.rho;
      if ((percent_flags & RhoValue) == 0)
        scale.x=100.0;
      scale.y=geometry_info.sigma;
      if ((percent_flags & SigmaValue) == 0)
        scale.y=scale.x;
      *width=(size_t) MagickMax(floor(scale.x*former_width/100.0+0.5),1.0);
      *height=(size_t) MagickMax(floor(scale.y*former_height/100.0+0.5),1.0);
      former_width=(*width);
      former_height=(*height);
    }
  if ((flags & AspectRatioValue) != 0)
    {
      double
        geometry_ratio,
        image_ratio;

      /*
        Geometry is a relative to image size and aspect ratio.
      */
      (void) ParseGeometry(geometry,&geometry_info);
      geometry_ratio=geometry_info.rho;
      image_ratio=(double) former_width*
        PerceptibleReciprocal((double) former_height);
      if (geometry_ratio >= image_ratio)
        {
          *width=former_width;
          *height=(size_t) floor((double) (former_height*image_ratio/
            geometry_ratio)+0.5);
        }
      else
        {
          *width=(size_t) floor((double) (former_width*geometry_ratio/
            image_ratio)+0.5);
          *height=former_height;
        }
      former_width=(*width);
      former_height=(*height);
    }
  if (((flags & AspectValue) != 0) || ((*width == former_width) &&
      (*height == former_height)))
    {
      if ((flags & RhoValue) == 0)
        *width=former_width;
      if ((flags & SigmaValue) == 0)
        *height=former_height;
    }
  else
    {
      double
        scale_factor;

      /*
        Respect aspect ratio of the image.
      */
      if ((former_width == 0) || (former_height == 0))
        scale_factor=1.0;
      else
        if (((flags & RhoValue) != 0) && (flags & SigmaValue) != 0)
          {
            scale_factor=(double) *width/(double) former_width;
            if ((flags & MinimumValue) == 0)
              {
                if (scale_factor > ((double) *height/(double) former_height))
                  scale_factor=(double) *height/(double) former_height;
              }
            else
              if (scale_factor < ((double) *height/(double) former_height))
                scale_factor=(double) *height/(double) former_height;
          }
        else
          if ((flags & RhoValue) != 0)
            {
              scale_factor=(double) *width/(double) former_width;
              if (((flags & MinimumValue) != 0) &&
                  (scale_factor < ((double) *width/(double) former_height)))
                scale_factor=(double) *width/(double) former_height;
            }
          else
            {
              scale_factor=(double) *height/(double) former_height;
              if (((flags & MinimumValue) != 0) &&
                  (scale_factor < ((double) *height/(double) former_width)))
                scale_factor=(double) *height/(double) former_width;
            }
      *width=MagickMax((size_t) floor(scale_factor*former_width+0.5),1UL);
      *height=MagickMax((size_t) floor(scale_factor*former_height+0.5),1UL);
    }
  if ((flags & GreaterValue) != 0)
    {
      if (former_width < *width)
        *width=former_width;
      if (former_height < *height)
        *height=former_height;
    }
  if ((flags & LessValue) != 0)
    {
      if (former_width > *width)
        *width=former_width;
      if (former_height > *height)
        *height=former_height;
    }
  if ((flags & AreaValue) != 0)
    {
      double
        area,
        distance;

      PointInfo
        scale;

      /*
        Geometry is a maximum area in pixels.
      */
      (void) ParseGeometry(geometry,&geometry_info);
      area=geometry_info.rho+sqrt(MagickEpsilon);
      distance=sqrt((double) former_width*former_height);
      scale.x=(double) former_width*PerceptibleReciprocal(distance/sqrt(area));
      scale.y=(double) former_height*PerceptibleReciprocal(distance/sqrt(area));
      if ((scale.x < (double) *width) || (scale.y < (double) *height))
        {
          *width=(unsigned long) (former_width*PerceptibleReciprocal(
            distance/sqrt(area)));
          *height=(unsigned long) (former_height*PerceptibleReciprocal(
            distance/sqrt(area)));
        }
      former_width=(*width);
      former_height=(*height);
    }
  return(flags);
}