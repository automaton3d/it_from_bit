/*
 * help.cpp (adaptado)
 */

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
// windows.h first: ShellExecuteA lives in shellapi.h and windows.h must not be
// dragged in after glad/GLFW (it redefines APIENTRY/WINGDIAPI).
#  include <windows.h>
#  include <shellapi.h>
#endif

#include "globals.h"
#include "help.h"

namespace framework
{
  // ------------------------------------------------------------------------
  // Help link
  // ------------------------------------------------------------------------

  // The `Help` link of the splash screen and the hyperlink of the HUD point
  // here: the repository of this tree, branch `main`, and the README as the
  // help page.  They used to call system("start ...") with
  // https://github.com/automaton3d/automaton/blob/master/help.md -- another
  // repository, another branch, and a file that does not exist there.
  const char* const kHelpUrl =
    "https://github.com/automaton3d/it_from_bit/blob/main/README.md";

  void openHelpPage()
  {
#ifdef _WIN32
    // ShellExecuteA returns a value <= 32 when it fails; report it instead of
    // starting a shell and losing the exit status.
    HINSTANCE result = ShellExecuteA(nullptr, "open", kHelpUrl,
                                     nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
      std::cerr << "openHelpPage: ShellExecuteA failed (code "
                << reinterpret_cast<INT_PTR>(result) << ")\n";
    }
#else
    const std::string cmd = std::string("xdg-open \"") + kHelpUrl + "\" >/dev/null 2>&1 &";
    if (std::system(cmd.c_str()) != 0)
      std::cerr << "openHelpPage: could not launch a browser\n";
#endif
  }

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
