/*
 * odr_gate_tu2.cpp - ODR/link gate, translation unit 2 of 2.
 *
 * Same header set as odr_gate_tu1.cpp; provides the anchor function that
 * forces the linker to combine both objects.  See Makefile target check-odr.
 */
#include "model/simulation.h"
#include "model/polarization.h"
#include "model/island_identity.h"
#include "model/wavefront.h"
#include "model/geometry.h"
#include "model/attractor.h"
#include "config.h"

int tu2_anchor()
{
  return 7;
}
