/*
 * help.cpp (adaptado)
 */

#include <vector>
#include <string>
#include "globals.h"
#include "help.h"

namespace framework
{
  // Texto de ajuda da interface
  const std::vector<std::string> ui_help = {
    "c: Print camera Eye, Center, Up",
    "r: Reset view",
    "t: Toggle Pan / First-Person",
    "x, y, z: Snap camera to axis",
    "Left-Click: Rotate",
    "Middle-Click: Pan or First-Person",
    "Right-Click: Roll",
    "Scroll-Wheel: Dolly (zoom)",
    "G: Toggle wrapping gizmo",
    "P: Pause the simulation",
    "F1: Toggle keyboard shortcuts",
    "Escape: EXIT"
  };

  // Help text for recording/replay
  const std::vector<std::string> record_help = {
    "F5 - record",
    "F6 - replay",
    "F7 - save to file",
    "F8 - load from file"
  };

  // Scenario help texts.
  // The model defines ONE scenario (the full simulation dynamics).  The old
  // debug scenarios of the reference GUI (wrapping / relocate / orphan /
  // contraction / hunting / reissue / dispersion) were scaffolding and have
  // been removed.  The list is kept so that future scenarios can be appended
  // here and in splash::scenarioOptions (splash.cpp).
  std::vector<std::string> scenarioHelpTexts = {
    "Full Simulation\n\n"
    "The single scenario of the model: deterministic, integer-only dynamics\n"
    "on a 3-torus with W winding layers.  Pulsating spherical wavefronts (the\n"
    "light clock), emergent charge and polarization, and the s2B-gated\n"
    "electroweak force.  Includes photon, graviton, neutrino, and boson\n"
    "interactions; tests superposition, annihilation, cohesion, and\n"
    "electroweak forces.\n\n"
    "Every configuration (controlled two-bubble probes, canonical Platonic\n"
    "seeds) is an initial condition within this one scenario.\n\n"
    "The suggested L in this case is 21."
  };
}
