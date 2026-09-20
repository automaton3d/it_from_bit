/*
 * odr_gate_tu1.cpp - ODR/link gate, translation unit 1 of 2.
 *
 * Part of the `check-odr` make target (see Makefile).  Both gate TUs include
 * the shared headers below; tu1 defines main(), tu2 defines one anchor
 * function.  If any included header DEFINES a function or variable with
 * external linkage without "inline", linking the two objects fails with
 * LNK2005/LNK1169 - the exact regression of 2026-09-17, when isqrt lost its
 * `inline` in model/simulation.h (commit 3cc70c4) and the ~20 translation
 * units that include the header could no longer be linked.
 *
 * Extend the include list below as more shared headers are added; the gate
 * is only as good as its coverage.
 */
#include "model/simulation.h"
#include "model/polarization.h"
#include "model/island_identity.h"
#include "model/wavefront.h"
#include "model/geometry.h"
#include "model/attractor.h"
#include "config.h"

int tu2_anchor();   // defined in odr_gate_tu2.cpp

int main()
{
  // Touch symbols from two of the included headers so nothing is dead-stripped.
  return (isqrt(9) == 3 && tu2_anchor() == 7) ? 0 : 1;
}
