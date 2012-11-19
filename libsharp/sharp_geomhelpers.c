/*
 *  This file is part of libsharp.
 *
 *  libsharp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libsharp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libsharp; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  libsharp is being developed at the Max-Planck-Institut fuer Astrophysik
 *  and financially supported by the Deutsches Zentrum fuer Luft- und Raumfahrt
 *  (DLR).
 */

/*! \file sharp_geomhelpers.c
 *  Spherical transform library
 *
 *  Copyright (C) 2006-2012 Max-Planck-Society
 *  Copyright (C) 2007-2008 Pavel Holoborodko (for gauss_legendre_tbl)
 *  \author Martin Reinecke \author Pavel Holoborodko
 */

#include <math.h>
#include "sharp_geomhelpers.h"
#include "c_utils.h"

void sharp_make_healpix_geom_info (int nside, int stride,
  sharp_geom_info **geom_info)
  {
  double *weight=RALLOC(double,2*nside);
  SET_ARRAY(weight,0,2*nside,1);
  sharp_make_weighted_healpix_geom_info (nside, stride, weight, geom_info);
  DEALLOC(weight);
  }

void sharp_make_weighted_healpix_geom_info (int nside, int stride,
  const double *weight, sharp_geom_info **geom_info)
  {
  const double pi=3.141592653589793238462643383279502884197;
  ptrdiff_t npix=(ptrdiff_t)nside*nside*12;
  ptrdiff_t ncap=2*(ptrdiff_t)nside*(nside-1);
  int nrings=4*nside-1;

  double *theta=RALLOC(double,nrings);
  double *weight_=RALLOC(double,nrings);
  int *nph=RALLOC(int,nrings);
  double *phi0=RALLOC(double,nrings);
  ptrdiff_t *ofs=RALLOC(ptrdiff_t,nrings);
  int *stride_=RALLOC(int,nrings);
  for (int m=0; m<nrings; ++m)
    {
    int ring=m+1;
    ptrdiff_t northring = (ring>2*nside) ? 4*nside-ring : ring;
    stride_[m] = stride;
    if (northring < nside)
      {
      theta[m] = 2*asin(northring/(sqrt(6.)*nside));
      nph[m] = 4*northring;
      phi0[m] = pi/nph[m];
      ofs[m] = 2*northring*(northring-1)*stride;
      }
    else
      {
      double fact1 = (8.*nside)/npix;
      double costheta = (2*nside-northring)*fact1;
      theta[m] = acos(costheta);
      nph[m] = 4*nside;
      if ((northring-nside) & 1)
        phi0[m] = 0;
      else
        phi0[m] = pi/nph[m];
      ofs[m] = (ncap + (northring-nside)*nph[m])*stride;
      }
    if (northring != ring) /* southern hemisphere */
      {
      theta[m] = pi-theta[m];
      ofs[m] = (npix - nph[m])*stride - ofs[m];
      }
    weight_[m]=4.*pi/npix*weight[northring-1];
    }

  sharp_make_geom_info (nrings, nph, ofs, stride_, phi0, theta, NULL, weight_,
    geom_info);

  DEALLOC(theta);
  DEALLOC(weight_);
  DEALLOC(nph);
  DEALLOC(phi0);
  DEALLOC(ofs);
  DEALLOC(stride_);
  }

/* Function adapted from GNU GSL file glfixed.c
   Original author: Pavel Holoborodko (http://www.holoborodko.com)

   Adjustments by M. Reinecke
    - adjusted interface (keep epsilon internal, return full number of points)
    - removed precomputed tables
    - tweaked Newton iteration to obtain higher accuracy */
static void gauss_legendre_tbl(int n, double* x, double* w)
  {
  const double pi = 3.141592653589793238462643383279502884197;
  const double eps = 3e-14;
  int i, k, m = (n+1)>>1;

  double t0 = 1 - (1-1./n) / (8.*n*n);
  double t1 = 1./(4.*n+2.);

  for (i=1; i<=m; ++i)
    {
    double x0 = cos(pi * ((i<<2)-1) * t1) * t0;

    int dobreak=0;
    int j=0;
    double dpdx;
    while(1)
      {
      double P_1 = 1.0;
      double P0 = x0;
      double dx, x1;

      for (k=2; k<=n; k++)
        {
        double P_2 = P_1;
        P_1 = P0;
//        P0 = ((2*k-1)*x0*P_1-(k-1)*P_2)/k;
        P0 = x0*P_1 + (k-1.)/k * (x0*P_1-P_2);
        }

//      dpdx = ((x0*P0 - P_1) * n) / ((x0-1.)*(x0+1.));
      dpdx = (x0*P0 - P_1) * n / (x0*x0-1.);

      /* Newton step */
      x1 = x0 - P0/dpdx;
      dx = x0-x1;
      x0 = x1;
      if (dobreak) break;

      if (fabs(dx)<=eps) dobreak=1;
      UTIL_ASSERT(++j<100,"convergence problem");
      }

    x[i-1] = -x0;
    x[n-i] = x0;
//    w[i-1] = w[n-i] = 2. / (((1.-x0)*(1.+x0)) * dpdx * dpdx);
    w[i-1] = w[n-i] = 2. / ((1.-x0*x0) * dpdx * dpdx);
    }
  }

