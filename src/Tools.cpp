#include <Rcpp.h>
#ifdef _OPENMP
#include <omp.h>
#endif
using namespace Rcpp;
using namespace std;

// [[Rcpp::export]]
List normalize(NumericMatrix matrix, const int nthreads = 1)
{
  int ncol = matrix.ncol();
  int nrow = matrix.nrow();
  NumericVector col_sum(ncol);
  NumericVector col_sqr_sum(ncol);
  IntegerVector col_nnas(ncol);
  col_nnas.fill(nrow);

  NumericMatrix::iterator p;
  double *sum_, *sqr_sum; int *nna; int j;
  #pragma omp parallel num_threads(nthreads) private(p, sum_, sqr_sum, nna)
  {
    #pragma omp for
    for (int i = 0; i < ncol; i++)
    {
      sum_    = &(col_sum[i]);
      sqr_sum = &(col_sqr_sum[i]);
      nna     = &(col_nnas[i]);
      for (p = matrix.begin() + i * nrow; p < matrix.begin() + i * nrow + nrow; ++p)
      {
        if (internal::Rcpp_IsNA(*p) || internal::Rcpp_IsNaN(*p)) {
          (*nna) --;
          continue;
        }
        *sum_ += *p;
        *sqr_sum += *p * *p;
      }
    }

    #pragma omp single
    {
      for (j = 0; j < ncol; j++)
      {
        if (col_nnas[j] <= 0) {
          col_sqr_sum[j] = NA_REAL;
          col_sum[j]     = NA_REAL;
        } else {
          col_sqr_sum[j] = col_nnas[j] == 1 ? 0:(sqrt((col_sqr_sum[j] - col_sum[j] * col_sum[j] / col_nnas[j]) / (col_nnas[j] - 1))); // std
          col_sum[j] /= col_nnas[j]; // mean
        }
      }
    }

    #pragma omp for
    for (int i = 0; i < ncol; i++)
    {
      sum_ = &(col_sum[i]);
      sqr_sum = &(col_sqr_sum[i]);
      if (*sqr_sum == 0 || internal::Rcpp_IsNA(*sqr_sum)) {
        for (p = matrix.begin() + i * nrow; p < matrix.begin() + i * nrow + nrow; ++p)
        {
          *p = *sqr_sum;
        }
      } else {
        for (p = matrix.begin() + i * nrow; p < matrix.begin() + i * nrow + nrow; ++p)
        {
          *p -= *sum_;
          *p /= *sqr_sum;
        }
      }
    }
  }

  return List::create(
    _["center"] = col_sum,
    _["scale"]  = col_sqr_sum
  );
}



// [[Rcpp::export]]
void normalize1(NumericMatrix matrix, List scales, const int nthreads = 1)
{
  int ncol = matrix.ncol();
  int nrow = matrix.nrow();
  NumericVector center = as<NumericVector>(scales["center"]);
  NumericVector scale = as<NumericVector>(scales["scale"]);

  NumericMatrix::iterator p;
  double *sum_, *sqr_sum;
  #pragma omp parallel for num_threads(nthreads) private(p, sum_, sqr_sum)
  for (int i = 0; i < ncol; i++)
  {
    sum_ = &(center[i]);
    sqr_sum = &(scale[i]);
    if (*sqr_sum == 0) {
      for (p = matrix.begin() + i * nrow; p < matrix.begin() + i * nrow + nrow; ++p)
      {
        *p = 0;
      }
    } else {
      for (p = matrix.begin() + i * nrow; p < matrix.begin() + i * nrow + nrow; ++p)
      {
        *p -= *sum_;
        *p /= *sqr_sum;
      }
    }
  }
}


// [[Rcpp::export]]
NumericVector NApredict(NumericVector pred, IntegerVector nas)
{
  NumericVector napred(pred.size()+nas.size());
  napred.fill(NA_REAL);

  NumericVector::iterator pp = pred.begin();
  IntegerVector::iterator pn = nas.begin();

  for (int i = 0; i < napred.size(); i++)
  {
    if (i + 1 == *pn) {
      pn ++;
      continue;
    }
    napred[i] = *pp;
    pp ++;
  }

  return(napred);
}
