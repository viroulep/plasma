/**
 *
 * @file
 *
 *  PLASMA is a software package provided by:
 *  University of Tennessee, US,
 *  University of Manchester, UK.
 *
 * @generated from compute/pzge2desc.c, normal z -> d, Mon Feb  6 14:06:13 2017
 *
 **/

#include "plasma_async.h"
#include "plasma_context.h"
#include "plasma_descriptor.h"
#include "plasma_internal.h"
#include "plasma_types.h"
#include "plasma_workspace.h"
#include "core_blas.h"

/******************************************************************************/
void plasma_pdge2desc(double *pA, int lda,
                      plasma_desc_t A,
                      plasma_sequence_t *sequence,
                      plasma_request_t *request)
{
    // Return if failed sequence.
    if (sequence->status != PlasmaSuccess)
        return;

    double *f77;
    double *bdl;

    int x1, y1;
    int x2, y2;
    int n, m, ldt;

    for (m = 0; m < A.mt; m++) {
        ldt = plasma_tile_mmain(A, m);
        for (n = 0; n < A.nt; n++) {
            x1 = n == 0 ? A.j%A.nb : 0;
            y1 = m == 0 ? A.i%A.mb : 0;
            x2 = n == A.nt-1 ? (A.j+A.n-1)%A.nb+1 : A.nb;
            y2 = m == A.mt-1 ? (A.i+A.m-1)%A.mb+1 : A.mb;

            f77 = &pA[(size_t)A.nb*lda*n + (size_t)A.mb*m];
            bdl = (double*)plasma_tile_addr(A, m, n);

            core_omp_dlacpy(PlasmaGeneral,
                            y2-y1, x2-x1,
                            &(f77[x1*lda+y1]), lda,
                            &(bdl[x1*A.nb+y1]), ldt,
                            sequence, request);
        }
    }
}