static void makeweights (int bw, double *weights)
  {
  const double pi = 3.141592653589793238462643383279502884197;
  const double fudge = pi/(4*bw);
  for (int j=0; j<2*bw; ++j)
    {
    double tmpsum = 0;
    for (int k=0; k<bw; ++k)
      tmpsum += 1./(2*k+1) * sin((2*j+1)*(2*k+1)*fudge);
    tmpsum *= sin((2*j+1)*fudge);
    tmpsum *= 2./bw;
    weights[j] = tmpsum;
    }
  }

void sharp_make_gauss_geom_info (int nrings, int nphi, int stride_lon,
  int stride_lat, sharp_geom_info **geom_info)
  {
  const double pi=3.141592653589793238462643383279502884197;

  double *theta=RALLOC(double,nrings);
  double *weight=RALLOC(double,nrings);
  int *nph=RALLOC(int,nrings);
  double *phi0=RALLOC(double,nrings);
  ptrdiff_t *ofs=RALLOC(ptrdiff_t,nrings);
  int *stride_=RALLOC(int,nrings);

  gauss_legendre_tbl(nrings,theta,weight);
  for (int m=0; m<nrings; ++m)
    {
    theta[m] = acos(-theta[m]);
    nph[m]=nphi;
    phi0[m]=0;
    ofs[m]=(ptrdiff_t)m*stride_lat;
    stride_[m]=stride_lon;
    weight[m]*=2*pi/nphi;
    }

  sharp_make_geom_info (nrings, nph, ofs, stride_, phi0, theta, NULL, weight,
    geom_info);

  DEALLOC(theta);
  DEALLOC(weight);
  DEALLOC(nph);
  DEALLOC(phi0);
  DEALLOC(ofs);
  DEALLOC(stride_);
  }

void sharp_make_ecp_geom_info (int nrings, int nphi, double phi0,
  int stride_lon, int stride_lat, sharp_geom_info **geom_info)
  {
  const double pi=3.141592653589793238462643383279502884197;

  double *theta=RALLOC(double,nrings);
  double *weight=RALLOC(double,nrings);
  int *nph=RALLOC(int,nrings);
  double *phi0_=RALLOC(double,nrings);
  ptrdiff_t *ofs=RALLOC(ptrdiff_t,nrings);
  int *stride_=RALLOC(int,nrings);

  UTIL_ASSERT((nrings&1)==0,
    "Even number of rings needed for equidistant grid!");
  makeweights(nrings/2,weight);
  for (int m=0; m<nrings; ++m)
    {
    theta[m] = (m+0.5)*pi/nrings;
    nph[m]=nphi;
    phi0_[m]=phi0;
    ofs[m]=(ptrdiff_t)m*stride_lat;
    stride_[m]=stride_lon;
    weight[m]*=2*pi/nphi;
    }

  sharp_make_geom_info (nrings, nph, ofs, stride_, phi0_, theta, NULL, weight,
    geom_info);

  DEALLOC(theta);
  DEALLOC(weight);
  DEALLOC(nph);
  DEALLOC(phi0_);
  DEALLOC(ofs);
  DEALLOC(stride_);
  }

static double eps(int j, int J)
  {
  if ((j==0)||(j==J)) return 0.5;
  if ((j>0)&&(j<J)) return 1.;
  return 0.;
  }

/* Weights from Keiner & Potts: "Fast evaluation of quadrature formulae
   on the sphere", 2000 */
/* nrings MUST be odd */
void sharp_make_hw_geom_info (int nrings, int ppring, double phi0,
  int stride_lon, int stride_lat, sharp_geom_info **geom_info)
  {
  const double pi=3.141592653589793238462643383279502884197;

  double *theta=RALLOC(double,nrings);
  double *weight=RALLOC(double,nrings);
  int *nph=RALLOC(int,nrings);
  double *phi0_=RALLOC(double,nrings);
  ptrdiff_t *ofs=RALLOC(ptrdiff_t,nrings);
  int *stride_=RALLOC(int,nrings);

  UTIL_ASSERT((nrings&1)==1,"nrings must be an odd number");
  int lmax=(nrings-1)/2;

  for (int m=0; m<nrings; ++m)
    {
    theta[m]=pi*m/(nrings-1.);
    if (theta[m]<1e-15) theta[m]=1e-15;
    if (theta[m]>pi-1e-15) theta[m]=pi-1e-15;
    nph[m]=ppring;
    phi0_[m]=phi0;
    ofs[m]=(ptrdiff_t)m*stride_lat;
    stride_[m]=stride_lon;
    double wgt=0;
    double prefac=4*pi*eps(m,2*lmax)/lmax;
    for (int l=0; l<=lmax; ++l)
      wgt+=eps(l,lmax)/(1-4*l*l)*cos((pi*m*l)/lmax);
    weight[m]=prefac*wgt/nph[m];
    }

  sharp_make_geom_info (nrings, nph, ofs, stride_, phi0_, theta, NULL, weight,
    geom_info);

  DEALLOC(theta);
  DEALLOC(weight);
  DEALLOC(nph);
  DEALLOC(phi0_);
  DEALLOC(ofs);
  DEALLOC(stride_);
  }
