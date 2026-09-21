/*
 * subregion_modal.h
 *
 * Full-window overlay that edits the region of the L x L x L lattice.
 *
 * It shows only what the choice needs: the lattice grid, the box being selected
 * (its six faces are the gizmo) and the readouts of what is selected.  There are
 * no buttons -- Enter or Esc closes it and gives control back to the setup
 * screen, because the region is edited live and the splash shows and prints the
 * result as soon as it returns.
 *
 * Reuses the camera of the main scene (OrbitCamera) and the project's 3D colour
 * shader, so it looks and orbits like the program's own 3D view.  The data stays
 * in SubRegionBox, which knows nothing about GL.
 */

#ifndef SUBREGION_MODAL_H_
#define SUBREGION_MODAL_H_

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

#include "camera.h"
#include "subregion_box.h"

class TextRenderer;

class SubRegionModal
{
public:
    // What an input call did, so the caller can re-print the region.
    struct Result
    {
        bool consumed = false;   // the overlay took the event
        bool changed  = false;   // the region changed
    };

    SubRegionModal() = default;
    ~SubRegionModal();

    bool isOpen() const { return open_; }
    // The model must outlive the overlay.  The window size is needed from the
    // start, because the input handlers project with it.
    void open(SubRegionBox* model, int winW, int winH);
    void close() { open_ = false; grabbed_ = -1; orbiting_ = false; panning_ = false; }

    // Where the centre of face `index` (SubRegionBox::Handle order) is on screen,
    // in the same window pixels the input calls take.  Public so a caller can
    // hit-test or annotate around a face.
    glm::vec2 handleScreenPos(int index) const;

    // Winding layers and sizeof(Cell) of the run being prepared: the overlay
    // shows the totals they imply, without depending on the model headers.
    void setRunContext(int W, unsigned long long cellBytes)
    {
        layers_    = (W > 0) ? W : 1;
        cellBytes_ = cellBytes;
    }

    // Mouse, in window pixels with GLFW's y (top-down), as the callbacks have
    // them.  button: 0 = left, 2 = middle.
    Result onMousePress(float mx, float my, int button);
    Result onMouseMove(float mx, float my);
    Result onMouseRelease();
    Result onScroll(float dy);
    // Esc / Enter / keypad-Enter close the overlay; arrows edit the region.
    Result onKey(int key, int mods);

    void render(TextRenderer* textRenderer, int winW, int winH);

private:
    // The 3D pass draws into this rect of the window (top-down pixels).
    void viewportRect(int winW, int winH, int& x, int& y, int& w, int& h) const;

    glm::mat4 viewMatrix() const;
    glm::mat4 projMatrix(int winW, int winH) const;
    glm::mat4 viewProj(int winW, int winH) const;

    // Lattice coordinates (0..L, on the cell boundaries) <-> world space, which
    // is the unit cube centred on the origin.
    glm::vec3 worldOf(const glm::vec3& latticeCoord) const;
    float     latticeSize() const;

    glm::vec2 screenOf(const glm::vec3& latticeCoord, int winW, int winH) const;
    bool      rayOf(float mx, float my, int winW, int winH,
                    glm::vec3& origin, glm::vec3& dir) const;
    int       handleAtScreen(float mx, float my, int winW, int winH) const;
    bool      dragFaceTo(float mx, float my, int winW, int winH);

    void rebuildGrid();
    void drawText2D(TextRenderer* textRenderer, const std::string& text,
                    float x, float yCenter, float scale,
                    const glm::vec3& color, int winW, int winH) const;

    SubRegionBox* model_ = nullptr;
    bool open_ = false;
    int  winW_ = 0, winH_ = 0;   // last window size seen by render(): the input
                                 // handlers project with it
    int  layers_ = 10;           // W of the run being prepared (totals on screen)
    unsigned long long cellBytes_ = 0;

    OrbitCamera camera_;

    // GL objects, created on first use.
    GLuint vao_ = 0, vbo_ = 0;
    GLuint alphaProgram_ = 0;
    GLint  alphaMvpLoc_ = -1, alphaColorLoc_ = -1, alphaAlphaLoc_ = -1;

    int lattice_ = 0;                       // the L the grid was built for
    std::vector<glm::vec3> edges_;          // the lattice cube's 12 edges
    std::vector<glm::vec3> faceGrid_[6];    // the grid on each face (x-, x+, y-, y+, z-, z+)

    int       grabbed_ = -1;                // face being dragged, or -1
    int       hovered_ = -1;
    bool      orbiting_ = false;
    bool      panning_ = false;
    float     lastMx_ = 0.0f, lastMy_ = 0.0f;
};

#endif /* SUBREGION_MODAL_H_ */
