/** \file apop_rank_exponential.c

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
#include <apophenia/stats.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_histogram.h>
#include <gsl/gsl_sort_vector.h>
#include <gsl/gsl_permutation.h>
#include <stdio.h>
#include <assert.h>

/* Let k be the rank, and x_k be the number of elements at that rank;
 then the mean rank (and therefore the most likely estimate for the
 exponential parameter) is sum(k * x_k)/sum(x) */
static apop_estimate * exponential_rank_estimate(gsl_matrix * data, apop_inventory *uses, void *parameters){
double          colsum,
                numerator   = 0,
                grand_total = 0;
apop_estimate 	*est	    = apop_estimate_alloc(data->size1,1,NULL, *uses);
gsl_vector_view v;
int             i;
	apop_inventory_filter(uses, apop_exponential_rank.inventory_filter);
    for(i=0; i< data->size2; i++){
        v            = gsl_matrix_column(data, i);
        colsum       = apop_sum(&(v.vector));
        numerator   += colsum * i;
        grand_total += colsum;
    }
	gsl_vector_set(est->parameters, 0, numerator/grand_total);
	if (est->uses.log_likelihood)
		est->log_likelihood	= apop_exponential_rank.log_likelihood(est->parameters, data);
	if (est->uses.covariance)
		apop_numerical_var_covar_matrix(apop_exponential_rank, est, data);
	return est;
}

/*
static apop_estimate * exponential_rank_estimate(gsl_matrix * data, apop_inventory *uses, void *parameters){
	apop_inventory_filter(uses, apop_exponential_rank.inventory_filter);
	return apop_maximum_likelihood(data, uses, apop_exponential_rank, *(apop_estimation_params *)parameters);
}
*/

static double beta_greater_than_x_constraint(gsl_vector *beta, void * d, gsl_vector *returned_beta){
double  limit       = 0,
        tolerance   = 1e10*GSL_DBL_EPSILON; //popular choices include 1e-2 or GSL_DBL_EPSILON.
double  mu          = gsl_vector_get(beta, 0);
    if (mu > limit) 
        return 0;
    //else:
    gsl_vector_set(returned_beta, 0, limit + tolerance);
    return limit - mu;    
}

static double rank_exponential_log_likelihood(const gsl_vector *beta, void *d){
double		bb		    = gsl_vector_get(beta, 0),
		    p,
		    llikelihood = 0,
		    ln_c		= log(bb);
gsl_matrix	*data		= d;
int 		i, k;
	for (k=0; k< data->size2; k++){
		p	= -ln_c - (k)/bb;
		for (i=0; i< data->size1; i++)
			llikelihood	+=  gsl_matrix_get(data, i, k) * p;
	}
	return llikelihood;
}


// The exponential distribution. A one-parameter likelihood fn.
//
//\todo Check that the borderline work here is correct too.
static void rank_exponential_dlog_likelihood(const gsl_vector *beta, void *d, gsl_vector *gradient){
double		bb		        = gsl_vector_get(beta, 0);
int 		i, k;
gsl_matrix	*data		    = d;
double 		d_likelihood 	= 0,
		one_over_ln_c	    = 1/log(bb),
		p;
	for (k=0; k< data->size2; k++) {
		p	= (one_over_ln_c -(k+1)) /bb;
		for (i=0; i< data->size1; i++)			
			d_likelihood	+= gsl_matrix_get(data, i, k) * p; 
	}
	gsl_vector_set(gradient,0, d_likelihood);
}

/* Just a wrapper for gsl_ran_exponential.

   cut & pasted from the GSL documentation:
\f$p(x) dx = {1 \over \mu} \exp(-x/\mu) dx \f$

See the notes for \ref apop_exponential_rng on a popular alternate form.
*/
static double rank_exponential_rng(gsl_rng* r, double * a){
	//This fn exists because the GSL requires a double, 
	//while the apop_model structure requires a double*. 
	return gsl_ran_exponential(r, *a);
}

/** The exponential distribution. A one-parameter likelihood fn.

Right now, it is keyed toward network analysis, meaning that the data
structure requires that the first column be the percentage of observations
which link to the most popular, the second column the percentage of
observations which link to the second-most popular, et cetera.


\f$Z(\mu,k) 	= 1/\mu e^{-k/\mu} 			\f$ <br>
\f$ln Z(\mu,k) 	= -\ln(\mu) - k/\mu			\f$ <br>

Some folks write the function as:
\f$Z(C,k) dx = \ln C C^{-k}. \f$
If you prefer this form, just convert your parameter via \f$\mu = {1\over
\ln C}\f$ (and convert back from the parameters this function gives you
via \f$C=\exp(1/\mu)\f$.

apop_exponential.estimate() is an MLE, so feed it appropriate \ref apop_estimation_params.

\ingroup models
\todo Check that the borderline work here is correct.
\todo Write a second object for the plain old not-network data Exponential.
*/
apop_model apop_exponential_rank = {"Exponential, rank data", 1,
	{
	1,	//parameters
	1,	//covariance
	1,	//confidence
	0,	//predicted
	0,	//residuals
	1,	//log_likelihood
	0	//names;
    },
	 exponential_rank_estimate, rank_exponential_log_likelihood, rank_exponential_dlog_likelihood, NULL,   
    {1, {beta_greater_than_x_constraint}}, rank_exponential_rng};
