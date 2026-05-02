MagickExport double GenerateDifferentialNoise(RandomInfo *random_info,
  const Quantum pixel,const NoiseType noise_type,const MagickRealType attenuate)
{
#define SigmaUniform  (attenuate*0.015625)
#define SigmaGaussian  (attenuate*0.015625)
#define SigmaImpulse  (attenuate*0.1)
#define SigmaLaplacian (attenuate*0.0390625)
#define SigmaMultiplicativeGaussian  (attenuate*0.5)
#define SigmaPoisson  (attenuate*12.5)
#define SigmaRandom  (attenuate)
#define TauGaussian  (attenuate*0.078125)

  double
    alpha,
    beta,
    noise,
    sigma;

  alpha=GetPseudoRandomValue(random_info);
  switch (noise_type)
  {
    case UniformNoise:
    default:
    {
      noise=(double) (pixel+QuantumRange*SigmaUniform*(alpha-0.5));
      break;
    }
    case GaussianNoise:
    {
      double
        gamma,
        tau;

      if (fabs(alpha) < MagickEpsilon)
        alpha=1.0;
      beta=GetPseudoRandomValue(random_info);
      gamma=sqrt(-2.0*log(alpha));
      sigma=gamma*cos((double) (2.0*MagickPI*beta));
      tau=gamma*sin((double) (2.0*MagickPI*beta));
      noise=(double) (pixel+sqrt((double) pixel)*SigmaGaussian*sigma+
        QuantumRange*TauGaussian*tau);
      break;
    }
    case ImpulseNoise:
    {
      if (alpha < (SigmaImpulse/2.0))
        noise=0.0;
      else
        if (alpha >= (1.0-(SigmaImpulse/2.0)))
          noise=(double) QuantumRange;
        else
          noise=(double) pixel;
      break;
    }
    case LaplacianNoise:
    {
      if (alpha <= 0.5)
        {
          if (alpha <= MagickEpsilon)
            noise=(double) (pixel-QuantumRange);
          else
            noise=(double) (pixel+QuantumRange*SigmaLaplacian*log(2.0*alpha)+
              0.5);
          break;
        }
      beta=1.0-alpha;
      if (beta <= (0.5*MagickEpsilon))
        noise=(double) (pixel+QuantumRange);
      else
        noise=(double) (pixel-QuantumRange*SigmaLaplacian*log(2.0*beta)+0.5);
      break;
    }
    case MultiplicativeGaussianNoise:
    {
      sigma=1.0;
      if (alpha > MagickEpsilon)
        sigma=sqrt(-2.0*log(alpha));
      beta=GetPseudoRandomValue(random_info);
      noise=(double) (pixel+pixel*SigmaMultiplicativeGaussian*sigma*
        cos((double) (2.0*MagickPI*beta))/2.0);
      break;
    }
    case PoissonNoise:
    {
      double
        poisson;

      ssize_t
        i;

      poisson=exp(-SigmaPoisson*QuantumScale*pixel);
      for (i=0; alpha > poisson; i++)
      {
        beta=GetPseudoRandomValue(random_info);
        alpha*=beta;
      }
      noise=(double) (QuantumRange*i/SigmaPoisson);
      break;
    }
    case RandomNoise:
    {
      noise=(double) (QuantumRange*SigmaRandom*alpha);
      break;
    }
  }
  return(noise);
}