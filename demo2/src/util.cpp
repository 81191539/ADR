#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"
#include "diffuse.h"

#define NR_END 1

double * dvector (long nl, long nh)
{
    double *v;    
    v=(double *) malloc((nh-nl+1+1)*sizeof(double));
    return v-nl+1;
}

void free_dvector(double *m, long nl, long nh)
{
    free(m+nl-NR_END );
}

int *ivector(long nl, long nh)    //新增
{
	int *v;

	v = (int *)malloc((nh - nl + 1) * sizeof(int));
	return v - nl;

}



void free_ivector(int *m, long nl, long nh)   //新增
{
	free(m + nl);
}



double ***dvector_3d(long nl, long nh)    //新增
{
	double ***v;

	v = (double ***)malloc((nh - nl + 1) * sizeof(double**));
	return v - nl;

}



void free_dvector_3d(double ***m, long nl, long nh)   //新增
{
	free(m + nl);
}



double **dmatrix(long nrl, long nrh, long ncl, long nch)
{
	double **m;
	long i, nrow = nrh - nrl + 1, ncol = nch - ncl + 1;

	m = (double **)malloc((nrow) * sizeof(double*));
	m -= nrl;

	m[nrl] = (double *)malloc((nrow*ncol) * sizeof(double));
	m[nrl] -= ncl;

	for (i = nrl + 1; i <= nrh; i++) m[i] = m[i - 1] + ncol;

	return m;
}



void free_dmatrix(double **m, long nrl, long nrh, long ncl, long nch)
{
	free(m[nrl] + ncl);
	free(m + nrl);
}



void mat_add(double **a, double **b, double **c, int xl, int xr, int yl, int yr)
{
	int i, j;

	for (i = xl; i <= xr; i++) {
		for (j = yl; j <= yr; j++) {
			a[i][j] = b[i][j] + c[i][j];
		}
	}

	return;
}

void vec_copy(double *a, double *b, int xl, int xr)
{
	int i;

	for (i = xl; i <= xr; i++) a[i] = b[i];

	return;
}

void zero_vector(double *a, int xl, int xr)
{
	int i;

	for (i = xl; i <= xr; i++) a[i] = 0.0;

	return;
}

void zero_matrix(double **a, int xl, int xr, int yl, int yr)
{
	int i, j;

	for (i = xl; i <= xr; i++) {
		for (j = yl; j <= yr; j++) {

			a[i][j] = 0.0;

		}
	}

	return;
}



void mat_copy(double **a, double **b, int xl, int xr, int yl, int yr)
{
	int i, j;

	for (i = xl; i <= xr; i++)
		for (j = yl; j <= yr; j++)

			a[i][j] = b[i][j];

	return;
}



void mat_sub(double **a, double **b, double **c, int nrl, int nrh, int ncl, int nch)
{
	int i, j;

	for (i = nrl; i <= nrh; i++)
		for (j = ncl; j <= nch; j++)
			a[i][j] = b[i][j] - c[i][j];

	return;
}


double mat_max(double **a, int nrl, int nrh, int ncl, int nch)
{
	int i, j;
	double x = 0.0;

	for (i = nrl; i <= nrh; i++) {
		for (j = ncl; j <= nch; j++) {

			if (fabs(a[i][j]) > x)
				x = fabs(a[i][j]);
		}
	}

	return x;
}


double vec_max(double *a, int nrl, int nrh)
{
	int i;
	double x;
	x = a[nrl];
	for (i = nrl; i <= nrh; i++) {
		if (a[i] > x)
			x = a[i];
	}
	return x;
}


double vec_min(double *a, int nrl, int nrh)
{
	int i;
	double x;
	x = a[nrl];
	for (i = nrl; i <= nrh; i++) {
		if (a[i] < x)
			x = a[i];
	}
	return x;
}