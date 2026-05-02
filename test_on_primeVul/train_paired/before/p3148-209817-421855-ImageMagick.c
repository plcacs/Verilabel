MagickExport MagickBooleanType InterpolatePixelChannel(
  const Image *magick_restrict image,const CacheView_ *image_view,
  const PixelChannel channel,const PixelInterpolateMethod method,
  const double x,const double y,double *pixel,ExceptionInfo *exception)
{
  double
    alpha[16],
    gamma,
    pixels[16];

  MagickBooleanType
    status;

  PixelInterpolateMethod
    interpolate;

  PixelTrait
    traits;

  register const Quantum
    *magick_restrict p;

  register ssize_t
    i;

  ssize_t
    x_offset,
    y_offset;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(image_view != (CacheView *) NULL);
  status=MagickTrue;
  *pixel=0.0;
  traits=GetPixelChannelTraits(image,channel);
  x_offset=(ssize_t) floor(x);
  y_offset=(ssize_t) floor(y);
  interpolate=method;
  if (interpolate == UndefinedInterpolatePixel)
    interpolate=image->interpolate;
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
      else
        if (interpolate == Average16InterpolatePixel)
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
      count*=count;  /* Number of pixels to average */
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < (ssize_t) count; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(double) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < (ssize_t) count; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      for (i=0; i < (ssize_t) count; i++)
      {
        gamma=PerceptibleReciprocal(alpha[i])/count;
        *pixel+=gamma*pixels[i];
      }
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
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < 4; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(double) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < 4; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      delta.x=x-x_offset;
      delta.y=y-y_offset;
      epsilon.x=1.0-delta.x;
      epsilon.y=1.0-delta.y;
      gamma=((epsilon.y*(epsilon.x*alpha[0]+delta.x*alpha[1])+delta.y*
        (epsilon.x*alpha[2]+delta.x*alpha[3])));
      gamma=PerceptibleReciprocal(gamma);
      *pixel=gamma*(epsilon.y*(epsilon.x*pixels[0]+delta.x*pixels[1])+delta.y*
        (epsilon.x*pixels[2]+delta.x*pixels[3]));
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
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < 4; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(MagickRealType) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < 4; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      gamma=1.0;  /* number of pixels blended together (its variable) */
      for (i=0; i <= 1L; i++) {
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
              pixels[i]+=pixels[i+2];
            }
      }
      if ((x-x_offset) >= 0.75)
        {
          alpha[0]=alpha[1];  /* take bottom row blend */
          pixels[0]=pixels[1];
        }
      else
        if ((x-x_offset) > 0.25)
          {
            gamma*=2.0;  /* blend both rows */
            alpha[0]+=alpha[1];  /* add up alpha weights */
            pixels[0]+=pixels[1];
          }
      if (channel != AlphaPixelChannel)
        gamma=PerceptibleReciprocal(alpha[0]);  /* (color) 1/alpha_weights */
      else
        gamma=PerceptibleReciprocal(gamma);  /* (alpha) 1/number_of_pixels */
      *pixel=gamma*pixels[0];
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
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < 16; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(double) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < 16; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      CatromWeights((double) (x-x_offset),&cx);
      CatromWeights((double) (y-y_offset),&cy);
      gamma=(channel == AlphaPixelChannel ? (double) 1.0 :
        PerceptibleReciprocal(cy[0]*(cx[0]*alpha[0]+cx[1]*alpha[1]+cx[2]*
        alpha[2]+cx[3]*alpha[3])+cy[1]*(cx[0]*alpha[4]+cx[1]*alpha[5]+cx[2]*
        alpha[6]+cx[3]*alpha[7])+cy[2]*(cx[0]*alpha[8]+cx[1]*alpha[9]+cx[2]*
        alpha[10]+cx[3]*alpha[11])+cy[3]*(cx[0]*alpha[12]+cx[1]*alpha[13]+
        cx[2]*alpha[14]+cx[3]*alpha[15])));
      *pixel=gamma*(cy[0]*(cx[0]*pixels[0]+cx[1]*pixels[1]+cx[2]*pixels[2]+
        cx[3]*pixels[3])+cy[1]*(cx[0]*pixels[4]+cx[1]*pixels[5]+cx[2]*
        pixels[6]+cx[3]*pixels[7])+cy[2]*(cx[0]*pixels[8]+cx[1]*pixels[9]+
        cx[2]*pixels[10]+cx[3]*pixels[11])+cy[3]*(cx[0]*pixels[12]+cx[1]*
        pixels[13]+cx[2]*pixels[14]+cx[3]*pixels[15]));
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
      *pixel=(double) GetPixelChannel(image,channel,p);
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
      *pixel=(double) GetPixelChannel(image,channel,p);
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
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < 4; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(double) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < 4; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      delta.x=x-x_offset;
      delta.y=y-y_offset;
      luminance.x=GetPixelLuma(image,p)-(double)
        GetPixelLuma(image,p+3*GetPixelChannels(image));
      luminance.y=GetPixelLuma(image,p+GetPixelChannels(image))-(double)
        GetPixelLuma(image,p+2*GetPixelChannels(image));
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
              *pixel=gamma*MeshInterpolate(&delta,pixels[2],pixels[3],
                pixels[0]);
            }
          else
            {
              /*
                Top-right triangle (pixel: 1, diagonal: 0-3).
              */
              delta.x=1.0-delta.x;
              gamma=MeshInterpolate(&delta,alpha[1],alpha[0],alpha[3]);
              gamma=PerceptibleReciprocal(gamma);
              *pixel=gamma*MeshInterpolate(&delta,pixels[1],pixels[0],
                pixels[3]);
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
              *pixel=gamma*MeshInterpolate(&delta,pixels[0],pixels[1],
                pixels[2]);
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
              *pixel=gamma*MeshInterpolate(&delta,pixels[3],pixels[2],
                pixels[1]);
            }
        }
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
      if ((traits & BlendPixelTrait) == 0)
        for (i=0; i < 16; i++)
        {
          alpha[i]=1.0;
          pixels[i]=(double) p[i*GetPixelChannels(image)+channel];
        }
      else
        for (i=0; i < 16; i++)
        {
          alpha[i]=QuantumScale*GetPixelAlpha(image,p+i*
            GetPixelChannels(image));
          pixels[i]=alpha[i]*p[i*GetPixelChannels(image)+channel];
        }
      SplineWeights((double) (x-x_offset),&cx);
      SplineWeights((double) (y-y_offset),&cy);
      gamma=(channel == AlphaPixelChannel ? (double) 1.0 :
        PerceptibleReciprocal(cy[0]*(cx[0]*alpha[0]+cx[1]*alpha[1]+cx[2]*
        alpha[2]+cx[3]*alpha[3])+cy[1]*(cx[0]*alpha[4]+cx[1]*alpha[5]+cx[2]*
        alpha[6]+cx[3]*alpha[7])+cy[2]*(cx[0]*alpha[8]+cx[1]*alpha[9]+cx[2]*
        alpha[10]+cx[3]*alpha[11])+cy[3]*(cx[0]*alpha[12]+cx[1]*alpha[13]+
        cx[2]*alpha[14]+cx[3]*alpha[15])));
      *pixel=gamma*(cy[0]*(cx[0]*pixels[0]+cx[1]*pixels[1]+cx[2]*pixels[2]+
        cx[3]*pixels[3])+cy[1]*(cx[0]*pixels[4]+cx[1]*pixels[5]+cx[2]*
        pixels[6]+cx[3]*pixels[7])+cy[2]*(cx[0]*pixels[8]+cx[1]*pixels[9]+
        cx[2]*pixels[10]+cx[3]*pixels[11])+cy[3]*(cx[0]*pixels[12]+cx[1]*
        pixels[13]+cx[2]*pixels[14]+cx[3]*pixels[15]));
      break;
    }
  }
  return(status);
}