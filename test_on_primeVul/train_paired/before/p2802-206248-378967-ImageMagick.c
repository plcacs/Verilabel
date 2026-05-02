MagickPrivate ResizeFilter *AcquireResizeFilter(const Image *image,
  const FilterType filter,const MagickBooleanType cylindrical,
  ExceptionInfo *exception)
{
  const char
    *artifact;

  FilterType
    filter_type,
    window_type;

  double
    B,
    C,
    value;

  register ResizeFilter
    *resize_filter;

  /*
    Table Mapping given Filter, into Weighting and Windowing functions.
    A 'Box' windowing function means its a simble non-windowed filter.
    An 'SincFast' filter function could be upgraded to a 'Jinc' filter if a
    "cylindrical" is requested, unless a 'Sinc' or 'SincFast' filter was
    specifically requested by the user.

    WARNING: The order of this table must match the order of the FilterType
    enumeration specified in "resample.h", or the filter names will not match
    the filter being setup.

    You can check filter setups with the "filter:verbose" expert setting.
  */
  static struct
  {
    FilterType
      filter,
      window;
  } const mapping[SentinelFilter] =
  {
    { UndefinedFilter,     BoxFilter      },  /* Undefined (default to Box)   */
    { PointFilter,         BoxFilter      },  /* SPECIAL: Nearest neighbour   */
    { BoxFilter,           BoxFilter      },  /* Box averaging filter         */
    { TriangleFilter,      BoxFilter      },  /* Linear interpolation filter  */
    { HermiteFilter,       BoxFilter      },  /* Hermite interpolation filter */
    { SincFastFilter,      HannFilter     },  /* Hann -- cosine-sinc          */
    { SincFastFilter,      HammingFilter  },  /* Hamming --   '' variation    */
    { SincFastFilter,      BlackmanFilter },  /* Blackman -- 2*cosine-sinc    */
    { GaussianFilter,      BoxFilter      },  /* Gaussian blur filter         */
    { QuadraticFilter,     BoxFilter      },  /* Quadratic Gaussian approx    */
    { CubicFilter,         BoxFilter      },  /* General Cubic Filter, Spline */
    { CatromFilter,        BoxFilter      },  /* Cubic-Keys interpolator      */
    { MitchellFilter,      BoxFilter      },  /* 'Ideal' Cubic-Keys filter    */
    { JincFilter,          BoxFilter      },  /* Raw 3-lobed Jinc function    */
    { SincFilter,          BoxFilter      },  /* Raw 4-lobed Sinc function    */
    { SincFastFilter,      BoxFilter      },  /* Raw fast sinc ("Pade"-type)  */
    { SincFastFilter,      KaiserFilter   },  /* Kaiser -- square root-sinc   */
    { LanczosFilter,       WelchFilter    },  /* Welch -- parabolic (3 lobe)  */
    { SincFastFilter,      CubicFilter    },  /* Parzen -- cubic-sinc         */
    { SincFastFilter,      BohmanFilter   },  /* Bohman -- 2*cosine-sinc      */
    { SincFastFilter,      TriangleFilter },  /* Bartlett -- triangle-sinc    */
    { LagrangeFilter,      BoxFilter      },  /* Lagrange self-windowing      */
    { LanczosFilter,       LanczosFilter  },  /* Lanczos Sinc-Sinc filters    */
    { LanczosSharpFilter,  LanczosSharpFilter }, /* | these require */
    { Lanczos2Filter,      Lanczos2Filter },     /* | special handling */
    { Lanczos2SharpFilter, Lanczos2SharpFilter },
    { RobidouxFilter,      BoxFilter      },  /* Cubic Keys tuned for EWA     */
    { RobidouxSharpFilter, BoxFilter      },  /* Sharper Cubic Keys for EWA   */
    { LanczosFilter,       CosineFilter   },  /* Cosine window (3 lobes)      */
    { SplineFilter,        BoxFilter      },  /* Spline Cubic Filter          */
    { LanczosRadiusFilter, LanczosFilter  },  /* Lanczos with integer radius  */
    { CubicSplineFilter,   BoxFilter      },  /* CubicSpline (2/3/4 lobes)    */
  };
  /*
    Table mapping the filter/window from the above table to an actual function.
    The default support size for that filter as a weighting function, the range
    to scale with to use that function as a sinc windowing function, (typ 1.0).

    Note that the filter_type -> function is 1 to 1 except for Sinc(),
    SincFast(), and CubicBC() functions, which may have multiple filter to
    function associations.

    See "filter:verbose" handling below for the function -> filter mapping.
  */
  static struct
  {
    double
      (*function)(const double,const ResizeFilter*),
      support, /* Default lobes/support size of the weighting filter. */
      scale,   /* Support when function used as a windowing function
                 Typically equal to the location of the first zero crossing. */
      B,C;     /* BC-spline coefficients, ignored if not a CubicBC filter. */
    ResizeWeightingFunctionType weightingFunctionType;
  } const filters[SentinelFilter] =
  {
    /*            .---  support window (if used as a Weighting Function)
                  |    .--- first crossing (if used as a Windowing Function)
                  |    |    .--- B value for Cubic Function
                  |    |    |    .---- C value for Cubic Function
                  |    |    |    |                                    */
    { Box,       0.5, 0.5, 0.0, 0.0, BoxWeightingFunction },      /* Undefined (default to Box)  */
    { Box,       0.0, 0.5, 0.0, 0.0, BoxWeightingFunction },      /* Point (special handling)    */
    { Box,       0.5, 0.5, 0.0, 0.0, BoxWeightingFunction },      /* Box                         */
    { Triangle,  1.0, 1.0, 0.0, 0.0, TriangleWeightingFunction }, /* Triangle                    */
    { CubicBC,   1.0, 1.0, 0.0, 0.0, CubicBCWeightingFunction },  /* Hermite (cubic  B=C=0)      */
    { Hann,      1.0, 1.0, 0.0, 0.0, HannWeightingFunction },     /* Hann, cosine window         */
    { Hamming,   1.0, 1.0, 0.0, 0.0, HammingWeightingFunction },  /* Hamming, '' variation       */
    { Blackman,  1.0, 1.0, 0.0, 0.0, BlackmanWeightingFunction }, /* Blackman, 2*cosine window   */
    { Gaussian,  2.0, 1.5, 0.0, 0.0, GaussianWeightingFunction }, /* Gaussian                    */
    { Quadratic, 1.5, 1.5, 0.0, 0.0, QuadraticWeightingFunction },/* Quadratic gaussian          */
    { CubicBC,   2.0, 2.0, 1.0, 0.0, CubicBCWeightingFunction },  /* General Cubic Filter        */
    { CubicBC,   2.0, 1.0, 0.0, 0.5, CubicBCWeightingFunction },  /* Catmull-Rom    (B=0,C=1/2)  */
    { CubicBC,   2.0, 8.0/7.0, 1./3., 1./3., CubicBCWeightingFunction }, /* Mitchell   (B=C=1/3)    */
    { Jinc,      3.0, 1.2196698912665045, 0.0, 0.0, JincWeightingFunction }, /* Raw 3-lobed Jinc */
    { Sinc,      4.0, 1.0, 0.0, 0.0, SincWeightingFunction },     /* Raw 4-lobed Sinc            */
    { SincFast,  4.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Raw fast sinc ("Pade"-type) */
    { Kaiser,    1.0, 1.0, 0.0, 0.0, KaiserWeightingFunction },   /* Kaiser (square root window) */
    { Welch,     1.0, 1.0, 0.0, 0.0, WelchWeightingFunction },    /* Welch (parabolic window)    */
    { CubicBC,   2.0, 2.0, 1.0, 0.0, CubicBCWeightingFunction },  /* Parzen (B-Spline window)    */
    { Bohman,    1.0, 1.0, 0.0, 0.0, BohmanWeightingFunction },   /* Bohman, 2*Cosine window     */
    { Triangle,  1.0, 1.0, 0.0, 0.0, TriangleWeightingFunction }, /* Bartlett (triangle window)  */
    { Lagrange,  2.0, 1.0, 0.0, 0.0, LagrangeWeightingFunction }, /* Lagrange sinc approximation */
    { SincFast,  3.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Lanczos, 3-lobed Sinc-Sinc  */
    { SincFast,  3.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Lanczos, Sharpened          */
    { SincFast,  2.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Lanczos, 2-lobed            */
    { SincFast,  2.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Lanczos2, sharpened         */
    /* Robidoux: Keys cubic close to Lanczos2D sharpened */
    { CubicBC,   2.0, 1.1685777620836932,
                            0.37821575509399867, 0.31089212245300067, CubicBCWeightingFunction },
    /* RobidouxSharp: Sharper version of Robidoux */
    { CubicBC,   2.0, 1.105822933719019,
                            0.2620145123990142,  0.3689927438004929, CubicBCWeightingFunction },
    { Cosine,    1.0, 1.0, 0.0, 0.0, CosineWeightingFunction },   /* Low level cosine window     */
    { CubicBC,   2.0, 2.0, 1.0, 0.0, CubicBCWeightingFunction },  /* Cubic B-Spline (B=1,C=0)    */
    { SincFast,  3.0, 1.0, 0.0, 0.0, SincFastWeightingFunction }, /* Lanczos, Interger Radius    */
    { CubicSpline,2.0, 0.5, 0.0, 0.0, BoxWeightingFunction },  /* Spline Lobes 2-lobed */
  };
  /*
    The known zero crossings of the Jinc() or more accurately the Jinc(x*PI)
    function being used as a filter. It is used by the "filter:lobes" expert
    setting and for 'lobes' for Jinc functions in the previous table. This way
    users do not have to deal with the highly irrational lobe sizes of the Jinc
    filter.

    Values taken from
    http://cose.math.bas.bg/webMathematica/webComputing/BesselZeros.jsp
    using Jv-function with v=1, then dividing by PI.
  */
  static double
    jinc_zeros[16] =
    {
      1.2196698912665045,
      2.2331305943815286,
      3.2383154841662362,
      4.2410628637960699,
      5.2427643768701817,
      6.2439216898644877,
      7.2447598687199570,
      8.2453949139520427,
      9.2458926849494673,
      10.246293348754916,
      11.246622794877883,
      12.246898461138105,
      13.247132522181061,
      14.247333735806849,
      15.247508563037300,
      16.247661874700962
   };

  /*
    Allocate resize filter.
  */
  assert(image != (const Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(UndefinedFilter < filter && filter < SentinelFilter);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  (void) exception;
  resize_filter=(ResizeFilter *) AcquireCriticalMemory(sizeof(*resize_filter));
  (void) memset(resize_filter,0,sizeof(*resize_filter));
  /*
    Defaults for the requested filter.
  */
  filter_type=mapping[filter].filter;
  window_type=mapping[filter].window;
  resize_filter->blur=1.0;
  /* Promote 1D Windowed Sinc Filters to a 2D Windowed Jinc filters */
  if ((cylindrical != MagickFalse) && (filter_type == SincFastFilter) &&
      (filter != SincFastFilter))
    filter_type=JincFilter;  /* 1D Windowed Sinc => 2D Windowed Jinc filters */

  /* Expert filter setting override */
  artifact=GetImageArtifact(image,"filter:filter");
  if (IsStringTrue(artifact) != MagickFalse)
    {
      ssize_t
        option;

      option=ParseCommandOption(MagickFilterOptions,MagickFalse,artifact);
      if ((UndefinedFilter < option) && (option < SentinelFilter))
        { /* Raw filter request - no window function. */
          filter_type=(FilterType) option;
          window_type=BoxFilter;
        }
      /* Filter override with a specific window function. */
      artifact=GetImageArtifact(image,"filter:window");
      if (artifact != (const char *) NULL)
        {
          option=ParseCommandOption(MagickFilterOptions,MagickFalse,artifact);
          if ((UndefinedFilter < option) && (option < SentinelFilter))
            window_type=(FilterType) option;
        }
    }
  else
    {
      /* Window specified, but no filter function?  Assume Sinc/Jinc. */
      artifact=GetImageArtifact(image,"filter:window");
      if (artifact != (const char *) NULL)
        {
          ssize_t
            option;

          option=ParseCommandOption(MagickFilterOptions,MagickFalse,artifact);
          if ((UndefinedFilter < option) && (option < SentinelFilter))
            {
              filter_type= cylindrical != MagickFalse ? JincFilter
                                                     : SincFastFilter;
              window_type=(FilterType) option;
            }
        }
    }

  /* Assign the real functions to use for the filters selected. */
  resize_filter->filter=filters[filter_type].function;
  resize_filter->support=filters[filter_type].support;
  resize_filter->filterWeightingType=filters[filter_type].weightingFunctionType;
  resize_filter->window=filters[window_type].function;
  resize_filter->windowWeightingType=filters[window_type].weightingFunctionType;
  resize_filter->scale=filters[window_type].scale;
  resize_filter->signature=MagickCoreSignature;

  /* Filter Modifications for orthogonal/cylindrical usage */
  if (cylindrical != MagickFalse)
    switch (filter_type)
    {
      case BoxFilter:
        /* Support for Cylindrical Box should be sqrt(2)/2 */
        resize_filter->support=(double) MagickSQ1_2;
        break;
      case LanczosFilter:
      case LanczosSharpFilter:
      case Lanczos2Filter:
      case Lanczos2SharpFilter:
      case LanczosRadiusFilter:
        resize_filter->filter=filters[JincFilter].function;
        resize_filter->window=filters[JincFilter].function;
        resize_filter->scale=filters[JincFilter].scale;
        /* number of lobes (support window size) remain unchanged */
        break;
      default:
        break;
    }
  /* Global Sharpening (regardless of orthoginal/cylindrical) */
  switch (filter_type)
  {
    case LanczosSharpFilter:
      resize_filter->blur *= 0.9812505644269356;
      break;
    case Lanczos2SharpFilter:
      resize_filter->blur *= 0.9549963639785485;
      break;
    /* case LanczosRadius:  blur adjust is done after lobes */
    default:
      break;
  }

  /*
    Expert Option Modifications.
  */

  /* User Gaussian Sigma Override - no support change */
  if ((resize_filter->filter == Gaussian) ||
      (resize_filter->window == Gaussian) ) {
    value=0.5;    /* guassian sigma default, half pixel */
    artifact=GetImageArtifact(image,"filter:sigma");
    if (artifact != (const char *) NULL)
      value=StringToDouble(artifact,(char **) NULL);
    /* Define coefficents for Gaussian */
    resize_filter->coefficient[0]=value;                 /* note sigma too */
    resize_filter->coefficient[1]=PerceptibleReciprocal(2.0*value*value); /* sigma scaling */
    resize_filter->coefficient[2]=PerceptibleReciprocal(Magick2PI*value*value);
       /* normalization - not actually needed or used! */
    if ( value > 0.5 )
      resize_filter->support *= 2*value;  /* increase support linearly */
  }

  /* User Kaiser Alpha Override - no support change */
  if ((resize_filter->filter == Kaiser) ||
      (resize_filter->window == Kaiser) ) {
    value=6.5; /* default beta value for Kaiser bessel windowing function */
    artifact=GetImageArtifact(image,"filter:alpha");  /* FUTURE: depreciate */
    if (artifact != (const char *) NULL)
      value=StringToDouble(artifact,(char **) NULL);
    artifact=GetImageArtifact(image,"filter:kaiser-beta");
    if (artifact != (const char *) NULL)
      value=StringToDouble(artifact,(char **) NULL);
    artifact=GetImageArtifact(image,"filter:kaiser-alpha");
    if (artifact != (const char *) NULL)
      value=StringToDouble(artifact,(char **) NULL)*MagickPI;
    /* Define coefficents for Kaiser Windowing Function */
    resize_filter->coefficient[0]=value;         /* alpha */
    resize_filter->coefficient[1]=PerceptibleReciprocal(I0(value));
      /* normalization */
  }

  /* Support Overrides */
  artifact=GetImageArtifact(image,"filter:lobes");
  if (artifact != (const char *) NULL)
    {
      ssize_t
        lobes;

      lobes=(ssize_t) StringToLong(artifact);
      if (lobes < 1)
        lobes=1;
      resize_filter->support=(double) lobes;
    }
  if (resize_filter->filter == Jinc)
    {
      /*
        Convert a Jinc function lobes value to a real support value.
      */
      if (resize_filter->support > 16)
        resize_filter->support=jinc_zeros[15];  /* largest entry in table */
      else
        resize_filter->support=jinc_zeros[((long) resize_filter->support)-1];
      /*
        Blur this filter so support is a integer value (lobes dependant).
      */
      if (filter_type == LanczosRadiusFilter)
        resize_filter->blur*=floor(resize_filter->support)/
          resize_filter->support;
    }
  /*
    Expert blur override.
  */
  artifact=GetImageArtifact(image,"filter:blur");
  if (artifact != (const char *) NULL)
    resize_filter->blur*=StringToDouble(artifact,(char **) NULL);
  if (resize_filter->blur < MagickEpsilon)
    resize_filter->blur=(double) MagickEpsilon;
  /*
    Expert override of the support setting.
  */
  artifact=GetImageArtifact(image,"filter:support");
  if (artifact != (const char *) NULL)
    resize_filter->support=fabs(StringToDouble(artifact,(char **) NULL));
  /*
    Scale windowing function separately to the support 'clipping' window
    that calling operator is planning to actually use. (Expert override)
  */
  resize_filter->window_support=resize_filter->support; /* default */
  artifact=GetImageArtifact(image,"filter:win-support");
  if (artifact != (const char *) NULL)
    resize_filter->window_support=fabs(StringToDouble(artifact,(char **) NULL));
  /*
    Adjust window function scaling to match windowing support for weighting
    function.  This avoids a division on every filter call.
  */
  resize_filter->scale/=resize_filter->window_support;
  /*
   * Set Cubic Spline B,C values, calculate Cubic coefficients.
  */
  B=0.0;
  C=0.0;
  if ((resize_filter->filter == CubicBC) ||
      (resize_filter->window == CubicBC) )
    {
      B=filters[filter_type].B;
      C=filters[filter_type].C;
      if (filters[window_type].function == CubicBC)
        {
          B=filters[window_type].B;
          C=filters[window_type].C;
        }
      artifact=GetImageArtifact(image,"filter:b");
      if (artifact != (const char *) NULL)
        {
          B=StringToDouble(artifact,(char **) NULL);
          C=(1.0-B)/2.0; /* Calculate C to get a Keys cubic filter. */
          artifact=GetImageArtifact(image,"filter:c"); /* user C override */
          if (artifact != (const char *) NULL)
            C=StringToDouble(artifact,(char **) NULL);
        }
      else
        {
          artifact=GetImageArtifact(image,"filter:c");
          if (artifact != (const char *) NULL)
            {
              C=StringToDouble(artifact,(char **) NULL);
              B=1.0-2.0*C; /* Calculate B to get a Keys cubic filter. */
            }
        }
      {
        const double
          twoB = B+B;

        /*
          Convert B,C values into Cubic Coefficents. See CubicBC().
        */
        resize_filter->coefficient[0]=1.0-(1.0/3.0)*B;
        resize_filter->coefficient[1]=-3.0+twoB+C;
        resize_filter->coefficient[2]=2.0-1.5*B-C;
        resize_filter->coefficient[3]=(4.0/3.0)*B+4.0*C;
        resize_filter->coefficient[4]=-8.0*C-twoB;
        resize_filter->coefficient[5]=B+5.0*C;
        resize_filter->coefficient[6]=(-1.0/6.0)*B-C;
      }
    }

  /*
    Expert Option Request for verbose details of the resulting filter.
  */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp master
  {
#endif
    if (IsStringTrue(GetImageArtifact(image,"filter:verbose")) != MagickFalse)
      {
        double
          support,
          x;

        /*
          Set the weighting function properly when the weighting function
          may not exactly match the filter of the same name.  EG: a Point
          filter is really uses a Box weighting function with a different
          support than is typically used.
        */
        if (resize_filter->filter == Box)       filter_type=BoxFilter;
        if (resize_filter->filter == Sinc)      filter_type=SincFilter;
        if (resize_filter->filter == SincFast)  filter_type=SincFastFilter;
        if (resize_filter->filter == Jinc)      filter_type=JincFilter;
        if (resize_filter->filter == CubicBC)   filter_type=CubicFilter;
        if (resize_filter->window == Box)       window_type=BoxFilter;
        if (resize_filter->window == Sinc)      window_type=SincFilter;
        if (resize_filter->window == SincFast)  window_type=SincFastFilter;
        if (resize_filter->window == Jinc)      window_type=JincFilter;
        if (resize_filter->window == CubicBC)   window_type=CubicFilter;
        /*
          Report Filter Details.
        */
        support=GetResizeFilterSupport(resize_filter);  /* practical_support */
        (void) FormatLocaleFile(stdout,
          "# Resampling Filter (for graphing)\n#\n");
        (void) FormatLocaleFile(stdout,"# filter = %s\n",
          CommandOptionToMnemonic(MagickFilterOptions,filter_type));
        (void) FormatLocaleFile(stdout,"# window = %s\n",
          CommandOptionToMnemonic(MagickFilterOptions,window_type));
        (void) FormatLocaleFile(stdout,"# support = %.*g\n",
          GetMagickPrecision(),(double) resize_filter->support);
        (void) FormatLocaleFile(stdout,"# window-support = %.*g\n",
          GetMagickPrecision(),(double) resize_filter->window_support);
        (void) FormatLocaleFile(stdout,"# scale-blur = %.*g\n",
          GetMagickPrecision(),(double) resize_filter->blur);
        if ((filter_type == GaussianFilter) || (window_type == GaussianFilter))
          (void) FormatLocaleFile(stdout,"# gaussian-sigma = %.*g\n",
            GetMagickPrecision(),(double) resize_filter->coefficient[0]);
        if ( filter_type == KaiserFilter || window_type == KaiserFilter )
          (void) FormatLocaleFile(stdout,"# kaiser-beta = %.*g\n",
            GetMagickPrecision(),(double) resize_filter->coefficient[0]);
        (void) FormatLocaleFile(stdout,"# practical-support = %.*g\n",
          GetMagickPrecision(), (double) support);
        if ((filter_type == CubicFilter) || (window_type == CubicFilter))
          (void) FormatLocaleFile(stdout,"# B,C = %.*g,%.*g\n",
            GetMagickPrecision(),(double) B,GetMagickPrecision(),(double) C);
        (void) FormatLocaleFile(stdout,"\n");
        /*
          Output values of resulting filter graph -- for graphing filter result.
        */
        for (x=0.0; x <= support; x+=0.01f)
          (void) FormatLocaleFile(stdout,"%5.2lf\t%.*g\n",x,
            GetMagickPrecision(),(double)
            GetResizeFilterWeight(resize_filter,x));
        /*
          A final value so gnuplot can graph the 'stop' properly.
        */
        (void) FormatLocaleFile(stdout,"%5.2lf\t%.*g\n",support,
          GetMagickPrecision(),0.0);
      }
      /* Output the above once only for each image - remove setting */
    (void) DeleteImageArtifact((Image *) image,"filter:verbose");
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  }
#endif
  return(resize_filter);
}