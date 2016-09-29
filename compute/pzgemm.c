/**
 *
 * @file
 *
 *  PLASMA is a software package provided by:
 *  University of Tennessee, US,
 *  University of Manchester, UK.
 *
 * @precisions normal z -> s d c
 *
 **/

#include "plasma_async.h"
#include "plasma_context.h"
#include "plasma_descriptor.h"
#include "plasma_internal.h"
#include "plasma_types.h"
#include "plasma_workspace.h"
#include "core_blas.h"

#define A(m, n) (plasma_complex64_t*)plasma_tile_addr(A, m, n)
#define B(m, n) (plasma_complex64_t*)plasma_tile_addr(B, m, n)
#define C(m, n) (plasma_complex64_t*)plasma_tile_addr(C, m, n)

/***************************************************************************//**
 * Parallel tile matrix-matrix multiplication.
 * @see plasma_omp_zgemm
 ******************************************************************************/
void plasma_pzgemm(plasma_enum_t transA, plasma_enum_t transB,
                   plasma_complex64_t alpha, plasma_desc_t A,
                                             plasma_desc_t B,
                   plasma_complex64_t beta,  plasma_desc_t C,
                   plasma_sequence_t *sequence, plasma_request_t *request)
{
    // Check sequence status.
    if (sequence->status != PlasmaSuccess) {
        plasma_request_fail(sequence, request, PlasmaErrorSequence);
        return;
    }

    for (int m = 0; m < C.mt; m++) {
        int mvcm = plasma_tile_mview(C, m);
        int ldcm = plasma_tile_mmain(C, m);
        for (int n = 0; n < C.nt; n++) {
            int nvcn = plasma_tile_nview(C, n);
            //=========================================
            // alpha*A*B does not contribute; scale C
            //=========================================
            int inner_k = transA == PlasmaNoTrans ? A.n : A.m;
            if (alpha == 0.0 || inner_k == 0) {
                int ldam = imax(1, plasma_tile_mmain(A, 0));
                int ldbk = imax(1, plasma_tile_mmain(B, 0));
                core_omp_zgemm(
                    transA, transB,
                    mvcm, nvcn, 0,
                    alpha, A(0, 0), ldam,
                           B(0, 0), ldbk,
                    beta,  C(m, n), ldcm);
            }
            else if (transA == PlasmaNoTrans) {
                int ldam = plasma_tile_mmain(A, m);
                //================================
                // PlasmaNoTrans / PlasmaNoTrans
                //================================
                if (transB == PlasmaNoTrans) {
                    for (int k = 0; k < A.nt; k++) {
                        int nvak = plasma_tile_nview(A, k);
                        int ldbk   = plasma_tile_mmain(B, k);
                        plasma_complex64_t zbeta = k == 0 ? beta : 1.0;
                        core_omp_zgemm(
                            transA, transB,
                            mvcm, nvcn, nvak,
                            alpha, A(m, k), ldam,
                                   B(k, n), ldbk,
                            zbeta, C(m, n), ldcm);
                    }
                }
                //=====================================
                // PlasmaNoTrans / Plasma[_Conj]Trans
                //=====================================
                else {
                    int ldbn = plasma_tile_mmain(B, n);
                    for (int k = 0; k < A.nt; k++) {
                        int nvak = plasma_tile_nview(A, k);
                        plasma_complex64_t zbeta = k == 0 ? beta : 1.0;
                        core_omp_zgemm(
                            transA, transB,
                            mvcm, nvcn, nvak,
                            alpha, A(m, k), ldam,
                                   B(n, k), ldbn,
                            zbeta, C(m, n), ldcm);
                    }
                }
            }
            else {
                //=====================================
                // Plasma[_Conj]Trans / PlasmaNoTrans
                //=====================================
                if (transB == PlasmaNoTrans) {
                    for (int k = 0; k < A.mt; k++) {
                        int mvak = plasma_tile_mview(A, k);
                        int ldak = plasma_tile_mmain(A, k);
                        int ldbk = plasma_tile_mmain(B, k);
                        plasma_complex64_t zbeta = k == 0 ? beta : 1.0;
                        core_omp_zgemm(
                            transA, transB,
                            mvcm, nvcn, mvak,
                            alpha, A(k, m), ldak,
                                   B(k, n), ldbk,
                            zbeta, C(m, n), ldcm);
                    }
                }
                //==========================================
                // Plasma[_Conj]Trans / Plasma[_Conj]Trans
                //==========================================
                else {
                    int ldbn = plasma_tile_mmain(B, n);
                    for (int k = 0; k < A.mt; k++) {
                        int mvak = plasma_tile_mview(A, k);
                        int ldak = plasma_tile_mmain(A, k);
                        plasma_complex64_t zbeta = k == 0 ? beta : 1.0;
                        core_omp_zgemm(
                            transA, transB,
                            mvcm, nvcn, mvak,
                            alpha, A(k, m), ldak,
                                   B(n, k), ldbn,
                            zbeta, C(m, n), ldcm);
                    }
                }
            }
        }
    }
}