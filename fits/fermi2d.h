
#include <omp.h>

#include "funcs/funcs.h"
#include <iostream>
#include <math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_multimin.h>


#define FIT(i) gsl_vector_get( s->x , i)
#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))

extern bool VERBOSE;
using namespace std;


void fit2dfermi_neldermead (gsl_matrix *m, double *fit);
void fit1dfermi_neldermead (gsl_vector * m, double *fit);
void fit1dfermi_azimuthal_neldermead (gsl_vector ** a, double *fit);
void fit1dfermi_azimuthal_zero_neldermead (gsl_vector ** a, double *fit);


/* Model to evaluate the 2D Fermi
 *
 */
double fermi2d_model( double i, double j, const gsl_vector *v){
  double n0     = gsl_vector_get (v, 0);
  double BetaMu = gsl_vector_get (v, 1);
  double ri     = gsl_vector_get (v, 2);
  double rj     = gsl_vector_get (v, 3);
  double ci     = gsl_vector_get (v, 4);
  double cj     = gsl_vector_get (v, 5);
  double B      = gsl_vector_get (v, 6); 

  return  n0/ f1(BetaMu) * f1( BetaMu - fq(BetaMu) * ( pow ( (j -cj)/rj ,2) + pow( ( i-ci)/ri ,2))) + B; 
}

/* Matrix data for 2D Fermi evaluation
 *
 */
gsl_matrix *fermi2d_eval( const gsl_matrix *d, const double fermi_fit[7]) {
  gsl_matrix *eval = gsl_matrix_alloc( d->size1, d->size2 ); 
  int nparams = 7;
  gsl_vector *v = gsl_vector_alloc( nparams );
  for ( int e=0; e < nparams; e++)  gsl_vector_set ( v, e, fermi_fit[e] ); 
  
  for ( unsigned int i=0; i<d->size1; i++)
  { 
    for ( unsigned int j=0; j<d->size2; j++)
    {
      gsl_matrix_set( eval, i, j, fermi2d_model( i, j, v));
  } 
  }
  return eval; 
}

void make_fermi2d_inspect( gsl_matrix *c, const double fermi2d_fit[6], const char *prefix){
  string datfile (prefix);
  datfile += "_fermi2ddat.ascii"; 
  string fitfile (prefix); 
  fitfile += "_fermi2dfit.ascii";
 
  save_gsl_matrix_ASCII ( c , datfile);  
  gsl_matrix *fit2d = fermi2d_eval (c, fermi2d_fit);
  save_gsl_matrix_ASCII (fit2d, fitfile);
 
  stringstream inspectstr;
  inspectstr << "inspect2d_ascii.py ";
  inspectstr << datfile;
  inspectstr << " ";
  inspectstr << fitfile;
  inspectstr << " ";
  inspectstr << floor (fermi2d_fit[5]);
  inspectstr << " ";
  inspectstr << floor (fermi2d_fit[4]);
  inspectstr << " ";
  inspectstr << prefix;
  inspectstr << "_fermi";
  //cerr << endl << inspectstr.str () << endl;
  system (inspectstr.str ().c_str ());

  remove ( datfile.c_str());
  remove ( fitfile.c_str());
  return; 
} 

void make_fermi2d_gaus2d_inspect( gsl_matrix *c, const double fermi2d_fit[7], const double gaus2d_fit[6], const char *prefix){
  string datfile (prefix);
  datfile += "_fermi2ddat.ascii"; 
  string fitfileFermi (prefix); 
  fitfileFermi += "_fermi2dfit.ascii";
  string fitfileGaus (prefix);
  fitfileGaus += "_gaus2dfit.ascii";
 
  save_gsl_matrix_ASCII ( c , datfile);  
  gsl_matrix *fit2dFermi = fermi2d_eval (c, fermi2d_fit);
  save_gsl_matrix_ASCII (fit2dFermi, fitfileFermi);
  gsl_matrix *fit2dGaus = gaus2d_eval (c, gaus2d_fit);
  save_gsl_matrix_ASCII (fit2dGaus, fitfileGaus);
 
  stringstream inspectstr;
  inspectstr << "inspect2d_ascii_multi.py ";
  inspectstr << datfile;
  inspectstr << ",";
  inspectstr << fitfileFermi;
  inspectstr << ",";
  inspectstr << fitfileGaus;
  inspectstr << " ";
  inspectstr << floor (fermi2d_fit[5]);
  inspectstr << " ";
  inspectstr << floor (fermi2d_fit[4]);
  inspectstr << " ";
  inspectstr << prefix;
  inspectstr << "_fermi";
  //cerr << endl << inspectstr.str () << endl;
  system (inspectstr.str ().c_str ());

  remove ( datfile.c_str());
  remove ( fitfileFermi.c_str());
  remove ( fitfileGaus.c_str());
  return; 
} 

