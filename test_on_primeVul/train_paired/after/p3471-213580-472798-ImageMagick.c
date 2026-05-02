static double ApplyEvaluateOperator(RandomInfo *random_info,const Quantum pixel,
  const MagickEvaluateOperator op,const double value)
{
  double
    result;

  register ssize_t
    i;

  result=0.0;
  switch (op)
  {
    case UndefinedEvaluateOperator:
      break;
    case AbsEvaluateOperator:
    {
      result=(double) fabs((double) (pixel+value));
      break;
    }
    case AddEvaluateOperator:
    {
      result=(double) (pixel+value);
      break;
    }
    case AddModulusEvaluateOperator:
    {
      /*
        This returns a 'floored modulus' of the addition which is a positive
        result.  It differs from % or fmod() that returns a 'truncated modulus'
        result, where floor() is replaced by trunc() and could return a
        negative result (which is clipped).
      */
      result=pixel+value;
      result-=(QuantumRange+1.0)*floor((double) result/(QuantumRange+1.0));
      break;
    }
    case AndEvaluateOperator:
    {
      result=(double) ((ssize_t) pixel & (ssize_t) (value+0.5));
      break;
    }
    case CosineEvaluateOperator:
    {
      result=(double) (QuantumRange*(0.5*cos((double) (2.0*MagickPI*
        QuantumScale*pixel*value))+0.5));
      break;
    }
    case DivideEvaluateOperator:
    {
      result=pixel/(value == 0.0 ? 1.0 : value);
      break;
    }
    case ExponentialEvaluateOperator:
    {
      result=(double) (QuantumRange*exp((double) (value*QuantumScale*pixel)));
      break;
    }
    case GaussianNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,
        GaussianNoise,value);
      break;
    }
    case ImpulseNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,ImpulseNoise,
        value);
      break;
    }
    case LaplacianNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,
        LaplacianNoise,value);
      break;
    }
    case LeftShiftEvaluateOperator:
    {
      result=(double) pixel;
      for (i=0; i < (ssize_t) value; i++)
        result*=2.0;
      break;
    }
    case LogEvaluateOperator:
    {
      if ((QuantumScale*pixel) >= MagickEpsilon)
        result=(double) (QuantumRange*log((double) (QuantumScale*value*pixel+
          1.0))/log((double) (value+1.0)));
      break;
    }
    case MaxEvaluateOperator:
    {
      result=(double) EvaluateMax((double) pixel,value);
      break;
    }
    case MeanEvaluateOperator:
    {
      result=(double) (pixel+value);
      break;
    }
    case MedianEvaluateOperator:
    {
      result=(double) (pixel+value);
      break;
    }
    case MinEvaluateOperator:
    {
      result=(double) MagickMin((double) pixel,value);
      break;
    }
    case MultiplicativeNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,
        MultiplicativeGaussianNoise,value);
      break;
    }
    case MultiplyEvaluateOperator:
    {
      result=(double) (value*pixel);
      break;
    }
    case OrEvaluateOperator:
    {
      result=(double) ((ssize_t) pixel | (ssize_t) (value+0.5));
      break;
    }
    case PoissonNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,PoissonNoise,
        value);
      break;
    }
    case PowEvaluateOperator:
    {
      if (pixel < 0)
        result=(double) -(QuantumRange*pow((double) -(QuantumScale*pixel),
          (double) value));
      else
        result=(double) (QuantumRange*pow((double) (QuantumScale*pixel),
          (double) value));
      break;
    }
    case RightShiftEvaluateOperator:
    {
      result=(double) pixel;
      for (i=0; i < (ssize_t) value; i++)
        result/=2.0;
      break;
    }
    case RootMeanSquareEvaluateOperator:
    {
      result=(double) (pixel*pixel+value);
      break;
    }
    case SetEvaluateOperator:
    {
      result=value;
      break;
    }
    case SineEvaluateOperator:
    {
      result=(double) (QuantumRange*(0.5*sin((double) (2.0*MagickPI*
        QuantumScale*pixel*value))+0.5));
      break;
    }
    case SubtractEvaluateOperator:
    {
      result=(double) (pixel-value);
      break;
    }
    case SumEvaluateOperator:
    {
      result=(double) (pixel+value);
      break;
    }
    case ThresholdEvaluateOperator:
    {
      result=(double) (((double) pixel <= value) ? 0 : QuantumRange);
      break;
    }
    case ThresholdBlackEvaluateOperator:
    {
      result=(double) (((double) pixel <= value) ? 0 : pixel);
      break;
    }
    case ThresholdWhiteEvaluateOperator:
    {
      result=(double) (((double) pixel > value) ? QuantumRange : pixel);
      break;
    }
    case UniformNoiseEvaluateOperator:
    {
      result=(double) GenerateDifferentialNoise(random_info,pixel,UniformNoise,
        value);
      break;
    }
    case XorEvaluateOperator:
    {
      result=(double) ((ssize_t) pixel ^ (ssize_t) (value+0.5));
      break;
    }
  }
  return(result);
}