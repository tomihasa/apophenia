/** \file apop_probit.c

Copyright (c) 2005 by Ben Klemens. Licensed under the GNU GPL version 2.
*/
#include "model.h"



//The default list. Probably don't need them all.
#include "name.h"
#include "bootstrap.h"
#include "regression.h"
#include "conversions.h"
#include "likelihoods.h"
#include "model.h"
#include "linear_algebra.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_histogram.h>
#include <gsl/gsl_sort_vector.h>
#include <gsl/gsl_permutation.h>
#include <stdio.h>
#include <assert.h>



/** This function is used to keep the minimizer away from bounds.

If you just return GSL_POSINF at the bounds, it's not necessarily smart
enough to get it.  This helps the minimzer along by providing a (almost)
continuous, steep line which steers the minimizer back to the covered
range. 
\todo Replace this with apop_constraints.
*/
static double keep_away(double value, double limit,  double base){
	return (50000+fabs(value - limit)) * base;
}

static apop_estimate * probit_estimate(gsl_matrix * data, apop_inventory *uses, void *parameters){
	apop_inventory_filter(uses, apop_probit.inventory_filter);
	return apop_maximum_likelihood(data, uses, apop_probit, *(apop_estimation_params *)parameters);
}


//////////////////
//The probit model
//////////////////

//This section includes some trickery to avoid recalculating beta dot x.
//
static gsl_vector *beta_dot_x 		= NULL;
static int	beta_dot_x_is_current	= 0;

static void	dot(const gsl_vector *beta, gsl_matrix *data){
gsl_matrix_view p 	= gsl_matrix_submatrix(data,0,1,data->size1,data->size2-1);
gsl_vector	*t;
	//if (beta_dot_x) printf("comparing %i with %i ",data->size1, beta_dot_x->size); fflush(NULL);
	if (beta_dot_x && (data->size1 != beta_dot_x->size)){
		//printf("freeing. "); fflush(NULL);
		gsl_vector_free(beta_dot_x); 
		beta_dot_x = NULL;
		//printf("freed.\n"); fflush(NULL);
		}
	if (!beta_dot_x){
		//printf("allocating %i. ",data->size1); fflush(NULL);
		t 		= gsl_vector_alloc(data->size1);		//global var
		beta_dot_x 	= t;						//global var
		//printf("allocated.\n"); fflush(NULL);
		}
        gsl_blas_dgemv (CblasNoTrans, 1.0, &p.matrix, beta, 0.0, beta_dot_x);	//dot product
}

/*
find (data dot beta'), then find the integral of the \f$\cal{N}(0,1)\f$
up to that point. Multiply likelihood either by that or by 1-that, depending 
on the choice the data made.
*/
static double probit_log_likelihood(const gsl_vector *beta, void *d){
int		i;
long double	n, total_prob	= 0;
gsl_matrix 	*data 		= d;		//just type casting.
	dot(beta,data);
	for(i=0;i< data->size1; i++){
		n	=gsl_cdf_gaussian_P(gsl_vector_get(beta_dot_x,i),1);
		if (gsl_matrix_get(data, i, 0)==0) 	total_prob	+= log(n);
		else 					total_prob	+= log((1 - n));
	}
	return total_prob;
}

/* The derivative of the probit distribution, for use in likelihood
  minimization. You'll probably never need to call this directly.*/
static void probit_dlog_likelihood(const gsl_vector *beta, void *d, gsl_vector *gradient){
	//derivative of the above. 
int		i, j;
long double	one_term, beta_term_sum;
gsl_matrix 	*data 		= d;		//just type casting.
	if (!beta_dot_x_is_current) 	
		dot(beta,data); 
	for(j=0; j< beta->size; j++){
		beta_term_sum	= 0;
		for(i=0; i< data->size1; i++){
			one_term	 = gsl_matrix_get(data, i,j+1)
						* gsl_ran_gaussian_pdf(gsl_vector_get(beta_dot_x,i),1);
			if (gsl_matrix_get(data, i, 0)==0) 	
				one_term	/= gsl_cdf_gaussian_P(gsl_vector_get(beta_dot_x,i),1);
			else 	one_term	/= (gsl_cdf_gaussian_P(gsl_vector_get(beta_dot_x,i),1)-1);
			beta_term_sum	+= one_term;
		}
	gsl_vector_set(gradient,j,beta_term_sum);
	}
	gsl_vector_free(beta_dot_x);
	beta_dot_x	= NULL;
}


/** Saves some time in calculating both log likelihood and dlog
likelihood for probit.	*/
static void probit_fdf( const gsl_vector *beta, void *d, double *f, gsl_vector *df){
	*f	= probit_log_likelihood(beta, d);
	beta_dot_x_is_current	=1;
	probit_dlog_likelihood(beta, d, df);
	beta_dot_x_is_current	=0;
}

/** The Probit model.
 The first column of the data matrix this model expects is ones and zeros;
 the remaining columns are values of the independent variables. Thus,
 the model will return (data columns)-1 parameters.

\ingroup models
*/
apop_model apop_probit = {"Probit", -1, 
{
	1,	//parameters
	1,	//covariance
	1,	//confidence
	0,	//predicted
	0,	//residuals
	1,	//log_likelihood
	1	//names;
}, 
	probit_estimate, probit_log_likelihood, probit_dlog_likelihood, probit_fdf, NULL, NULL};
//apop_model apop_probit = {"Probit", -1, NULL, apop_probit_log_likelihood, NULL, apop_probit_fdf,  NULL};