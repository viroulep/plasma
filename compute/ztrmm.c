/**
 *
 * @file ztrmm.c
 *
 *  PLASMA computational routines
 *  PLASMA is a software package provided by Univ. of Tennessee,
 *  Univ. of Manchester, Univ. of California Berkeley and Univ. of Colorado Denver
 *
 * @version 3.0.0
 * @author  Mathieu Faverge
 * @author  Maksims Abalenkovs
 * @date    2016-06-22
 * @precisions normal z -> s d c
 *
 **/

#include "plasma_types.h"
#include "plasma_async.h"
#include "plasma_context.h"
#include "plasma_descriptor.h"
#include "plasma_internal.h"
#include "plasma_z.h"

/***************************************************************************//**
 *
 * @ingroup plasma_trmm
 *
 *  Performs a triangular matrix multiply
 *
 *          \f[B = \alpha [op(A) \times B] \f], if side = PlasmaLeft  or
 *          \f[B = \alpha [B \times op(A)] \f], if side = PlasmaRight
 *  
 *  where op( X ) is one of:
 *
 *          - op(A) = A   or
 *          - op(A) = A^T or
 *          - op(A) = A^H
 *
 *  alpha is a scalar, A and B are matrices with op(A) an n by k matrix and B a
 *  k by n matrix.
 *
 *******************************************************************************
 *
 * @param[in] side
 *          Specifies whether A appears on the left or on the right of X:
 *          - PlasmaLeft:  alpha*op( A )*B
 *          - PlasmaRight: alpha*B*op( A )
 *
 * @param[in] uplo
 *          Specifies whether the matrix A is upper triangular or lower triangular:
 *          - PlasmaUpper: Upper triangle of A is stored;
 *          - PlasmaLower: Lower triangle of A is stored.
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          - PlasmaNoTrans:   A is transposed;
 *          - PlasmaTrans:     A is not transposed;
 *          - PlasmaConjTrans: A is conjugate transposed.
 *
 * @param[in] diag
 *          Specifies whether or not A is unit triangular:
 *          - PlasmaNonUnit: A is non unit;
 *          - PlasmaUnit:    A us unit.
 *
 * @param[in] n
 *          The order of the matrix A. n >= 0.
 *
 * @param[in] nrhs
 *          The number of right hand sides, i.e., the number of columns of the matrix B. nrhs >= 0.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha.
 *
 * @param[in] A
 *          The triangular matrix A. If uplo = PlasmaUpper, the leading n-by-n upper triangular
 *          part of the array A contains the upper triangular matrix, and the strictly lower
 *          triangular part of A is not referenced. If uplo = PlasmaLower, the leading n-by-n
 *          lower triangular part of the array A contains the lower triangular matrix, and the
 *          strictly upper triangular part of A is not referenced. If diag = PlasmaUnit, the
 *          diagonal elements of A are also not referenced and are assumed to be 1.
 *
 * @param[in] lda
 *          The leading dimension of the array A. lda >= max(1,n).
 *
 * @param[in,out] B
 *          On entry, the n-by-nrhs right hand side matrix B.
 *          On exit, if return value = 0, the n-by-nrhs solution matrix X.
 *
 * @param[in] ldb
 *          The leading dimension of the array B. ldb >= max(1,n).
 *
 *******************************************************************************
 *
 * @retval PLASMA_SUCCESS successful exit
 * @retval <0 if -i, the i-th argument had an illegal value
 *
 *******************************************************************************
 *
 * @sa PLASMA_ztrmm_Tile_Async
 * @sa PLASMA_ctrmm
 * @sa PLASMA_dtrmm
 * @sa PLASMA_strmm
 *
 ******************************************************************************/
