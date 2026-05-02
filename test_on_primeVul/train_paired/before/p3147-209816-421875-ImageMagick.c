MagickExport MagickBooleanType InterpolatePixelInfo(const Image *image,
  const CacheView_ *image_view,const PixelInterpolateMethod method,
  const double x,const double y,PixelInfo *pixel,ExceptionInfo *exception)
{
  MagickBooleanType
    status;

  double
    alpha[16],
    gamma;

  PixelInfo
    pixels[16];

  register const Quantum
    *p;

  register ssize_t
    i;

  ssize_t
    x_offset,
    y_offset;

  PixelInterpolateMethod
    interpolate;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(image_view != (CacheView *) NULL);
  status=MagickTrue;
  x_offset=(ssize_t) floor(x);
  y_offset=(ssize_t) floor(y);
  interpolate=method;
  if (interpolate == UndefinedInterpolatePixel)
    interpolate=image->interpolate;
  GetPixelInfoPixel(image,(const Quantum *) NULL,pixel);
  (void) memset(&pixels,0,sizeof(pixels));
  switch (interpolate)
  {
    case AverageInterpolatePixel:  /* nearest 4 neighbours */
    case Average9InterpolatePixel:  /* nearest 9 neighbours */
    case Average16InterpolatePixel:  /* nearest 16 neighbours */
    {
      ssize_t
        count;

      count=2;  /* size of the area to average - default nearest 4 */
      if (interpolate == Average9InterpolatePixel)
        {
          count=3;
          x_offset=(ssize_t) (floor(x+0.5)-1);
          y_offset=(ssize_t) (floor(y+0.5)-1);
        }
      else if (interpolate == Average16InterpolatePixel)
        {
          count=4;
          x_offset--;
          y_offset--;
        }
      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,(size_t) count,
        (size_t) count,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      count*=count;  /* number of pixels - square of size */
      for (i=0; i < (ssize_t) count; i++)
      {
        AlphaBlendPixelInfo(image,p,pixels,alpha);
        gamma=PerceptibleReciprocal(alpha[0]);
        pixel->red+=gamma*pixels[0].red;
        pixel->green+=gamma*pixels[0].green;
        pixel->blue+=gamma*pixels[0].blue;
        pixel->black+=gamma*pixels[0].black;
        pixel->alpha+=pixels[0].alpha;
        p += GetPixelChannels(image);
      }
      gamma=1.0/count;   /* average weighting of each pixel in area */
      pixel->red*=gamma;
      pixel->green*=gamma;
      pixel->blue*=gamma;
      pixel->black*=gamma;
      pixel->alpha*=gamma;
      break;
    }
    case BackgroundInterpolatePixel:
    {
      *pixel=image->background_color;  /* Copy PixelInfo Structure  */
      break;
    }
    case BilinearInterpolatePixel:
    default:
    {
      PointInfo
        delta,
        epsilon;

      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < 4L; i++)
        AlphaBlendPixelInfo(image,p+i*GetPixelChannels(image),pixels+i,alpha+i);
      delta.x=x-x_offset;
      delta.y=y-y_offset;
      epsilon.x=1.0-delta.x;
      epsilon.y=1.0-delta.y;
      gamma=((epsilon.y*(epsilon.x*alpha[0]+delta.x*alpha[1])+delta.y*
        (epsilon.x*alpha[2]+delta.x*alpha[3])));
      gamma=PerceptibleReciprocal(gamma);
      pixel->red=gamma*(epsilon.y*(epsilon.x*pixels[0].red+delta.x*
        pixels[1].red)+delta.y*(epsilon.x*pixels[2].red+delta.x*pixels[3].red));
      pixel->green=gamma*(epsilon.y*(epsilon.x*pixels[0].green+delta.x*
        pixels[1].green)+delta.y*(epsilon.x*pixels[2].green+delta.x*
        pixels[3].green));
      pixel->blue=gamma*(epsilon.y*(epsilon.x*pixels[0].blue+delta.x*
        pixels[1].blue)+delta.y*(epsilon.x*pixels[2].blue+delta.x*
        pixels[3].blue));
      if (image->colorspace == CMYKColorspace)
        pixel->black=gamma*(epsilon.y*(epsilon.x*pixels[0].black+delta.x*
          pixels[1].black)+delta.y*(epsilon.x*pixels[2].black+delta.x*
          pixels[3].black));
      gamma=((epsilon.y*(epsilon.x+delta.x)+delta.y*(epsilon.x+delta.x)));
      gamma=PerceptibleReciprocal(gamma);
      pixel->alpha=gamma*(epsilon.y*(epsilon.x*pixels[0].alpha+delta.x*
        pixels[1].alpha)+delta.y*(epsilon.x*pixels[2].alpha+delta.x*
        pixels[3].alpha));
      break;
    }
    case BlendInterpolatePixel:
    {
      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < 4L; i++)
      {
        GetPixelInfoPixel(image,p+i*GetPixelChannels(image),pixels+i);
        AlphaBlendPixelInfo(image,p+i*GetPixelChannels(image),pixels+i,alpha+i);
      }
      gamma=1.0;  /* number of pixels blended together (its variable) */
      for (i=0; i <= 1L; i++)
      {
        if ((y-y_offset) >= 0.75)
          {
            alpha[i]=alpha[i+2];  /* take right pixels */
            pixels[i]=pixels[i+2];
          }
        else
          if ((y-y_offset) > 0.25)
            {
              gamma=2.0;  /* blend both pixels in row */
              alpha[i]+=alpha[i+2];  /* add up alpha weights */
              pixels[i].red+=pixels[i+2].red;
              pixels[i].green+=pixels[i+2].green;
              pixels[i].blue+=pixels[i+2].blue;
              pixels[i].black+=pixels[i+2].black;
              pixels[i].alpha+=pixels[i+2].alpha;
            }
      }
      if ((x-x_offset) >= 0.75)
        {
          alpha[0]=alpha[1];
          pixels[0]=pixels[1];
        }
      else
        if ((x-x_offset) > 0.25)
          {
            gamma*=2.0;  /* blend both rows */
            alpha[0]+= alpha[1];  /* add up alpha weights */
            pixels[0].red+=pixels[1].red;
            pixels[0].green+=pixels[1].green;
            pixels[0].blue+=pixels[1].blue;
            pixels[0].black+=pixels[1].black;
            pixels[0].alpha+=pixels[1].alpha;
          }
      gamma=1.0/gamma;
      alpha[0]=PerceptibleReciprocal(alpha[0]);
      pixel->red=alpha[0]*pixels[0].red;
      pixel->green=alpha[0]*pixels[0].green;  /* divide by sum of alpha */
      pixel->blue=alpha[0]*pixels[0].blue;
      pixel->black=alpha[0]*pixels[0].black;
      pixel->alpha=gamma*pixels[0].alpha;   /* divide by number of pixels */
      break;
    }
    case CatromInterpolatePixel:
    {
      double
        cx[4],
        cy[4];

      p=GetCacheViewVirtualPixels(image_view,x_offset-1,y_offset-1,4,4,
        exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < 16L; i++)
        AlphaBlendPixelInfo(image,p+i*GetPixelChannels(image),pixels+i,alpha+i);
      CatromWeights((double) (x-x_offset),&cx);
      CatromWeights((double) (y-y_offset),&cy);
      pixel->red=(cy[0]*(cx[0]*pixels[0].red+cx[1]*pixels[1].red+cx[2]*
        pixels[2].red+cx[3]*pixels[3].red)+cy[1]*(cx[0]*pixels[4].red+cx[1]*
        pixels[5].red+cx[2]*pixels[6].red+cx[3]*pixels[7].red)+cy[2]*(cx[0]*
        pixels[8].red+cx[1]*pixels[9].red+cx[2]*pixels[10].red+cx[3]*
        pixels[11].red)+cy[3]*(cx[0]*pixels[12].red+cx[1]*pixels[13].red+cx[2]*
        pixels[14].red+cx[3]*pixels[15].red));
      pixel->green=(cy[0]*(cx[0]*pixels[0].green+cx[1]*pixels[1].green+cx[2]*
        pixels[2].green+cx[3]*pixels[3].green)+cy[1]*(cx[0]*pixels[4].green+
        cx[1]*pixels[5].green+cx[2]*pixels[6].green+cx[3]*pixels[7].green)+
        cy[2]*(cx[0]*pixels[8].green+cx[1]*pixels[9].green+cx[2]*
        pixels[10].green+cx[3]*pixels[11].green)+cy[3]*(cx[0]*
        pixels[12].green+cx[1]*pixels[13].green+cx[2]*pixels[14].green+cx[3]*
        pixels[15].green));
      pixel->blue=(cy[0]*(cx[0]*pixels[0].blue+cx[1]*pixels[1].blue+cx[2]*
        pixels[2].blue+cx[3]*pixels[3].blue)+cy[1]*(cx[0]*pixels[4].blue+cx[1]*
        pixels[5].blue+cx[2]*pixels[6].blue+cx[3]*pixels[7].blue)+cy[2]*(cx[0]*
        pixels[8].blue+cx[1]*pixels[9].blue+cx[2]*pixels[10].blue+cx[3]*
        pixels[11].blue)+cy[3]*(cx[0]*pixels[12].blue+cx[1]*pixels[13].blue+
        cx[2]*pixels[14].blue+cx[3]*pixels[15].blue));
      if (image->colorspace == CMYKColorspace)
        pixel->black=(cy[0]*(cx[0]*pixels[0].black+cx[1]*pixels[1].black+cx[2]*
          pixels[2].black+cx[3]*pixels[3].black)+cy[1]*(cx[0]*pixels[4].black+
          cx[1]*pixels[5].black+cx[2]*pixels[6].black+cx[3]*pixels[7].black)+
          cy[2]*(cx[0]*pixels[8].black+cx[1]*pixels[9].black+cx[2]*
          pixels[10].black+cx[3]*pixels[11].black)+cy[3]*(cx[0]*
          pixels[12].black+cx[1]*pixels[13].black+cx[2]*pixels[14].black+cx[3]*
          pixels[15].black));
      pixel->alpha=(cy[0]*(cx[0]*pixels[0].alpha+cx[1]*pixels[1].alpha+cx[2]*
        pixels[2].alpha+cx[3]*pixels[3].alpha)+cy[1]*(cx[0]*pixels[4].alpha+
        cx[1]*pixels[5].alpha+cx[2]*pixels[6].alpha+cx[3]*pixels[7].alpha)+
        cy[2]*(cx[0]*pixels[8].alpha+cx[1]*pixels[9].alpha+cx[2]*
        pixels[10].alpha+cx[3]*pixels[11].alpha)+cy[3]*(cx[0]*pixels[12].alpha+
        cx[1]*pixels[13].alpha+cx[2]*pixels[14].alpha+cx[3]*pixels[15].alpha));
      break;
    }
    case IntegerInterpolatePixel:
    {
      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,1,1,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      GetPixelInfoPixel(image,p,pixel);
      break;
    }
    case MeshInterpolatePixel:
    {
      PointInfo
        delta,
        luminance;

      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      delta.x=x-x_offset;
      delta.y=y-y_offset;
      luminance.x=GetPixelLuma(image,p)-(double)
        GetPixelLuma(image,p+3*GetPixelChannels(image));
      luminance.y=GetPixelLuma(image,p+GetPixelChannels(image))-(double)
        GetPixelLuma(image,p+2*GetPixelChannels(image));
      AlphaBlendPixelInfo(image,p,pixels+0,alpha+0);
      AlphaBlendPixelInfo(image,p+GetPixelChannels(image),pixels+1,alpha+1);
      AlphaBlendPixelInfo(image,p+2*GetPixelChannels(image),pixels+2,alpha+2);
      AlphaBlendPixelInfo(image,p+3*GetPixelChannels(image),pixels+3,alpha+3);
      if (fabs(luminance.x) < fabs(luminance.y))
        {
          /*
            Diagonal 0-3 NW-SE.
          */
          if (delta.x <= delta.y)
            {
              /*
                Bottom-left triangle (pixel: 2, diagonal: 0-3).
              */
              delta.y=1.0-delta.y;
              gamma=MeshInterpolate(&delta,alpha[2],alpha[3],alpha[0]);
              gamma=PerceptibleReciprocal(gamma);
              pixel->red=gamma*MeshInterpolate(&delta,pixels[2].red,
                pixels[3].red,pixels[0].red);
              pixel->green=gamma*MeshInterpolate(&delta,pixels[2].green,
                pixels[3].green,pixels[0].green);
              pixel->blue=gamma*MeshInterpolate(&delta,pixels[2].blue,
                pixels[3].blue,pixels[0].blue);
              if (image->colorspace == CMYKColorspace)
                pixel->black=gamma*MeshInterpolate(&delta,pixels[2].black,
                  pixels[3].black,pixels[0].black);
              gamma=MeshInterpolate(&delta,1.0,1.0,1.0);
              pixel->alpha=gamma*MeshInterpolate(&delta,pixels[2].alpha,
                pixels[3].alpha,pixels[0].alpha);
            }
          else
            {
              /*
                Top-right triangle (pixel:1 , diagonal: 0-3).
              */
              delta.x=1.0-delta.x;
              gamma=MeshInterpolate(&delta,alpha[1],alpha[0],alpha[3]);
              gamma=PerceptibleReciprocal(gamma);
              pixel->red=gamma*MeshInterpolate(&delta,pixels[1].red,
                pixels[0].red,pixels[3].red);
              pixel->green=gamma*MeshInterpolate(&delta,pixels[1].green,
                pixels[0].green,pixels[3].green);
              pixel->blue=gamma*MeshInterpolate(&delta,pixels[1].blue,
                pixels[0].blue,pixels[3].blue);
              if (image->colorspace == CMYKColorspace)
                pixel->black=gamma*MeshInterpolate(&delta,pixels[1].black,
                  pixels[0].black,pixels[3].black);
              gamma=MeshInterpolate(&delta,1.0,1.0,1.0);
              pixel->alpha=gamma*MeshInterpolate(&delta,pixels[1].alpha,
                pixels[0].alpha,pixels[3].alpha);
            }
        }
      else
        {
          /*
            Diagonal 1-2 NE-SW.
          */
          if (delta.x <= (1.0-delta.y))
            {
              /*
                Top-left triangle (pixel: 0, diagonal: 1-2).
              */
              gamma=MeshInterpolate(&delta,alpha[0],alpha[1],alpha[2]);
              gamma=PerceptibleReciprocal(gamma);
              pixel->red=gamma*MeshInterpolate(&delta,pixels[0].red,
                pixels[1].red,pixels[2].red);
              pixel->green=gamma*MeshInterpolate(&delta,pixels[0].green,
                pixels[1].green,pixels[2].green);
              pixel->blue=gamma*MeshInterpolate(&delta,pixels[0].blue,
                pixels[1].blue,pixels[2].blue);
              if (image->colorspace == CMYKColorspace)
                pixel->black=gamma*MeshInterpolate(&delta,pixels[0].black,
                  pixels[1].black,pixels[2].black);
              gamma=MeshInterpolate(&delta,1.0,1.0,1.0);
              pixel->alpha=gamma*MeshInterpolate(&delta,pixels[0].alpha,
                pixels[1].alpha,pixels[2].alpha);
            }
          else
            {
              /*
                Bottom-right triangle (pixel: 3, diagonal: 1-2).
              */
              delta.x=1.0-delta.x;
              delta.y=1.0-delta.y;
              gamma=MeshInterpolate(&delta,alpha[3],alpha[2],alpha[1]);
              gamma=PerceptibleReciprocal(gamma);
              pixel->red=gamma*MeshInterpolate(&delta,pixels[3].red,
                pixels[2].red,pixels[1].red);
              pixel->green=gamma*MeshInterpolate(&delta,pixels[3].green,
                pixels[2].green,pixels[1].green);
              pixel->blue=gamma*MeshInterpolate(&delta,pixels[3].blue,
                pixels[2].blue,pixels[1].blue);
              if (image->colorspace == CMYKColorspace)
                pixel->black=gamma*MeshInterpolate(&delta,pixels[3].black,
                  pixels[2].black,pixels[1].black);
              gamma=MeshInterpolate(&delta,1.0,1.0,1.0);
              pixel->alpha=gamma*MeshInterpolate(&delta,pixels[3].alpha,
                pixels[2].alpha,pixels[1].alpha);
            }
        }
      break;
    }
    case NearestInterpolatePixel:
    {
      x_offset=(ssize_t) floor(x+0.5);
      y_offset=(ssize_t) floor(y+0.5);
      p=GetCacheViewVirtualPixels(image_view,x_offset,y_offset,1,1,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      GetPixelInfoPixel(image,p,pixel);
      break;
    }
    case SplineInterpolatePixel:
    {
      double
        cx[4],
        cy[4];

      p=GetCacheViewVirtualPixels(image_view,x_offset-1,y_offset-1,4,4,
        exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < 16L; i++)
        AlphaBlendPixelInfo(image,p+i*GetPixelChannels(image),pixels+i,alpha+i);
      SplineWeights((double) (x-x_offset),&cx);
      SplineWeights((double) (y-y_offset),&cy);
      pixel->red=(cy[0]*(cx[0]*pixels[0].red+cx[1]*pixels[1].red+cx[2]*
        pixels[2].red+cx[3]*pixels[3].red)+cy[1]*(cx[0]*pixels[4].red+cx[1]*
        pixels[5].red+cx[2]*pixels[6].red+cx[3]*pixels[7].red)+cy[2]*(cx[0]*
        pixels[8].red+cx[1]*pixels[9].red+cx[2]*pixels[10].red+cx[3]*
        pixels[11].red)+cy[3]*(cx[0]*pixels[12].red+cx[1]*pixels[13].red+cx[2]*
        pixels[14].red+cx[3]*pixels[15].red));
      pixel->green=(cy[0]*(cx[0]*pixels[0].green+cx[1]*pixels[1].green+cx[2]*
        pixels[2].green+cx[3]*pixels[3].green)+cy[1]*(cx[0]*pixels[4].green+
        cx[1]*pixels[5].green+cx[2]*pixels[6].green+cx[3]*pixels[7].green)+
        cy[2]*(cx[0]*pixels[8].green+cx[1]*pixels[9].green+cx[2]*
        pixels[10].green+cx[3]*pixels[11].green)+cy[3]*(cx[0]*pixels[12].green+
        cx[1]*pixels[13].green+cx[2]*pixels[14].green+cx[3]*pixels[15].green));
      pixel->blue=(cy[0]*(cx[0]*pixels[0].blue+cx[1]*pixels[1].blue+cx[2]*
        pixels[2].blue+cx[3]*pixels[3].blue)+cy[1]*(cx[0]*pixels[4].blue+cx[1]*
        pixels[5].blue+cx[2]*pixels[6].blue+cx[3]*pixels[7].blue)+cy[2]*(cx[0]*
        pixels[8].blue+cx[1]*pixels[9].blue+cx[2]*pixels[10].blue+cx[3]*
        pixels[11].blue)+cy[3]*(cx[0]*pixels[12].blue+cx[1]*pixels[13].blue+
        cx[2]*pixels[14].blue+cx[3]*pixels[15].blue));
      if (image->colorspace == CMYKColorspace)
        pixel->black=(cy[0]*(cx[0]*pixels[0].black+cx[1]*pixels[1].black+cx[2]*
          pixels[2].black+cx[3]*pixels[3].black)+cy[1]*(cx[0]*pixels[4].black+
          cx[1]*pixels[5].black+cx[2]*pixels[6].black+cx[3]*pixels[7].black)+
          cy[2]*(cx[0]*pixels[8].black+cx[1]*pixels[9].black+cx[2]*
          pixels[10].black+cx[3]*pixels[11].black)+cy[3]*(cx[0]*
          pixels[12].black+cx[1]*pixels[13].black+cx[2]*pixels[14].black+cx[3]*
          pixels[15].black));
      pixel->alpha=(cy[0]*(cx[0]*pixels[0].alpha+cx[1]*pixels[1].alpha+cx[2]*
        pixels[2].alpha+cx[3]*pixels[3].alpha)+cy[1]*(cx[0]*pixels[4].alpha+
        cx[1]*pixels[5].alpha+cx[2]*pixels[6].alpha+cx[3]*pixels[7].alpha)+
        cy[2]*(cx[0]*pixels[8].alpha+cx[1]*pixels[9].alpha+cx[2]*
        pixels[10].alpha+cx[3]*pixels[11].alpha)+cy[3]*(cx[0]*pixels[12].alpha+
        cx[1]*pixels[13].alpha+cx[2]*pixels[14].alpha+cx[3]*pixels[15].alpha));
      break;
    }
  }
  return(status);
}