/* Error function for 2D Nelder-Mead
 *
 */ 
double
fermi2d_simplex_f (const gsl_vector * v, void *params)
{

  //benchmark start
//  double start = omp_get_wtime(); 
  
  unsigned int s1 = ((gsl_matrix *) params)->size1;
  unsigned int s2 = ((gsl_matrix *) params)->size2;

  double sumsq = 0.;

  //i is radial === y
  //j is axial  === g

  for (unsigned int i = 0; i < s1; i++)
    {
      for (unsigned int j = 0; j < s2; j++)
	{
          double model = fermi2d_model(i,j,v);
	  double dat = gsl_matrix_get ((gsl_matrix *) params, i, j);
	  sumsq += pow (model - dat, 2);
	}
    }

  //end benchmark
//  double end = omp_get_wtime();
//  printf( "time elapsed in fermi errfunc %.5f\n", end-start); 

  return sumsq;
}
/* Nelder-Mead algorithm 2D Fermi fit
 *
 */ 
void
fit2dfermi_neldermead (gsl_matrix * m, double *fit)
{

  //i is radial === y
  //j is axial  === g

  double n0 = fit[0];
  double BetaMu = fit[1];
  double ri = fit[2];
  double rj = fit[3];
  double ci = fit[4];
  double cj = fit[5];
  double B = fit[6];

  if (VERBOSE)
    printf (" Nelder-Mead 2D Fermi fit started   ==>  BetaMu= %.3f\n",
	    BetaMu);


  const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
  gsl_multimin_fminimizer *s = NULL;
  gsl_vector *ss, *x;
  gsl_multimin_function f;

/*Starting Point */
  x = gsl_vector_alloc (7);
  gsl_vector_set (x, 0, n0);
  gsl_vector_set (x, 1, BetaMu);
  gsl_vector_set (x, 2, ri);
  gsl_vector_set (x, 3, rj);
  gsl_vector_set (x, 4, ci);
  gsl_vector_set (x, 5, cj);
  gsl_vector_set (x, 6, B);


/*Set initial step sizes to 1*/
  ss = gsl_vector_alloc (7);
  gsl_vector_set_all (ss, 1.0);

  f.n = 7;
  f.f = &fermi2d_simplex_f;
  f.params = m;

  s = gsl_multimin_fminimizer_alloc (T, 7);
  gsl_multimin_fminimizer_set (s, &f, x, ss);


  size_t iter = 0;
  int status;
  double size;

  double tolerance = 1e-2;

  do
    {
      iter++;
      status = gsl_multimin_fminimizer_iterate (s);
      if (status)
	break;

      size = gsl_multimin_fminimizer_size (s);
      status = gsl_multimin_test_size (size, tolerance);

      if (status == GSL_SUCCESS && VERBOSE)
	{
	  printf (" converged to minimum at \n");
	}
      if ((VERBOSE) && iter % 20 == 0)
	{
	  printf
	    ("%5d %10.3e %10.3e %10.3e %10.3e %10.3e %10.3e %10.3e f() = %7.3e size = %.3f\n",
	     iter, gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1),
	     gsl_vector_get (s->x, 2), gsl_vector_get (s->x, 3),
	     gsl_vector_get (s->x, 4), gsl_vector_get (s->x, 5),
	     gsl_vector_get (s->x, 6), s->fval, size);
	}
    }
  while (status == GSL_CONTINUE && iter < 1000);

  fit[0] = FIT (0);
  fit[1] = FIT (1);
  fit[2] = FIT (2);
  fit[3] = FIT (3);
  fit[4] = FIT (4);
  fit[5] = FIT (5);
  fit[6] = FIT (6);

  gsl_vector_free (x);
  gsl_vector_free (ss);
  gsl_multimin_fminimizer_free (s);

  return;
}


