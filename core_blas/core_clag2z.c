/**
 *
 * @file
 *
 *  PLASMA is a software package provided by:
 *  University of Tennessee, US,
 *  University of Manchester, UK.
 *
 * @precisions mixed zc -> ds
 *
 **/

#include "core_blas.h"
#include "plasma_types.h"
#include "core_lapack.h"

/***************************************************************************//**
 *
 * @ingroup core_lag2
 *
 *  Converts m-by-n matrix A from single complex to double complex precision.
 *
 *******************************************************************************
 *
 * @param[in] m
 *          The number of rows of the matrix As.
 *          m >= 0.
 *
 * @param[in] n
 *          The number of columns of the matrix As.
 *          n >= 0.
 *
 * @param[in] As
 *          The ldas-by-n matrix in single complex precision to convert.
 *
 * @param[in] ldas
 *          The leading dimension of the matrix As.
 *          ldas >= max(1,m).
 *
 * @param[out] A
 *          On exit, the converted lda-by-n matrix in double complex precision.
 *
 * @param[in] lda
 *          The leading dimension of the matrix A.
 *          lda >= max(1,m).
 *
 ******************************************************************************/
void core_clag2z(int m, int n,
                 plasma_complex32_t *As, int ldas,
                 plasma_complex64_t *A,  int lda,
                 plasma_sequence_t *sequence, plasma_request_t *request)
{
    LAPACKE_clag2z_work(LAPACK_COL_MAJOR, m, n, As, ldas, A, lda);
}

/******************************************************************************/
void core_omp_clag2z(int m, int n,
                     const plasma_complex32_t *As, int ldas,
                           plasma_complex64_t *A,  int lda,
                           plasma_sequence_t *sequence, plasma_request_t *request)
{
    // omp depend assumes ldas == lda == m
    #pragma omp task depend(in:As[0:m*n]) depend(out:A[0:m*n])
    core_zlag2c(m, n, As, ldas, A, lda, sequence, request);
}
