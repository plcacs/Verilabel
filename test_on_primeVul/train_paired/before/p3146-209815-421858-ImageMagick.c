MagickExport MagickBooleanType InterpolatePixelChannels(
  const Image *magick_restrict source,const CacheView_ *source_view,
  const Image *magick_restrict destination,const PixelInterpolateMethod method,
  const double x,const double y,Quantum *pixel,ExceptionInfo *exception)
{
  MagickBooleanType
    status;

  double
    alpha[16],
    gamma,
    pixels[16];

  register const Quantum
    *magick_restrict p;

  register ssize_t
    i;

  ssize_t
    x_offset,
    y_offset;

  PixelInterpolateMethod
    interpolate;

  assert(source != (Image *) NULL);
  assert(source->signature == MagickCoreSignature);
  assert(source_view != (CacheView *) NULL);
  status=MagickTrue;
  x_offset=(ssize_t) floor(x);
  y_offset=(ssize_t) floor(y);
  interpolate=method;
  if (interpolate == UndefinedInterpolatePixel)
    interpolate=source->interpolate;
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
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,(size_t) count,
        (size_t) count,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      count*=count;  /* Number of pixels to average */
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        double
          sum;

        register ssize_t
          j;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        for (j=0; j < (ssize_t) count; j++)
          pixels[j]=(double) p[j*GetPixelChannels(source)+i];
        sum=0.0;
        if ((traits & BlendPixelTrait) == 0)
          {
            for (j=0; j < (ssize_t) count; j++)
              sum+=pixels[j];
            sum/=count;
            SetPixelChannel(destination,channel,ClampToQuantum(sum),pixel);
            continue;
          }
        for (j=0; j < (ssize_t) count; j++)
        {
          alpha[j]=QuantumScale*GetPixelAlpha(source,p+j*
            GetPixelChannels(source));
          pixels[j]*=alpha[j];
          gamma=PerceptibleReciprocal(alpha[j]);
          sum+=gamma*pixels[j];
        }
        sum/=count;
        SetPixelChannel(destination,channel,ClampToQuantum(sum),pixel);
      }
      break;
    }
    case BilinearInterpolatePixel:
    default:
    {
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        PointInfo
          delta,
          epsilon;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        delta.x=x-x_offset;
        delta.y=y-y_offset;
        epsilon.x=1.0-delta.x;
        epsilon.y=1.0-delta.y;
        pixels[0]=(double) p[i];
        pixels[1]=(double) p[GetPixelChannels(source)+i];
        pixels[2]=(double) p[2*GetPixelChannels(source)+i];
        pixels[3]=(double) p[3*GetPixelChannels(source)+i];
        if ((traits & BlendPixelTrait) == 0)
          {
            gamma=((epsilon.y*(epsilon.x+delta.x)+delta.y*(epsilon.x+delta.x)));
            gamma=PerceptibleReciprocal(gamma);
            SetPixelChannel(destination,channel,ClampToQuantum(gamma*(epsilon.y*
              (epsilon.x*pixels[0]+delta.x*pixels[1])+delta.y*(epsilon.x*
              pixels[2]+delta.x*pixels[3]))),pixel);
            continue;
          }
        alpha[0]=QuantumScale*GetPixelAlpha(source,p);
        alpha[1]=QuantumScale*GetPixelAlpha(source,p+GetPixelChannels(source));
        alpha[2]=QuantumScale*GetPixelAlpha(source,p+2*
          GetPixelChannels(source));
        alpha[3]=QuantumScale*GetPixelAlpha(source,p+3*
          GetPixelChannels(source));
        pixels[0]*=alpha[0];
        pixels[1]*=alpha[1];
        pixels[2]*=alpha[2];
        pixels[3]*=alpha[3];
        gamma=((epsilon.y*(epsilon.x*alpha[0]+delta.x*alpha[1])+delta.y*
          (epsilon.x*alpha[2]+delta.x*alpha[3])));
        gamma=PerceptibleReciprocal(gamma);
        SetPixelChannel(destination,channel,ClampToQuantum(gamma*(epsilon.y*
          (epsilon.x*pixels[0]+delta.x*pixels[1])+delta.y*(epsilon.x*pixels[2]+
          delta.x*pixels[3]))),pixel);
      }
      break;
    }
    case BlendInterpolatePixel:
    {
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        register ssize_t
          j;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        if (source->alpha_trait != BlendPixelTrait)
          for (j=0; j < 4; j++)
          {
            alpha[j]=1.0;
            pixels[j]=(double) p[j*GetPixelChannels(source)+i];
          }
        else
          for (j=0; j < 4; j++)
          {
            alpha[j]=QuantumScale*GetPixelAlpha(source,p+j*
              GetPixelChannels(source));
            pixels[j]=(double) p[j*GetPixelChannels(source)+i];
            if (channel != AlphaPixelChannel)
              pixels[j]*=alpha[j];
          }
        gamma=1.0;  /* number of pixels blended together (its variable) */
        for (j=0; j <= 1L; j++)
        {
          if ((y-y_offset) >= 0.75)
            {
              alpha[j]=alpha[j+2];  /* take right pixels */
              pixels[j]=pixels[j+2];
            }
          else
            if ((y-y_offset) > 0.25)
              {
                gamma=2.0;  /* blend both pixels in row */
                alpha[j]+=alpha[j+2];  /* add up alpha weights */
                pixels[j]+=pixels[j+2];
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
        SetPixelChannel(destination,channel,ClampToQuantum(gamma*pixels[0]),
          pixel);
      }
      break;
    }
    case CatromInterpolatePixel:
    {
      double
        cx[4],
        cy[4];

      p=GetCacheViewVirtualPixels(source_view,x_offset-1,y_offset-1,4,4,
        exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        register ssize_t
          j;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        if ((traits & BlendPixelTrait) == 0)
          for (j=0; j < 16; j++)
          {
            alpha[j]=1.0;
            pixels[j]=(double) p[j*GetPixelChannels(source)+i];
          }
        else
          for (j=0; j < 16; j++)
          {
            alpha[j]=QuantumScale*GetPixelAlpha(source,p+j*
              GetPixelChannels(source));
            pixels[j]=alpha[j]*p[j*GetPixelChannels(source)+i];
          }
        CatromWeights((double) (x-x_offset),&cx);
        CatromWeights((double) (y-y_offset),&cy);
        gamma=((traits & BlendPixelTrait) ? (double) (1.0) :
          PerceptibleReciprocal(cy[0]*(cx[0]*alpha[0]+cx[1]*alpha[1]+cx[2]*
          alpha[2]+cx[3]*alpha[3])+cy[1]*(cx[0]*alpha[4]+cx[1]*alpha[5]+cx[2]*
          alpha[6]+cx[3]*alpha[7])+cy[2]*(cx[0]*alpha[8]+cx[1]*alpha[9]+cx[2]*
          alpha[10]+cx[3]*alpha[11])+cy[3]*(cx[0]*alpha[12]+cx[1]*alpha[13]+
          cx[2]*alpha[14]+cx[3]*alpha[15])));
        SetPixelChannel(destination,channel,ClampToQuantum(gamma*(cy[0]*(cx[0]*
          pixels[0]+cx[1]*pixels[1]+cx[2]*pixels[2]+cx[3]*pixels[3])+cy[1]*
          (cx[0]*pixels[4]+cx[1]*pixels[5]+cx[2]*pixels[6]+cx[3]*pixels[7])+
          cy[2]*(cx[0]*pixels[8]+cx[1]*pixels[9]+cx[2]*pixels[10]+cx[3]*
          pixels[11])+cy[3]*(cx[0]*pixels[12]+cx[1]*pixels[13]+cx[2]*
          pixels[14]+cx[3]*pixels[15]))),pixel);
      }
      break;
    }
    case IntegerInterpolatePixel:
    {
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,1,1,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        SetPixelChannel(destination,channel,p[i],pixel);
      }
      break;
    }
    case NearestInterpolatePixel:
    {
      x_offset=(ssize_t) floor(x+0.5);
      y_offset=(ssize_t) floor(y+0.5);
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,1,1,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        SetPixelChannel(destination,channel,p[i],pixel);
      }
      break;
    }
    case MeshInterpolatePixel:
    {
      p=GetCacheViewVirtualPixels(source_view,x_offset,y_offset,2,2,exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        PointInfo
          delta,
          luminance;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        pixels[0]=(double) p[i];
        pixels[1]=(double) p[GetPixelChannels(source)+i];
        pixels[2]=(double) p[2*GetPixelChannels(source)+i];
        pixels[3]=(double) p[3*GetPixelChannels(source)+i];
        if ((traits & BlendPixelTrait) == 0)
          {
            alpha[0]=1.0;
            alpha[1]=1.0;
            alpha[2]=1.0;
            alpha[3]=1.0;
          }
        else
          {
            alpha[0]=QuantumScale*GetPixelAlpha(source,p);
            alpha[1]=QuantumScale*GetPixelAlpha(source,p+
              GetPixelChannels(source));
            alpha[2]=QuantumScale*GetPixelAlpha(source,p+2*
              GetPixelChannels(source));
            alpha[3]=QuantumScale*GetPixelAlpha(source,p+3*
              GetPixelChannels(source));
          }
        delta.x=x-x_offset;
        delta.y=y-y_offset;
        luminance.x=fabs((double) (GetPixelLuma(source,p)-
          GetPixelLuma(source,p+3*GetPixelChannels(source))));
        luminance.y=fabs((double) (GetPixelLuma(source,p+
          GetPixelChannels(source))-GetPixelLuma(source,p+2*
          GetPixelChannels(source))));
        if (luminance.x < luminance.y)
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
                SetPixelChannel(destination,channel,ClampToQuantum(gamma*
                  MeshInterpolate(&delta,pixels[2],pixels[3],pixels[0])),pixel);
              }
            else
              {
                /*
                  Top-right triangle (pixel: 1, diagonal: 0-3).
                */
                delta.x=1.0-delta.x;
                gamma=MeshInterpolate(&delta,alpha[1],alpha[0],alpha[3]);
                gamma=PerceptibleReciprocal(gamma);
                SetPixelChannel(destination,channel,ClampToQuantum(gamma*
                  MeshInterpolate(&delta,pixels[1],pixels[0],pixels[3])),pixel);
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
                SetPixelChannel(destination,channel,ClampToQuantum(gamma*
                  MeshInterpolate(&delta,pixels[0],pixels[1],pixels[2])),pixel);
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
                SetPixelChannel(destination,channel,ClampToQuantum(gamma*
                  MeshInterpolate(&delta,pixels[3],pixels[2],pixels[1])),pixel);
              }
          }
      }
      break;
    }
    case SplineInterpolatePixel:
    {
      double
        cx[4],
        cy[4];

      p=GetCacheViewVirtualPixels(source_view,x_offset-1,y_offset-1,4,4,
        exception);
      if (p == (const Quantum *) NULL)
        {
          status=MagickFalse;
          break;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(source); i++)
      {
        register ssize_t
          j;

        PixelChannel channel = GetPixelChannelChannel(source,i);
        PixelTrait traits = GetPixelChannelTraits(source,channel);
        PixelTrait destination_traits=GetPixelChannelTraits(destination,
          channel);
        if ((traits == UndefinedPixelTrait) ||
            (destination_traits == UndefinedPixelTrait))
          continue;
        if ((traits & BlendPixelTrait) == 0)
          for (j=0; j < 16; j++)
          {
            alpha[j]=1.0;
            pixels[j]=(double) p[j*GetPixelChannels(source)+i];
          }
        else
          for (j=0; j < 16; j++)
          {
            alpha[j]=QuantumScale*GetPixelAlpha(source,p+j*
              GetPixelChannels(source));
            pixels[j]=alpha[j]*p[j*GetPixelChannels(source)+i];
          }
        SplineWeights((double) (x-x_offset),&cx);
        SplineWeights((double) (y-y_offset),&cy);
        gamma=((traits & BlendPixelTrait) ? (double) (1.0) :
          PerceptibleReciprocal(cy[0]*(cx[0]*alpha[0]+cx[1]*alpha[1]+cx[2]*
          alpha[2]+cx[3]*alpha[3])+cy[1]*(cx[0]*alpha[4]+cx[1]*alpha[5]+cx[2]*
          alpha[6]+cx[3]*alpha[7])+cy[2]*(cx[0]*alpha[8]+cx[1]*alpha[9]+cx[2]*
          alpha[10]+cx[3]*alpha[11])+cy[3]*(cx[0]*alpha[12]+cx[1]*alpha[13]+
          cx[2]*alpha[14]+cx[3]*alpha[15])));
        SetPixelChannel(destination,channel,ClampToQuantum(gamma*(cy[0]*(cx[0]*
          pixels[0]+cx[1]*pixels[1]+cx[2]*pixels[2]+cx[3]*pixels[3])+cy[1]*
          (cx[0]*pixels[4]+cx[1]*pixels[5]+cx[2]*pixels[6]+cx[3]*pixels[7])+
          cy[2]*(cx[0]*pixels[8]+cx[1]*pixels[9]+cx[2]*pixels[10]+cx[3]*
          pixels[11])+cy[3]*(cx[0]*pixels[12]+cx[1]*pixels[13]+cx[2]*
          pixels[14]+cx[3]*pixels[15]))),pixel);
      }
      break;
    }
  }
  return(status);
}