/*
 * GUI.h
 *
 * Declares the Graphical Unit Interface rendering routines.
 */

#ifndef GUI_H
#define GUI_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "menubar.h"
#include "renderer.h"

#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <atomic>

#include "button.h"

#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "model/simulation.h"
#include "radio.h"
#include "layers.h"
#include "text_renderer.h"
#include "tickbox.h"
#include "progress.h"
#include "globals.h"
#include "app_context.h"

extern AxisProjection gAxisProj[3];
extern bool gAxisProjValid;

namespace automaton
{
  extern unsigned L2;
}

namespace framework
{
  extern const std::vector<std::string> record_help;
  extern const std::vector<std::string> ui_help;

  // Global GUI mode variable (defined once in GUI.cpp)
  extern bool MULTICUBE_MODE;
  extern unsigned tomo_x, tomo_y, tomo_z;

  extern ProgressBar *progress;
  extern bool helpHover;
  extern bool active;

  extern std::vector<Tickbox> data3D;
  extern std::unique_ptr<LayerList> layerList;
  extern std::vector<Tickbox> delays;
  extern std::vector<Radio> views;
  extern std::vector<Radio> projectRads;

  extern unsigned long long replayTimer;
  extern std::atomic<bool> replayFrames;

  // ---------------------------------------------------------------------------
  // Geometry of the two HUD side panels, and of the 3D window they leave between them.
  // Both panels are overlays at the edges of the window (drawn by renderHUD in hud.cpp),
  // so anything that has to live *inside the 3D window* -- the status row and its
  // dividers, the L/W readout -- measures itself from here instead of from the window
  // edges: move a panel and the row follows.
  // ---------------------------------------------------------------------------
  inline constexpr float kPanelTop        = 60.0f;    // inset of both panels from the top
  inline constexpr float kPanelBottomGap  = 170.0f;   // distance of the panels from the bottom
  inline constexpr float kPanelLeftX      = 35.0f;    // left panel: x and width
  inline constexpr float kPanelLeftW      = 170.0f;
  inline constexpr float kPanelRightW     = 250.0f;   // right panel: width, inset from the edge
  inline constexpr float kPanelRightInset = 10.0f;

  // The status row (GUI_2D.cpp): one line of displays between the two side panels, whose
  // fields are partitioned across the 3D window and share one font size.  kStatusLineY is
  // its distance from the *top* edge of the window (the row is fed to the text renderer as
  // `height - kStatusLineY`, because that renderer counts y upwards from the bottom, while
  // the 2D drawing helpers count downwards from the top -- the two spaces are opposite).
  // It lives here so that anything measuring the row -- the build gauge in
  // experiments\row_gauge.cpp, for instance -- uses the same number.
  inline constexpr float kStatusLineY = 80.0f;

  void init();
  void resize(int width, int height);
  void clearVoxels();
  // initMenuDropdowns() and handleMenuSelection() were declared here but never
  // defined and never called; the first one went away with the Dropdown widget
  // (Cortina is the only combo box in the project now).

  // Internal rendering methods
  void renderAxes();
  void renderGizmo();
  void renderCenter();
  void renderClear();
  void renderCavity();
  void renderLattice();
  void renderGrid();
  void render3DObjects();
  void renderWavefront();
  void renderCounts();
  void renderMomentum(AppContext& ctx);
  void renderSpin();
  void renderSineMask();
  void renderVisitedMask();
  void renderHunting();
  void renderCenters();
  void renderUI();
  void renderElapsedTime();
  void renderStatusRow();
  void renderLayers();
  void renderHelpText();
  void renderSliders();
  void renderTomoControls();
  void renderSineVisitedToggle(int screenW, int screenH);
  void renderRecordToggle(int screenW, int screenH);
  void renderPauseOverlay();
  void renderSectionLabels();
  void render3Dboxes();
  void renderDelays();
  void renderScenarioHelpToggle();
  void renderViewpointRadios();
  void renderProjectionRadios();
  void renderTomoRadios();
  void renderHyperlink();
  void renderScenarioHelpPane();

  void drawPanel(float x, float y, float w, float h,
                 const glm::vec3& bgColor = glm::vec3(0.05f, 0.05f, 0.1f),
                 const glm::vec3& borderColor = glm::vec3(0.3f, 0.3f, 0.8f),
                 float borderThickness = 2.0f,
                 const glm::mat4& proj = ProjectionManager::instance().get2DOrtho());

  void drawRoundedRect(float x, float y, float w, float h, float radius);

  void drawRoundedRectOutline(float x, float y, float w, float h, float radius);

  void renderExitDialog();

  void enhanceVoxel();

  // Helper methods
  inline bool isVoxelVisible(unsigned x, unsigned y, unsigned z)
  {
      if (x >= automaton::EL) return false;
      if (y >= automaton::W_USED) return false;
      if (z >= automaton::L2) return false;
      return true;
  }

  extern glm::mat4 mProjection_;
  extern glm::mat4 mOrtho;
  extern glm::mat4 mPerspective;

  extern std::vector<std::array<unsigned, 3>> lastPositions_;
  extern std::vector<std::array<float, 2>> screenPositions_;
  extern std::map<std::pair<int,int>, int> counts_;

  // HUD text renderer
  extern TextRenderer hudText;

  void initializeWidgets(AppContext& ctx);

  void promptExit();

} // namespace framework

// ============================================================================
// Tomography
// ============================================================================

namespace tomography {

    void init();

    void cleanup();

    void update();

    void requestUpdate();

    float getSlicePosition();

    void setSlicePosition(float pos);

    bool isVoxelVisible(unsigned x,
                        unsigned y,
                        unsigned z);

    void renderTomoPlane();

    void renderControls();
}

#endif // GUI_H