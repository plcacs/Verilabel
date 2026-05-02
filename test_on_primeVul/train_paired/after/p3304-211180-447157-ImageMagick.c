WandExport double *DrawGetStrokeDashArray(const DrawingWand *wand,
  size_t *number_elements)
{
  double
    *dasharray;

  register const double
    *p;

  register double
    *q;

  register ssize_t
    i;

  size_t
    n;

  assert(wand != (const DrawingWand *) NULL);
  assert(wand->signature == MagickWandSignature);
  if (wand->debug != MagickFalse)
    (void) LogMagickEvent(WandEvent,GetMagickModule(),"%s",wand->name);
  assert(number_elements != (size_t *) NULL);
  n=0;
  p=CurrentContext->dash_pattern;
  if (p != (const double *) NULL)
    while (fabs(*p++) >= MagickEpsilon)
      n++;
  *number_elements=n;
  dasharray=(double *) NULL;
  if (n != 0)
    {
      dasharray=(double *) AcquireQuantumMemory((size_t) n+1UL,
        sizeof(*dasharray));
      if (dasharray != (double *) NULL)
        {
          p=CurrentContext->dash_pattern;
          q=dasharray;
          for (i=0; i < (ssize_t) n; i++)
            *q++=(*p++);
          *q=0.0;
        }
    }
  return(dasharray);
}