static void inline ConvertXYZToJzazbz(const double X,const double Y,
  const double Z,const double white_luminance,double *Jz,double *az,double *bz)
{
#define Jzazbz_b  1.15  /* https://observablehq.com/@jrus/jzazbz */
#define Jzazbz_g  0.66
#define Jzazbz_c1  (3424.0/4096.0)
#define Jzazbz_c2  (2413.0/128.0)
#define Jzazbz_c3  (2392.0/128.0)
#define Jzazbz_n  (2610.0/16384.0)
#define Jzazbz_p  (1.7*2523.0/32.0)
#define Jzazbz_d  (-0.56)
#define Jzazbz_d0  (1.6295499532821566e-11)

  double
    gamma,
    Iz,
    L,
    Lp,
    M,
    Mp,
    S,
    Sp,
    Xp,
    Yp,
    Zp;

  Xp=(Jzazbz_b*X-Z*(Jzazbz_b-1));
  Yp=(Jzazbz_g*Y-X*(Jzazbz_g-1));
  Zp=Z;
  L=0.41478972*Xp+0.579999*Yp+0.0146480*Zp;
  M=(-0.2015100)*Xp+1.120649*Yp+0.0531008*Zp;
  S=(-0.0166008)*Xp+0.264800*Yp+0.6684799*Zp;
  gamma=pow(L/white_luminance,Jzazbz_n);
  Lp=pow((Jzazbz_c1+Jzazbz_c2*gamma)/(1.0+Jzazbz_c3*gamma),Jzazbz_p);
  gamma=pow(M/white_luminance,Jzazbz_n);
  Mp=pow((Jzazbz_c1+Jzazbz_c2*gamma)/(1.0+Jzazbz_c3*gamma),Jzazbz_p);
  gamma=pow(S/white_luminance,Jzazbz_n);
  Sp=pow((Jzazbz_c1+Jzazbz_c2*gamma)/(1.0+Jzazbz_c3*gamma),Jzazbz_p);
  Iz=0.5*Lp+0.5*Mp;
  *az=3.52400*Lp-4.066708*Mp+0.542708*Sp+0.5;
  *bz=0.199076*Lp+1.096799*Mp-1.295875*Sp+0.5;
  *Jz=((Jzazbz_d+1.0)*Iz)/(Jzazbz_d*Iz+1.0)-Jzazbz_d0;
}