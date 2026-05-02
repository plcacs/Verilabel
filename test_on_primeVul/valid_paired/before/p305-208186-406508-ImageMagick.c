MagickExport void ScaleResampleFilter(ResampleFilter *resample_filter,
  const double dux,const double duy,const double dvx,const double dvy)
{
  double A,B,C,F;

  assert(resample_filter != (ResampleFilter *) NULL);
  assert(resample_filter->signature == MagickCoreSignature);

  resample_filter->limit_reached = MagickFalse;

  /* A 'point' filter forces use of interpolation instead of area sampling */
  if ( resample_filter->filter == PointFilter )
    return; /* EWA turned off - nothing to do */

#if DEBUG_ELLIPSE
  (void) FormatLocaleFile(stderr, "# -----\n" );
  (void) FormatLocaleFile(stderr, "dux=%lf; dvx=%lf;   duy=%lf; dvy=%lf;\n",
       dux, dvx, duy, dvy);
#endif

  /* Find Ellipse Coefficents such that
        A*u^2 + B*u*v + C*v^2 = F
     With u,v relative to point around which we are resampling.
     And the given scaling dx,dy vectors in u,v space
         du/dx,dv/dx   and  du/dy,dv/dy
  */
#if EWA
  /* Direct conversion of derivatives into elliptical coefficients
     However when magnifying images, the scaling vectors will be small
     resulting in a ellipse that is too small to sample properly.
     As such we need to clamp the major/minor axis to a minumum of 1.0
     to prevent it getting too small.
  */
#if EWA_CLAMP
  { double major_mag,
           minor_mag,
           major_x,
           major_y,
           minor_x,
           minor_y;

  ClampUpAxes(dux,dvx,duy,dvy, &major_mag, &minor_mag,
                &major_x, &major_y, &minor_x, &minor_y);
  major_x *= major_mag;  major_y *= major_mag;
  minor_x *= minor_mag;  minor_y *= minor_mag;
#if DEBUG_ELLIPSE
  (void) FormatLocaleFile(stderr, "major_x=%lf; major_y=%lf;  minor_x=%lf; minor_y=%lf;\n",
        major_x, major_y, minor_x, minor_y);
#endif
  A = major_y*major_y+minor_y*minor_y;
  B = -2.0*(major_x*major_y+minor_x*minor_y);
  C = major_x*major_x+minor_x*minor_x;
  F = major_mag*minor_mag;
  F *= F; /* square it */
  }
#else /* raw unclamped EWA */
  A = dvx*dvx+dvy*dvy;
  B = -2.0*(dux*dvx+duy*dvy);
  C = dux*dux+duy*duy;
  F = dux*dvy-duy*dvx;
  F *= F; /* square it */
#endif /* EWA_CLAMP */

#else /* HQ_EWA */
  /*
    This Paul Heckbert's "Higher Quality EWA" formula, from page 60 in his
    thesis, which adds a unit circle to the elliptical area so as to do both
    Reconstruction and Prefiltering of the pixels in the resampling.  It also
    means it is always likely to have at least 4 pixels within the area of the
    ellipse, for weighted averaging.  No scaling will result with F == 4.0 and
    a circle of radius 2.0, and F smaller than this means magnification is
    being used.

    NOTE: This method produces a very blury result at near unity scale while
    producing perfect results for strong minitification and magnifications.

    However filter support is fixed to 2.0 (no good for Windowed Sinc filters)
  */
  A = dvx*dvx+dvy*dvy+1;
  B = -2.0*(dux*dvx+duy*dvy);
  C = dux*dux+duy*duy+1;
  F = A*C - B*B/4;
#endif

#if DEBUG_ELLIPSE
  (void) FormatLocaleFile(stderr, "A=%lf; B=%lf; C=%lf; F=%lf\n", A,B,C,F);

  /* Figure out the various information directly about the ellipse.
     This information currently not needed at this time, but may be
     needed later for better limit determination.

     It is also good to have as a record for future debugging
  */
  { double alpha, beta, gamma, Major, Minor;
    double Eccentricity, Ellipse_Area, Ellipse_Angle;

    alpha = A+C;
    beta  = A-C;
    gamma = sqrt(beta*beta + B*B );

    if ( alpha - gamma <= MagickEpsilon )
      Major=MagickMaximumValue;
    else
      Major=sqrt(2*F/(alpha - gamma));
    Minor = sqrt(2*F/(alpha + gamma));

    (void) FormatLocaleFile(stderr, "# Major=%lf; Minor=%lf\n", Major, Minor );

    /* other information about ellipse include... */
    Eccentricity = Major/Minor;
    Ellipse_Area = MagickPI*Major*Minor;
    Ellipse_Angle = atan2(B, A-C);

    (void) FormatLocaleFile(stderr, "# Angle=%lf   Area=%lf\n",
         (double) RadiansToDegrees(Ellipse_Angle), Ellipse_Area);
  }
#endif

  /* If one or both of the scaling vectors is impossibly large
     (producing a very large raw F value), we may as well not bother
     doing any form of resampling since resampled area is very large.
     In this case some alternative means of pixel sampling, such as
     the average of the whole image is needed to get a reasonable
     result. Calculate only as needed.
  */
  if ( (4*A*C - B*B) > MagickMaximumValue ) {
    resample_filter->limit_reached = MagickTrue;
    return;
  }

  /* Scale ellipse to match the filters support
     (that is, multiply F by the square of the support)
     Simplier to just multiply it by the support twice!
  */
  F *= resample_filter->support;
  F *= resample_filter->support;

  /* Orthogonal bounds of the ellipse */
  resample_filter->Ulimit = sqrt(C*F/(A*C-0.25*B*B));
  resample_filter->Vlimit = sqrt(A*F/(A*C-0.25*B*B));

  /* Horizontally aligned parallelogram fitted to Ellipse */
  resample_filter->Uwidth = sqrt(F/A); /* Half of the parallelogram width */
  resample_filter->slope = -B/(2.0*A); /* Reciprocal slope of the parallelogram */

#if DEBUG_ELLIPSE
  (void) FormatLocaleFile(stderr, "Ulimit=%lf; Vlimit=%lf; UWidth=%lf; Slope=%lf;\n",
           resample_filter->Ulimit, resample_filter->Vlimit,
           resample_filter->Uwidth, resample_filter->slope );
#endif

  /* Check the absolute area of the parallelogram involved.
   * This limit needs more work, as it is too slow for larger images
   * with tiled views of the horizon.
  */
  if ( (resample_filter->Uwidth * resample_filter->Vlimit)
         > (4.0*resample_filter->image_area)) {
    resample_filter->limit_reached = MagickTrue;
    return;
  }

  /* Scale ellipse formula to directly index the Filter Lookup Table */
  { double scale;
#if FILTER_LUT
    /* scale so that F = WLUT_WIDTH; -- hardcoded */
    scale = (double)WLUT_WIDTH/F;
#else
    /* scale so that F = resample_filter->F (support^2) */
    scale = resample_filter->F/F;
#endif
    resample_filter->A = A*scale;
    resample_filter->B = B*scale;
    resample_filter->C = C*scale;
  }
}