double
fermi1d_azimuthal_simplex_f (const gsl_vector * v,  void *params)
{


  //benchmark start
//  double start = omp_get_wtime(); 
  
  unsigned int s = (((gsl_vector **) params)[0])->size; 
  

  double n0     = gsl_vector_get (v, 0);
  double BetaMu = gsl_vector_get (v, 1);
  double r     = gsl_vector_get (v, 2);
  double B      = gsl_vector_get (v, 3);
  double mx    = gsl_vector_get(v, 4); 

  double sumsq = 0.;
  
  gsl_vector *d  = ((gsl_vector **)params)[0]; 
  gsl_vector *az = ((gsl_vector **)params)[1]; 

  //i is radial === y
  //j is axial  === g

  for (unsigned int i = 0; i < s; i++){
  //printf("Inisde simplex_f loop\n"); 
          double dist = gsl_vector_get(d,i); 
	  double dat = gsl_vector_get (az, i);
	  double model = n0/ f1(BetaMu) * f1( BetaMu - fq(BetaMu) * ( pow ( dist/r ,2) )) + B + mx*dist; 
	  sumsq += pow (model - dat, 2);
    }

//  end benchmark
//  double end = omp_get_wtime();
//  printf( "time elapsed in fermi errfunc %.5f\n", end-start); 

  return sumsq;
}

double
fermi1d_azimuthal_zero_simplex_f (const gsl_vector * v,  void *params)
{
  //benchmark start
//  double start = omp_get_wtime(); 
  
  unsigned int s = (((gsl_vector **) params)[0])->size; 
  

  double n0     = gsl_vector_get (v, 0);
  double r     = gsl_vector_get (v, 1);
  double B      = gsl_vector_get (v, 2);
  double mx    = gsl_vector_get(v, 3); 

  double sumsq = 0.;
  
  gsl_vector *d  = ((gsl_vector **)params)[0]; 
  gsl_vector *az = ((gsl_vector **)params)[1]; 

  //i is radial === y
  //j is axial  === g

  for (unsigned int i = 0; i < s; i++){
  //printf("Inisde simplex_f loop\n"); 
          double dist = gsl_vector_get(d,i); 
	  double dat = gsl_vector_get (az, i);
          double model = n0 * pow( std::max ( 1. - pow( dist/r,2.) , 0.), 2.) + B + mx*dist;    
	  sumsq += pow (model - dat, 2);
    }

//  end benchmark
//  double end = omp_get_wtime();
//  printf( "time elapsed in fermi errfunc %.5f\n", end-start); 

  return sumsq;
}

double
fermi1d_simplex_f (const gsl_vector *v, void *params)
{
  unsigned int s1 = ((gsl_vector *) params)->size;

  double n0     = gsl_vector_get (v, 0);
  double BetaMu = gsl_vector_get (v, 1);
  double r      = gsl_vector_get (v, 2);
  double c      = gsl_vector_get (v, 3);
  double B      = gsl_vector_get (v, 4); 

  double sumsq = 0.;

  //i is radial === y
  //j is axial  === g

  for (unsigned int i = 0; i < s1; i++)
    {
	  double model = n0/ f32(BetaMu) * f32( BetaMu - fq(BetaMu) *  pow ( (i -c)/r ,2) ) + B ; 
	  double dat = gsl_vector_get ((gsl_vector *) params, i);
	  sumsq += pow (model - dat, 2);
    }

  return sumsq;
}