int PLASMA_ztrmm(PLASMA_enum side, PLASMA_enum uplo,
                 PLASMA_enum transA, PLASMA_enum diag,
                 int n, int nrhs, PLASMA_Complex64_t alpha,
                 PLASMA_Complex64_t *A, int lda,
                 PLASMA_Complex64_t *B, int ldb)
{
    int retval;
    int nb, na;
    int status;
    plasma_context_t *plasma;
    PLASMA_sequence *sequence = NULL;
    PLASMA_request request = PLASMA_REQUEST_INITIALIZER;
    PLASMA_desc descA, descB;

    /* Get PLASMA context */
    plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_error("PLASMA not initialized");
        return PLASMA_ERR_NOT_INITIALIZED;
    }

    if (side == PlasmaLeft) {
      na = n;
    } else {
      na = nrhs;
    }

    /* Check input arguments */
    if (side != PlasmaLeft && side != PlasmaRight) {
        plasma_error("illegal value of side");
        return -1;
    }
    if (uplo != PlasmaUpper && uplo != PlasmaLower) {
        plasma_error("illegal value of uplo");
        return -2;
    }
    if (transA != PlasmaConjTrans &&
        transA != PlasmaNoTrans   &&
        transA != PlasmaTrans )
    {
        plasma_error("illegal value of transA");
        return -3;
    }
    if (diag != PlasmaUnit && diag != PlasmaNonUnit) {
        plasma_error("illegal value of diag");
        return -4;
    }
    if (n < 0) {
        plasma_error("illegal value of N");
        return -5;
    }
    if (nrhs < 0) {
        plasma_error("illegal value of NRHS");
        return -6;
    }
    if (lda < imax(1, na)) {
        plasma_error("illegal value of LDA");
        return -8;
    }
    if (ldb < imax(1, n)) {
        plasma_error("illegal value of LDB");
        return -10;
    }

    /* Quick return */
    if (imin(n, nrhs) == 0)
        return PLASMA_SUCCESS;

    /* Tune nb depending on m, n & nrhs; Set nbnb */
    /*
    if (plasma_tune(PLASMA_FUNC_ZPOSV, N, N, NRHS) != PLASMA_SUCCESS) {
        plasma_error("plasma_tune() failed");
        return status;
    }
    */
    nb = plasma->nb;

    /* Initialise matrix descriptors */
    descA = plasma_desc_init(PlasmaComplexDouble, nb, nb,
                             nb*nb, na, n,  0, 0, na, n);

    descB = plasma_desc_init(PlasmaComplexDouble, nb, nb,
                             nb*nb, n,  na, 0, 0, n,  na);

    /* Allocae matrices in tile layout */
    retval = plasma_desc_mat_alloc(&descA);
    if (retval != PLASMA_SUCCESS) {
        plasma_error("plasma_desc_mat_alloc() failed");
        return retval;
    }
    retval = plasma_desc_mat_alloc(&descB);
    if (retval != PLASMA_SUCCESS) {
        plasma_error("plasma_desc_mat_alloc() failed");
        plasma_desc_mat_free(&descA);
        return retval;
    }

    /* Create sequence */
    retval = plasma_sequence_create(&sequence);
    if (retval != PLASMA_SUCCESS) {
        plasma_error("plasma_sequence_create() failed");
        return retval;
    }

#pragma omp parallel
#pragma omp master
    {
        /*
         * the Async functions are submitted here.  If an error occurs
         * (at submission time or at run time) the sequence->status
         * will be marked with an error.  After an error, the next
         * Async will not _insert_ more tasks into the runtime.  The
         * sequence->status can be checked after each call to _Async
         * or at the end of the parallel region.
         */

        /* Translate matrices to tile layout */
        PLASMA_zcm2ccrb_Async(A, lda, &descA, sequence, &request);

        if (sequence->status == PLASMA_SUCCESS)
            PLASMA_zcm2ccrb_Async(B, ldb, &descB, sequence, &request);

        /* Call the tile async interface */
        if (sequence->status == PLASMA_SUCCESS) {
            
            PLASMA_ztrmm_Tile_Async(side, uplo, transA, diag, alpha,
                                    &descA, &descB, sequence, &request);
        }

        /* Revert matrices to LAPACK layout */
        if (sequence->status == PLASMA_SUCCESS)
            PLASMA_zccrb2cm_Async(&descA, A, lda, sequence, &request);

        if (sequence->status == PLASMA_SUCCESS)
            PLASMA_zccrb2cm_Async(&descB, B, ldb, sequence, &request);

    } /* pragma omp parallel block closed */

    /* Check for errors in async execution */
    if (sequence->status != PLASMA_SUCCESS)
        return sequence->status;

    /* Free matrices in tile layout */
    plasma_desc_mat_free(&descA);
    plasma_desc_mat_free(&descB);

    /* Return status */
    status = sequence->status;
    plasma_sequence_destroy(sequence);
    return status;
}


