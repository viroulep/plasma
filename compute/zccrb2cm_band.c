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
#include "plasma_z.h"
#include "plasma_types.h"

/***************************************************************************//**
    @ingroup plasma_ccrb2cm

    Convert tiled (CCRB) to column-major (CM) layout for a band matrix.
    Out-of-place.
*/
void PLASMA_zccrb2cm_band_Async(plasma_enum_t uplo,
                                plasma_desc_t *A, plasma_complex64_t *Af77, int lda,
                                plasma_sequence_t *sequence, plasma_request_t *request)
{
    // Get PLASMA context.
    plasma_context_t *plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_error("PLASMA not initialized");
        plasma_request_fail(sequence, request, PlasmaErrorIllegalValue);
        return;
    }

    // Check input arguments.
    if (plasma_desc_band_check(uplo, A) != PlasmaSuccess) {
        plasma_error("invalid A");
        plasma_request_fail(sequence, request, PlasmaErrorIllegalValue);
        return;
    }
    if (Af77 == NULL) {
        plasma_error("NULL A");
        plasma_request_fail(sequence, request, PlasmaErrorIllegalValue);
        return;
    }
    if (sequence == NULL) {
        plasma_error("NULL sequence");
        plasma_request_fail(sequence, request, PlasmaErrorIllegalValue);
        return;
    }
    if (request == NULL) {
        plasma_error("NULL request");
        plasma_request_fail(sequence, request, PlasmaErrorIllegalValue);
        return;
    }

    // Check sequence status.
    if (sequence->status != PlasmaSuccess) {
        plasma_request_fail(sequence, request, PlasmaErrorSequence);
        return;
    }

    // quick return with success
    if (A->m == 0 || A->n == 0)
        return;

    // Call the parallel function.
    plasma_pzooccrb2cm_band(uplo, *A, Af77, lda, sequence, request);

    return;
}