void
fit1dfermi_neldermead (gsl_vector * m, double *fit)
{

  double n0 = fit[0];
  double BetaMu = fit[1];
  double r = fit[2];
  double c = fit[3];
  double B = fit[4];

  const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
  gsl_multimin_fminimizer *s = NULL;
  gsl_vector *ss, *x;
  gsl_multimin_function f;

/*Starting Point */
  x = gsl_vector_alloc (5);
  gsl_vector_set (x, 0, n0);
  gsl_vector_set (x, 1, BetaMu);
  gsl_vector_set (x, 2, r);
  gsl_vector_set (x, 3, c);
  gsl_vector_set (x, 4, B);


/*Set initial step sizes to 1*/
  ss = gsl_vector_alloc (5);
  gsl_vector_set_all (ss, 1.0);

  f.n = 5;
  f.f = &fermi1d_simplex_f;
  f.params = m;

  s = gsl_multimin_fminimizer_alloc (T, 5);
  gsl_multimin_fminimizer_set (s, &f, x, ss);


  size_t iter = 0;
  int status;
  double size;

  do
    {
      iter++;
      status = gsl_multimin_fminimizer_iterate (s);
      if (status)
	break;

      size = gsl_multimin_fminimizer_size (s);
      status = gsl_multimin_test_size (size, 1e-2);

      if (status == GSL_SUCCESS && VERBOSE)
	{
	  printf (" converged to minimum at \n");
	}

      if (VERBOSE && iter % 4 == 0)
	{
	  printf
	    ("%5d %10.3e %10.3e %10.3e %10.3e %10.3e f() = %7.3e size = %.3f\n",
	     iter, gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1),
	     gsl_vector_get (s->x, 2), gsl_vector_get (s->x, 3),
	     gsl_vector_get (s->x, 4), s->fval, size);
	}
    }
  while (status == GSL_CONTINUE && iter < 1000);

  gsl_vector_free (x);
  gsl_vector_free (ss);
  gsl_multimin_fminimizer_free (s);

  fit[0] = FIT (0);
  fit[1] = FIT (1);
  fit[2] = FIT (2);
  fit[3] = FIT (3);
  fit[4] = FIT (4);
  return;
}

void
fit1dfermi_azimuthal_neldermead (gsl_vector ** a, double *fit)
{

  double n0 = fit[0];
  double BetaMu = fit[1];
  double r = fit[2];
  double B = fit[3];
  double mx = fit[4];

  const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
  gsl_multimin_fminimizer *s = NULL;
  gsl_vector *ss, *x;
  gsl_multimin_function f;

/*Starting Point */
  x = gsl_vector_alloc (5);
  gsl_vector_set (x, 0, n0);
  gsl_vector_set (x, 1, BetaMu);
  gsl_vector_set (x, 2, r);
  gsl_vector_set (x, 3, B);
  gsl_vector_set (x, 4, mx);


/*Set initial step sizes to 1*/
  ss = gsl_vector_alloc (5);
  gsl_vector_set_all (ss, 1.0);

  f.n = 5;
  f.f = &fermi1d_azimuthal_simplex_f;
  f.params = a;


  s = gsl_multimin_fminimizer_alloc (T, 5);

  gsl_multimin_fminimizer_set (s, &f, x, ss);


  size_t iter = 0;
  int status;
  double size;


  do
    {
      iter++;
      status = gsl_multimin_fminimizer_iterate (s);
      if (status)
	break;

      size = gsl_multimin_fminimizer_size (s);
      status = gsl_multimin_test_size (size, 1e-6);

      if (status == GSL_SUCCESS && VERBOSE)
	{
	  printf (" converged to minimum at \n");
	}

      if ((VERBOSE) && iter % 20 == 0)
	{
	  printf
	    ("%5d %10.3e %10.3e %10.3e %10.3e %10.3e f() = %7.3e size = %.5f\n",
	     iter, gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1),
	     gsl_vector_get (s->x, 2), gsl_vector_get (s->x, 3),
	     gsl_vector_get (s->x, 4), s->fval, size);
	}
    }
  while (status == GSL_CONTINUE && iter < 1000);

  gsl_vector_free (x);
  gsl_vector_free (ss);
  gsl_multimin_fminimizer_free (s);

  fit[0] = FIT (0);
  fit[1] = FIT (1);
  fit[2] = FIT (2);
  fit[3] = FIT (3);
  fit[4] = FIT (4);
  return;
}


