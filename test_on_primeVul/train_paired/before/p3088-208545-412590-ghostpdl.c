FloydSteinbergDitheringC(gx_device_bjc_printer *dev,
                         byte *row, byte *dithered, uint width,
                         uint raster, bool limit_extr, bool composeK)
{   byte byteC=0, byteM=0, byteY=0, byteK=0, bitmask = 0x80; /* first bit */
    int i;
    int errorC = 0, errorM = 0, errorY = 0, delta;
    int err_corrC, err_corrM, err_corrY;
    int *err_vect;

    if (dev->FloydSteinbergDirectionForward) {
        err_vect = dev->FloydSteinbergErrorsC + 3;         /* errCMY */
        /* First  point */

        for( i=width; i>0; i--, row+=4, err_vect+=3) { /*separate components */

/*                                               C     +       K           */
            err_corrC = dev->bjc_gamma_tableC[ (*row)    + (*(row+3))]
                          + dev->FloydSteinbergC;
            err_corrM = dev->bjc_gamma_tableM[(*(row+1)) + (*(row+3))]
                          + dev->FloydSteinbergM;
            err_corrY = dev->bjc_gamma_tableY[(*(row+2)) + (*(row+3))]
                          + dev->FloydSteinbergY;

            if(err_corrC > 4080 && limit_extr) err_corrC = 4080;
            if(err_corrM > 4080 && limit_extr) err_corrM = 4080;
            if(err_corrY > 4080 && limit_extr) err_corrY = 4080;

            errorC += err_corrC + (*(err_vect + 3));       /* CMYCMYCMY */
            errorM += err_corrM + (*(err_vect + 4));       /* |  ^  !   */
            errorY += err_corrY + (*(err_vect + 5));

            if(errorC > dev->bjc_treshold[bjc_rand(dev)])  {
                errorC -=  4080;
                byteC |=  bitmask;
            }

            if(errorM > dev->bjc_treshold[bjc_rand(dev)])  {
                errorM -=  4080;
                byteM |=  bitmask;
            }

            if(errorY > dev->bjc_treshold[bjc_rand(dev)])  {
                errorY -=  4080;
                byteY |=  bitmask;
            }

            *(err_vect+3) = (errorC + 8) >> 4;                   /* 1/16 */
            delta = errorC << 1;                                 /* 2 err */
            errorC += delta;
            *(err_vect-3) += (errorC + 8) >> 4;                  /* 3/16  */
            errorC += delta;
            *err_vect += (errorC + 8) >> 4;                           /* 5/16  */
            errorC += delta + 8;                                  /* 7/16  */
            errorC >>= 4;

            *(err_vect+4) = (errorM + 8) >> 4;                    /* 1/16 */
            delta = errorM << 1;                                 /* 2 err */
            errorM += delta;
            *(err_vect-2) += (errorM + 8) >> 4;                       /* 3/16  */
            errorM += delta;
            *(err_vect+1) += (errorM + 8) >> 4;                           /* 5/16  */
            errorM += delta + 8;                                     /* 7/16  */
            errorM >>= 4;

            *(err_vect+5) = (errorY + 8) >> 4;                      /* 1/16 */
            delta = errorY << 1;                                 /* 2 err */
            errorY += delta;
            *(err_vect-1) += (errorY + 8) >> 4;                       /* 3/16  */
            errorY += delta;
            *(err_vect+2) += (errorY + 8) >> 4;                           /* 5/16  */
            errorY += delta + 8;                                     /* 7/16  */
            errorY >>= 4;

            if (bitmask == 0x01) {
                bitmask = 0x80;
                if(composeK) {
                    byteK = byteC & byteM & byteY;
                    byteC = byteC & ~byteK;
                    byteM = byteM & ~byteK;
                    byteY = byteY & ~byteK;
                }                               /* if no K byteK always 0 */
                *dithered            = byteC;
                *(dithered+  raster) = byteM;
                *(dithered+2*raster) = byteY;
                *(dithered+3*raster) = byteK;
                byteC = byteM = byteY = byteK = 0;
                dithered++;
            }
            else if(i == 1) {
                if(composeK) {
                    byteK = byteC & byteM & byteY;
                    byteC = byteC & ~byteK;
                    byteM = byteM & ~byteK;
                    byteY = byteY & ~byteK;
                }                               /* if no K byteK always 0 */
                *dithered            = byteC;
                *(dithered+  raster) = byteM;
                *(dithered+2*raster) = byteY;
                *(dithered+3*raster) = byteK;
            }
            else bitmask >>= 1;
        }
        dev->FloydSteinbergDirectionForward=false;
    }
    else {
        row += (width << 2) - 4;   /* point to the end of the row */
        dithered += raster - 1;
        err_vect = dev->FloydSteinbergErrorsC + 3 * width + 3;       /* errCMY */
        bitmask = 1 << ((raster << 3 ) - width) ;

        for( i=width; i>0; i--, row-=4, err_vect-=3) {

            err_corrC = dev->bjc_gamma_tableC[  (*row)   + (*(row+3))]
                          + dev->FloydSteinbergC;
            err_corrM = dev->bjc_gamma_tableM[(*(row+1)) + (*(row+3))]
                          + dev->FloydSteinbergM;
            err_corrY = dev->bjc_gamma_tableY[(*(row+2)) + (*(row+3))]
                          + dev->FloydSteinbergY;

            if(err_corrC > 4080 && limit_extr) err_corrC = 4080;
            if(err_corrM > 4080 && limit_extr) err_corrM = 4080;
            if(err_corrY > 4080 && limit_extr) err_corrY = 4080;

            errorC += err_corrC + (*(err_vect - 3));       /* CMYCMYCMY */
            errorM += err_corrM + (*(err_vect - 2));       /* !  ^  |   */
            errorY += err_corrY + (*(err_vect - 1));

            if(errorC > dev->bjc_treshold[bjc_rand(dev)])  {
                errorC -=  4080;
                byteC |=  bitmask;
            }

            if(errorM > dev->bjc_treshold[bjc_rand(dev)])  {
                errorM -=  4080;
                byteM |=  bitmask;
            }

            if(errorY > dev->bjc_treshold[bjc_rand(dev)])  {
                errorY -=  4080;
                byteY |=  bitmask;
            }

            *(err_vect-3) = (errorC + 8) >> 4;                      /* 1/16 */
            delta = errorC << 1;                                 /* 2 err */
            errorC += delta;
            *(err_vect+3) += (errorC + 8) >> 4;                       /* 3/16  */
            errorC += delta;
            *err_vect += (errorC + 8) >> 4;                           /* 5/16  */
            errorC += delta + 8;                                     /* 7/16  */
            errorC >>= 4;

            *(err_vect-2) = (errorM + 8) >> 4;                      /* 1/16 */
            delta = errorM << 1;                                 /* 2 err */
            errorM += delta;
            *(err_vect+4) += (errorM + 8) >> 4;                       /* 3/16  */
            errorM += delta;
            *(err_vect+1) += (errorM + 8) >> 4;                           /* 5/16  */
            errorM += delta + 8;                                     /* 7/16  */
            errorM >>= 4;

            *(err_vect-1) = (errorY + 8) >> 4;                      /* 1/16 */
            delta = errorY << 1;                                 /* 2 err */
            errorY += delta;
            *(err_vect+5) += (errorY + 8) >> 4;                       /* 3/16  */
            errorY += delta;
            *(err_vect+2) += (errorY + 8) >> 4;                           /* 5/16  */
            errorY += delta + 8;                                     /* 7/16  */
            errorY >>= 4;

            if (bitmask == 0x80) {
                bitmask = 0x01;
                if(composeK) {
                    byteK = byteC & byteM & byteY;
                    byteC = byteC & ~byteK;
                    byteM = byteM & ~byteK;
                    byteY = byteY & ~byteK;
                }                               /* if no K byteK always 0 */
                *dithered            = byteC;
                *(dithered+  raster) = byteM;
                *(dithered+2*raster) = byteY;
                *(dithered+3*raster) = byteK;
                byteC = byteM = byteY = byteK = 0;
                dithered--;
            }
            else if(i == 1) {
                if(composeK) {
                    byteK = byteC & byteM & byteY;
                    byteC = byteC & ~byteK;
                    byteM = byteM & ~byteK;
                    byteY = byteY & ~byteK;
                }                               /* if no K byteK always 0 */
                *dithered            = byteC;
                *(dithered+  raster) = byteM;
                *(dithered+2*raster) = byteY;
                *(dithered+3*raster) = byteK;
            }
            else bitmask <<= 1;
        }
        dev->FloydSteinbergDirectionForward=true;
    }
}