/***************************************************************************//**
 *
 * @ingroup PLASMA_trmm
 *
 *  Performs triangular matrix multiplication. Non-blocking tile version of
 *  PLASMA_ztrmm(). May return before the computation is finished. Operates on
 *  matrices stored by tiles. All matrices are passed through descriptors. All
 *  dimensions are taken from the descriptors. Allows for pipelining of
 *  operations at runtime.
 *
 *******************************************************************************
 *
 * @param[in] side
 *          Specifies whether A appears on the left or on the right of X:
 *          - PlasmaLeft:  alpha*op( A )*B
 *          - PlasmaRight: alpha*B*op( A )
 *
 * @param[in] uplo
 *          Specifies whether the matrix A is upper triangular or lower triangular:
 *          - PlasmaUpper: Upper triangle of A is stored;
 *          - PlasmaLower: Lower triangle of A is stored.
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          - PlasmaNoTrans:   A is transposed;
 *          - PlasmaTrans:     A is not transposed;
 *          - PlasmaConjTrans: A is conjugate transposed.
 *
 * @param[in] diag
 *          Specifies whether or not A is unit triangular:
 *          - PlasmaNonUnit: A is non unit;
 *          - PlasmaUnit:    A us unit.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha.
 *
 * @param[in] A
 *          Descriptor of the triangular matrix A. If uplo = PlasmaUpper, the
 *          leading n-by-n upper triangular part of the array A contains the
 *          upper triangular matrix, and the strictly lower triangular part of
 *          A is not referenced. If uplo = PlasmaLower, the leading n-by-n
 *          lower triangular part of the array A contains the lower triangular
 *          matrix, and the strictly upper triangular part of A is not
 *          referenced. If diag = PlasmaUnit, the diagonal elements of A are
 *          also not referenced and are assumed to be 1.
 *
 * @param[in,out] B
 *          Descriptor of matrix B. On entry, the n-by-nrhs right hand side
 *          matrix B.  On exit, if return value = 0, the n-by-nrhs solution
 *          matrix X.
 *
 * @param[in] sequence
 *          Identifies the sequence of function calls that this call belongs to
 *          (for completion checks and exception handling purposes).
 *
 * @param[out] request
 *          Identifies this function call (for exception handling purposes).
 *
 * @retval void
 *          Errors are returned by setting sequence->status and
 *          request->status to error values.  The sequence->status and
 *          request->status should never be set to PLASMA_SUCCESS (the
 *          initial values) since another async call may be setting a
 *          failure value at the same time.
 *
 *******************************************************************************
 *
 * @sa PLASMA_ztrmm
 * @sa PLASMA_ctrmm_Tile_Async
 * @sa PLASMA_dtrmm_Tile_Async
 * @sa PLASMA_strmm_Tile_Async
 *
 ******************************************************************************/
void PLASMA_ztrmm_Tile_Async(PLASMA_enum side, PLASMA_enum uplo,
                            PLASMA_enum transA, PLASMA_enum diag,
                            PLASMA_Complex64_t alpha, PLASMA_desc *A, PLASMA_desc *B,
                            PLASMA_sequence *sequence, PLASMA_request *request)
{
    PLASMA_desc descA;
    PLASMA_desc descB;
    plasma_context_t *plasma;

    /* Get PLASMA context */
    plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_error("PLASMA not initialized");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (sequence == NULL) {
        plasma_error("NULL sequence");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (request == NULL) {
        plasma_error("NULL request");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    /* Check sequence status */
    if (sequence->status != PLASMA_SUCCESS) {
        plasma_request_fail(sequence, request, PLASMA_ERR_SEQUENCE_FLUSHED);
        return;
    }

    /* Check descriptors for correctness */
    if (plasma_desc_check(A) != PLASMA_SUCCESS) {
        plasma_error("invalid first descriptor");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    } else {
        descA = *A;
    }

    if (plasma_desc_check(B) != PLASMA_SUCCESS) {
        plasma_error("invalid second descriptor");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    } else {
        descB = *B;
    }

    /* Check input arguments */
    if (descA.nb != descA.mb || descB.nb != descB.mb) {
        plasma_error("only square tiles supported");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (side != PlasmaLeft && side != PlasmaRight) {
        plasma_error("illegal value of side");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (uplo != PlasmaUpper && uplo != PlasmaLower) {
        plasma_error("illegal value of uplo");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (transA != PlasmaConjTrans && transA != PlasmaNoTrans && transA != PlasmaTrans) {
        plasma_error("illegal value of transA");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    if (diag != PlasmaUnit && diag != PlasmaNonUnit) {
        plasma_error("illegal value of diag");
        plasma_request_fail(sequence, request, PLASMA_ERR_ILLEGAL_VALUE);
        return;
    }

    /* Quick return */
    if (A->m == 0 || A->n == 0 || alpha == 0.0 || B->m == 0 || B->n == 0)
        return;

    /* Call parallel function */
    plasma_pztrmm(side, uplo, transA, diag, alpha,
                  descA, descB, sequence, request);

    return;
}