void
fit1dfermi_azimuthal_zero_neldermead (gsl_vector ** a, double *fit)
{

  double n0 = fit[0];
  double r = fit[1];
  double B = fit[2];
  double mx = fit[3];

  const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
  gsl_multimin_fminimizer *s = NULL;
  gsl_vector *ss, *x;
  gsl_multimin_function f;

/*Starting Point */
  x = gsl_vector_alloc (4);
  gsl_vector_set (x, 0, n0);
  gsl_vector_set (x, 1, r);
  gsl_vector_set (x, 2, B);
  gsl_vector_set (x, 3, mx);


/*Set initial step sizes to 1*/
  ss = gsl_vector_alloc (4);
  gsl_vector_set_all (ss, 1.0);

  f.n = 4;
  f.f = &fermi1d_azimuthal_zero_simplex_f;
  f.params = a;


  s = gsl_multimin_fminimizer_alloc (T, 4);

  gsl_multimin_fminimizer_set (s, &f, x, ss);


  size_t iter = 0;
  int status;
  double size;


  do
    {
      iter++;
      status = gsl_multimin_fminimizer_iterate (s);
      if (status)
	break;

      size = gsl_multimin_fminimizer_size (s);
      status = gsl_multimin_test_size (size, 1e-6);

      if (status == GSL_SUCCESS && VERBOSE)
	{
	  printf (" converged to minimum at \n");
	}

      if ((VERBOSE) && iter % 20 == 0)
	{
	  printf
	    ("%5d %10.3e %10.3e %10.3e %10.3e f() = %7.3e size = %.5f\n",
	     iter, gsl_vector_get (s->x, 0), gsl_vector_get (s->x, 1),
	     gsl_vector_get (s->x, 2), gsl_vector_get (s->x, 3),
	     s->fval, size);
	}
    }
  while (status == GSL_CONTINUE && iter < 1000);

  gsl_vector_free (x);
  gsl_vector_free (ss);
  gsl_multimin_fminimizer_free (s);

  fit[0] = FIT (0);
  fit[1] = FIT (1);
  fit[2] = FIT (2);
  fit[3] = FIT (3);
  return;
}

/*

int
fermi2d_f (const gsl_vector * x, void *data, gsl_vector * f)
{

  // x contains the parameters
  // data  contains the column density matrix
  // f is a vector containing the differences between matrix and fit function

  unsigned int s1 = ((gsl_matrix *) data)->size1;
  unsigned int s2 = ((gsl_matrix *) data)->size2;

  // Trap parameters 
  wx = 2.*M_PI*3800.;
  wy = 2.*M_PI*3800.;
  wz = 2.*M_PI*3800./8.;
  a  = cos(52.5*M_PI/180.);
  b  = sin(52.5*M_PI/180.);

  bx = pow(wx,2)/(1+pow(wx*t,2));
  by = pow(wy,2)/(1+pow(wy*t,2));
  bz = pow(wz,2)/(1+pow(wz*t,2));

  magnif = 3.2;  // um per pixel

  A  = m*( pow(by,0.5) *( pow(a,2)*bx + pow(b,2)*bz - pow(a*b,2)*pow(bz-bx,2)/(pow(b,2)*bx + pow(a,2)*bz)) );
  B1 = m*( pow(a,2)*bx + pow(b,2)*bz - pow(a*b,2)*pow(bz-bx,2)/(pow(b,2)*bx + pow(a,2)*bz));
  B2 = m*( bz );

  double N      = gsl_vector_get(x,0);
  double Beta   = gsl_vector_get(x,1);
  double BetaMu = gsl_vector_get(x,2);
  double cy     = gsl_vector_get(x,3);
  double cg     = gsl_vector_get(x,4);

  size_t ii = 0;

  //i is radial === y
  //j is axial  === g

  for (unsigned int i = 0; i < s1; i++)
    {
      for (unsigned int j = 0; j < s2; j++)
	{
          double model = 
            N*Beta/(2.*M_PI*F2(BetaMu)) * A * F1(BetaMu - Beta/2.* (  B1*pow( (j-cg)*magnif , 2) + B2*pow( (i-cy)*magnif , 2))); 
	  double dat = gsl_matrix_get ((gsl_matrix *) data, i, j);
	  gsl_vector_set (f, ii, (model - dat));
	  ii++;
	}
    }
}


*/
