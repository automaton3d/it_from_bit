/*
 * subregion_modal.cpp
 *
 * See subregion_modal.h.  Three things happen in render():
 *   1. the whole window is dimmed with the project's alpha shader (the one
 *      compileTransparentShader() builds, which had no user until now);
 *   2. a 3D pass inside an inset viewport draws the lattice grid, the selected
 *      box (translucent faces + bright edges) and its six face handles, using
 *      the same OrbitCamera and colour shader as the main scene;
 *   3. 2D strips on top carry the readouts and the hints.
 *
 * Picking is done in screen space (the six face centres are projected with the
 * same matrices) and the drag maps the mouse ray onto the face's plane, so the
 * face follows the cursor exactly at any camera angle.
 */

#include "subregion_modal.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "draw_utils.h"
#include "globals.h"                 // colorProgram3D, colorMvpLoc3D, textProgram
#include "projection_manager.h"
#include "shader.h"                  // compileTransparentShader()
#include "text_renderer.h"

namespace {

const float kCameraDistance = 2.4f;
const float kCameraYaw      = -55.0f;
const float kCameraPitch    = 24.0f;
const float kFovDegrees     = 45.0f;
const float kPickRadiusPx   = 18.0f;

// Layout of the overlay: bands top and bottom, margins on the sides.
const int kTopBandPx    = 54;
const int kBottomBandPx = 168;
const int kSideMarginPx = 40;

const glm::vec3 kGridColor  (0.40f, 0.44f, 0.55f);
const glm::vec3 kEdgeColor  (0.16f, 0.58f, 1.00f);
const glm::vec3 kFaceTop    (0.62f, 0.78f, 0.98f);
const glm::vec3 kFaceX      (0.33f, 0.49f, 0.86f);
const glm::vec3 kFaceZ      (0.45f, 0.62f, 0.92f);
const glm::vec3 kHandleOn   (0.98f, 0.62f, 0.12f);
const glm::vec3 kHandleOff  (0.92f, 0.94f, 1.00f);
const glm::vec3 kHandleHover(0.35f, 0.75f, 1.00f);

// One VAO/VBO for every primitive the overlay draws (lines, points and the 2D
// quads of its strips), created on first use.
void ensureBuffers(GLuint& vao, GLuint& vbo)
{
    if (vao) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Refills the shared VBO and draws: every primitive of the overlay is small
// (a grid of lines, twelve edges, six points, a couple of quads), so one buffer
// refilled per call is simpler than caching each set.
void uploadAndDraw(GLenum mode, const std::vector<glm::vec3>& verts)
{
    if (verts.empty()) return;

    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3),
                 verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(mode, 0, (GLsizei)verts.size());
}

} // namespace

SubRegionModal::~SubRegionModal()
{
    if (vbo_)   glDeleteBuffers(1, &vbo_);
    if (vao_)   glDeleteVertexArrays(1, &vao_);
    if (alphaProgram_) glDeleteProgram(alphaProgram_);
}

void SubRegionModal::open(SubRegionBox* model, int winW, int winH)
{
    model_ = model;
    open_  = true;
    winW_  = winW;
    winH_  = winH;

    camera_.target       = glm::vec3(0.0f);
    camera_.distance     = kCameraDistance;
    camera_.yaw          = kCameraYaw;
    camera_.pitch        = kCameraPitch;
    camera_.minDistance  = 1.1f;
    camera_.maxDistance  = 9.0f;
    camera_.Orthographic = false;

    grabbed_  = -1;
    hovered_  = -1;
    orbiting_ = false;
    panning_  = false;
}

void SubRegionModal::rebuildGrid()
{
    lattice_ = model_ ? model_->latticeSize() : 0;

    edges_.clear();
    for (int f = 0; f < 6; ++f) faceGrid_[f].clear();

    if (lattice_ < 1) return;

    const float L = (float)lattice_;

    // The twelve edges of the whole lattice cube.
    for (int i = 0; i < 4; ++i)
    {
        const float a = (i & 1) ? L : 0.0f;
        const float b = (i & 2) ? L : 0.0f;

        edges_.push_back(worldOf(glm::vec3(0.0f, a, b)));
        edges_.push_back(worldOf(glm::vec3(L,    a, b)));

        edges_.push_back(worldOf(glm::vec3(a, 0.0f, b)));
        edges_.push_back(worldOf(glm::vec3(a, L,    b)));

        edges_.push_back(worldOf(glm::vec3(a, b, 0.0f)));
        edges_.push_back(worldOf(glm::vec3(a, b, L)));
    }

    // The grid of each of the six faces.  Only three of them are drawn at a
    // time -- the ones turned towards the camera -- so the lattice reads as a
    // lattice without the inside of the cube becoming a thicket of lines.
    // 0 = x-, 1 = x+, 2 = y-, 3 = y+, 4 = z-, 5 = z+.
    for (int t = 1; t < lattice_; ++t)
    {
        const float c = (float)t;

        for (int side = 0; side < 2; ++side)
        {
            const float x = side ? L : 0.0f;
            const float y = side ? L : 0.0f;
            const float z = side ? L : 0.0f;

            // x constant: the face spans y and z
            faceGrid_[0 + side].push_back(worldOf(glm::vec3(x, c, 0.0f)));
            faceGrid_[0 + side].push_back(worldOf(glm::vec3(x, c, L)));
            faceGrid_[0 + side].push_back(worldOf(glm::vec3(x, 0.0f, c)));
            faceGrid_[0 + side].push_back(worldOf(glm::vec3(x, L,    c)));

            // y constant: the face spans x and z
            faceGrid_[2 + side].push_back(worldOf(glm::vec3(0.0f, y, c)));
            faceGrid_[2 + side].push_back(worldOf(glm::vec3(L,    y, c)));
            faceGrid_[2 + side].push_back(worldOf(glm::vec3(c, y, 0.0f)));
            faceGrid_[2 + side].push_back(worldOf(glm::vec3(c, y, L)));

            // z constant: the face spans x and y
            faceGrid_[4 + side].push_back(worldOf(glm::vec3(0.0f, c, z)));
            faceGrid_[4 + side].push_back(worldOf(glm::vec3(L,    c, z)));
            faceGrid_[4 + side].push_back(worldOf(glm::vec3(c, 0.0f, z)));
            faceGrid_[4 + side].push_back(worldOf(glm::vec3(c, L,    z)));
        }
    }
}


// ============================================================================
// Spaces and matrices
// ============================================================================

float SubRegionModal::latticeSize() const
{
    return (float)((model_ && model_->latticeSize() > 0) ? model_->latticeSize() : 1);
}

glm::vec3 SubRegionModal::worldOf(const glm::vec3& latticeCoord) const
{
    return (latticeCoord / latticeSize()) - glm::vec3(0.5f);
}

void SubRegionModal::viewportRect(int winW, int winH, int& x, int& y, int& w, int& h) const
{
    x = kSideMarginPx;
    y = kTopBandPx;
    w = std::max(160, winW - 2 * kSideMarginPx);
    h = std::max(120, winH - kTopBandPx - kBottomBandPx);
}

glm::mat4 SubRegionModal::viewMatrix() const
{
    return camera_.GetViewMatrix();
}

glm::mat4 SubRegionModal::projMatrix(int winW, int winH) const
{
    int x, y, w, h;
    viewportRect(winW, winH, x, y, w, h);

    const float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    // The overlay owns its perspective: ProjectionManager's 3D matrix belongs to
    // the main scene's full-window viewport, not to this inset.
    return glm::perspective(glm::radians(kFovDegrees), aspect, 0.05f, 50.0f);
}

glm::mat4 SubRegionModal::viewProj(int winW, int winH) const
{
    return projMatrix(winW, winH) * viewMatrix();
}

glm::vec2 SubRegionModal::screenOf(const glm::vec3& latticeCoord,
                                   int winW, int winH) const
{
    int vx, vy, vw, vh;
    viewportRect(winW, winH, vx, vy, vw, vh);

    const glm::vec4 clip = viewProj(winW, winH) * glm::vec4(worldOf(latticeCoord), 1.0f);
    if (clip.w == 0.0f) return glm::vec2(-10000.0f, -10000.0f);

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    // Window pixels with y downwards (what the splash and the mouse callbacks
    // use), so the viewport's top edge is `vy`.
    return glm::vec2((float)vx + (ndc.x * 0.5f + 0.5f) * (float)vw,
                     (float)vy + (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)vh);
}

bool SubRegionModal::rayOf(float mx, float my, int winW, int winH,
                           glm::vec3& origin, glm::vec3& dir) const
{
    int vx, vy, vw, vh;
    viewportRect(winW, winH, vx, vy, vw, vh);
    if (vw <= 0 || vh <= 0) return false;

    const float ndcX = ((mx - (float)vx) / (float)vw) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((my - (float)vy) / (float)vh) * 2.0f;

    const glm::mat4 inv = glm::inverse(viewProj(winW, winH));

    const glm::vec4 near4 = inv * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 far4  = inv * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);


    if (near4.w == 0.0f || far4.w == 0.0f) return false;

    origin = glm::vec3(near4) / near4.w;
    dir    = glm::normalize(glm::vec3(far4) / far4.w - origin);
    return true;
}

// ============================================================================
// The gizmo: the six faces of the selected box
// ============================================================================

namespace {

// Centre of face `index` (SubRegionBox::Handle order), in lattice coordinates on
// the cell boundaries: the face at x0 is the boundary x = x0, the face at x1 is
// the boundary x = x1 + 1.
glm::vec3 faceCentre(const SubRegionBox& b, int index)
{
    const float mx = (b.x0() + b.x1() + 1) * 0.5f;
    const float my = (b.y0() + b.y1() + 1) * 0.5f;
    const float mz = (b.z0() + b.z1() + 1) * 0.5f;

    switch ((SubRegionBox::Handle)index)
    {
        case SubRegionBox::Handle::XMin: return glm::vec3((float)b.x0(),           my, mz);
        case SubRegionBox::Handle::XMax: return glm::vec3((float)(b.x1() + 1),     my, mz);
        case SubRegionBox::Handle::YMin: return glm::vec3(mx, (float)b.y0(),       mz);
        case SubRegionBox::Handle::YMax: return glm::vec3(mx, (float)(b.y1() + 1), mz);
        case SubRegionBox::Handle::ZMin: return glm::vec3(mx, my, (float)b.z0());
        default:                         return glm::vec3(mx, my, (float)(b.z1() + 1));
    }
}

bool isMaxFace(int index)
{
    return index == (int)SubRegionBox::Handle::XMax ||
           index == (int)SubRegionBox::Handle::YMax ||
           index == (int)SubRegionBox::Handle::ZMax;
}

// The four lattice corners of one face of the box, in fan order.  axis: 0 = x,
// 1 = y, 2 = z; `positive` picks the far one of the pair.
void faceLatticeCorners(const float bx[2], const float by[2], const float bz[2],
                        int axis, bool positive, glm::vec3 out[4])
{
    const int v = positive ? 1 : 0;

    if (axis == 0)
    {
        out[0] = glm::vec3(bx[v], by[0], bz[0]);
        out[1] = glm::vec3(bx[v], by[1], bz[0]);
        out[2] = glm::vec3(bx[v], by[1], bz[1]);
        out[3] = glm::vec3(bx[v], by[0], bz[1]);
    }
    else if (axis == 1)
    {
        out[0] = glm::vec3(bx[0], by[v], bz[0]);
        out[1] = glm::vec3(bx[1], by[v], bz[0]);
        out[2] = glm::vec3(bx[1], by[v], bz[1]);
        out[3] = glm::vec3(bx[0], by[v], bz[1]);
    }
    else
    {
        out[0] = glm::vec3(bx[0], by[0], bz[v]);
        out[1] = glm::vec3(bx[1], by[0], bz[v]);
        out[2] = glm::vec3(bx[1], by[1], bz[v]);
        out[3] = glm::vec3(bx[0], by[1], bz[v]);
    }
}

} // namespace

int SubRegionModal::handleAtScreen(float mx, float my, int winW, int winH) const
{
    if (!model_ || winW <= 0 || winH <= 0) return -1;

    const glm::vec3 eye = camera_.getPosition();
    const float radius2 = kPickRadiusPx * kPickRadiusPx;

    int   best = -1;
    float bestDepth = 0.0f;

    for (int i = 0; i < SubRegionBox::kHandleCount; ++i)
    {
        const glm::vec3 centre = faceCentre(*model_, i);
        const glm::vec2 p = screenOf(centre, winW, winH);

        const float dx = mx - p.x;
        const float dy = my - p.y;
        if (dx * dx + dy * dy > radius2) continue;

        // Faces overlap on screen: keep the one nearest the camera, because that
        // is the one the user sees there.
        const float depth = glm::length(worldOf(centre) - eye);
        if (best < 0 || depth < bestDepth)
        {
            best = i;
            bestDepth = depth;
        }
    }
    return best;
}

glm::vec2 SubRegionModal::handleScreenPos(int index) const
{
    if (!model_) return glm::vec2(0.0f);

    if (index < 0) index = 0;
    if (index >= SubRegionBox::kHandleCount) index = SubRegionBox::kHandleCount - 1;

    return screenOf(faceCentre(*model_, index), winW_, winH_);
}

bool SubRegionModal::dragFaceTo(float mx, float my, int winW, int winH)
{
    if (!model_ || grabbed_ < 0) return false;

    glm::vec3 origin, dir;
    if (!rayOf(mx, my, winW, winH, origin, dir)) return false;

    const int axis = model_->axisOf((SubRegionBox::Handle)grabbed_);

    // The face slides along its own axis, so the mouse ray and that axis line are
    // (in general) skew: the dragging position is the closest point between them.
    // Intersecting the ray with the face's PLANE does not work -- every ray hits
    // that plane at the plane's own coordinate, so the value could never change
    // (the self-test in the splash caught exactly that).
    const glm::vec3 axisDir((axis == 0) ? 1.0f : 0.0f,
                            (axis == 1) ? 1.0f : 0.0f,
                            (axis == 2) ? 1.0f : 0.0f);

    const glm::vec3 anchor = worldOf(faceCentre(*model_, grabbed_));
    const glm::vec3 w      = anchor - origin;

    const float uv    = glm::dot(axisDir, dir);
    const float denom = 1.0f - uv * uv;

    // Parallel to the face: no useful information from the mouse.
    if (std::abs(denom) < 1.0e-4f) return false;

    const float t = (uv * glm::dot(w, dir) - glm::dot(w, axisDir)) / denom;

    // World coordinate of the dragged point along the axis, then lattice units
    // and a cell index (a minimum face ends on the low cell boundary, a maximum
    // face on the high one).
    const float worldCoord = anchor[axis] + t;
    const float lat        = (worldCoord + 0.5f) * latticeSize();

    int value = (int)std::lround(lat);
    if (isMaxFace(grabbed_)) value -= 1;

    return model_->assignValueOf((SubRegionBox::Handle)grabbed_, value);
}
// ============================================================================
// Input
// ============================================================================

SubRegionModal::Result SubRegionModal::onMousePress(float mx, float my, int button)
{
    Result r;
    if (!open_) return r;
    r.consumed = true;

    lastMx_ = mx;
    lastMy_ = my;

    if (button == 0)
    {
        const int h = handleAtScreen(mx, my, winW_, winH_);
        if (h >= 0 && model_)
        {
            grabbed_ = h;
            model_->setActiveHandle((SubRegionBox::Handle)h);
            return r;
        }

        orbiting_ = true;   // a left drag away from a handle orbits as well
    }
    else if (button == 2)
    {
        panning_ = true;
    }
    return r;
}

SubRegionModal::Result SubRegionModal::onMouseMove(float mx, float my)
{
    Result r;
    if (!open_) return r;
    r.consumed = true;

    const float dx = mx - lastMx_;
    const float dy = my - lastMy_;

    if (grabbed_ >= 0)
    {
        r.changed = dragFaceTo(mx, my, winW_, winH_);
    }
    else if (orbiting_)
    {
        // The main scene orbits on a middle drag; here a left drag on empty
        // space does it too, with the same yoffset sign (see input.cpp).
        camera_.ProcessMiddleMouseOrbit(dx, -dy);
    }
    else if (panning_)
    {
        camera_.ProcessMiddleMousePan(dx, -dy);
    }
    else
    {
        hovered_ = handleAtScreen(mx, my, winW_, winH_);
    }

    lastMx_ = mx;
    lastMy_ = my;
    return r;
}

SubRegionModal::Result SubRegionModal::onMouseRelease()
{
    Result r;
    if (!open_) return r;
    r.consumed = true;

    if (grabbed_ >= 0) r.changed = true;   // the caller re-prints the final value

    grabbed_  = -1;
    orbiting_ = false;
    panning_  = false;
    return r;
}

SubRegionModal::Result SubRegionModal::onScroll(float dy)
{
    Result r;
    if (!open_) return r;
    r.consumed = true;

    camera_.ProcessMouseScroll(dy);
    return r;
}

SubRegionModal::Result SubRegionModal::onKey(int key, int mods)
{
    Result r;
    if (!open_) return r;
    r.consumed = true;

    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    switch (key)
    {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            // Nothing to confirm: the region is edited live, so both keys just
            // hand control back to the setup screen.
            close();
            break;

        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
            if (model_) model_->cycleHandle((key == GLFW_KEY_RIGHT) ? 1 : -1);
            r.changed = true;
            break;

        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
            if (model_)
                r.changed = model_->moveActive(((key == GLFW_KEY_UP) ? 1 : -1) *
                                               (shift ? 5 : 1));
            break;

        case GLFW_KEY_HOME:
        case GLFW_KEY_0:
        case GLFW_KEY_KP_0:
            if (model_) r.changed = model_->resetToFull();
            break;

        default:
            break;
    }
    return r;
}


// ============================================================================
// Drawing
// ============================================================================

void SubRegionModal::drawText2D(TextRenderer* textRenderer, const std::string& text,
                                float x, float yCenter, float scale,
                                const glm::vec3& color, int winW, int winH) const
{
    if (!textRenderer) return;

    // Same convention as the splash: yCenter is top-down, RenderText takes a
    // bottom-up baseline.
    const float ascender  = textRenderer->getAscenderPx();
    const float descender = textRenderer->getDescenderPx();
    const float halfLine  = 0.5f * (ascender - descender) * scale;
    const float baselineBU = ((float)winH - yCenter) + halfLine - 1.0f * scale;

    glUseProgram(textProgram);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "projection"), 1, GL_FALSE,
                       glm::value_ptr(ProjectionManager::instance().get2DOrtho()));
    textRenderer->RenderText(text, x, baselineBU, scale, color, winW, winH);
    glUseProgram(0);
}

void SubRegionModal::render(TextRenderer* textRenderer, int winW, int winH)
{
    if (!open_ || !model_ || winW <= 0 || winH <= 0) return;

    winW_ = winW;
    winH_ = winH;

    ensureBuffers(vao_, vbo_);

    if (alphaProgram_ == 0)
    {
        // The alpha shader the project already had (shader.h) and never used:
        // it is the only one that can dim the window and tint the faces.
        alphaProgram_  = compileTransparentShader();
        alphaMvpLoc_   = glGetUniformLocation(alphaProgram_, "uMVP");
        alphaColorLoc_ = glGetUniformLocation(alphaProgram_, "uColor");
        alphaAlphaLoc_ = glGetUniformLocation(alphaProgram_, "uAlpha");
    }

    if (model_->latticeSize() != lattice_) rebuildGrid();

    const SubRegionBox& b = *model_;

    // The box in lattice coordinates, on the cell boundaries (+1 on the far
    // side, because bounds are inclusive cell indices).
    const float bx[2] = { (float)b.x0(),        (float)(b.x1() + 1) };
    const float by[2] = { (float)b.y0(),        (float)(b.y1() + 1) };
    const float bz[2] = { (float)b.z0(),        (float)(b.z1() + 1) };

    std::vector<glm::vec3> edges;
    edges.reserve(24);
    for (int i = 0; i < 4; ++i)
    {
        const int a = i & 1;
        const int c = (i >> 1) & 1;

        edges.push_back(worldOf(glm::vec3(bx[0], by[a], bz[c])));
        edges.push_back(worldOf(glm::vec3(bx[1], by[a], bz[c])));

        edges.push_back(worldOf(glm::vec3(bx[a], by[0], bz[c])));
        edges.push_back(worldOf(glm::vec3(bx[a], by[1], bz[c])));

        edges.push_back(worldOf(glm::vec3(bx[a], by[c], bz[0])));
        edges.push_back(worldOf(glm::vec3(bx[a], by[c], bz[1])));
    }

    // The three faces of the box that look at the camera: which one of each pair
    // is visible follows the camera, because the overlay can be orbited.
    const glm::vec3 eye = camera_.getPosition();

    const glm::mat4 ortho2D = ProjectionManager::instance().get2DOrtho();

    // ---- 1) dim the whole window -------------------------------------------
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(alphaProgram_);
    glUniformMatrix4fv(alphaMvpLoc_, 1, GL_FALSE, glm::value_ptr(ortho2D));
    glUniform3f(alphaColorLoc_, 0.04f, 0.05f, 0.09f);
    glUniform1f(alphaAlphaLoc_, 0.82f);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    uploadAndDraw(GL_TRIANGLE_STRIP, {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3((float)winW, 0.0f, 0.0f),
        glm::vec3(0.0f, (float)winH, 0.0f),
        glm::vec3((float)winW, (float)winH, 0.0f)
    });

    // ---- 2) the 3D pass, inside the inset viewport --------------------------
    int vx, vy, vw, vh;
    viewportRect(winW, winH, vx, vy, vw, vh);

    glViewport(vx, winH - (vy + vh), vw, vh);   // GL viewports grow upwards
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    const glm::mat4 mvp = viewProj(winW, winH);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // The lattice, faint: its twelve edges plus the grid of the faces that look
    // at the camera.
    glUseProgram(colorProgram3D);
    glUniformMatrix4fv(colorMvpLoc3D, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorColorLoc3D, kGridColor.r, kGridColor.g, kGridColor.b);
    glLineWidth(1.0f);
    uploadAndDraw(GL_LINES, edges_);

    for (int f = 0; f < 6; ++f)
    {
        const int  axis = f / 2;
        const bool positive = (f % 2) == 1;
        const bool visible = positive ? (eye[axis] > 0.0f) : (eye[axis] < 0.0f);

        if (visible) uploadAndDraw(GL_LINES, faceGrid_[f]);
    }

    // The selected box: its three camera-facing faces first (translucent, so the
    // lattice behind stays readable), then its edges and handles.
    glUseProgram(alphaProgram_);
    glUniformMatrix4fv(alphaMvpLoc_, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform1f(alphaAlphaLoc_, 0.32f);

    for (int axis = 0; axis < 3; ++axis)
    {
        const bool positive = (eye[axis] > 0.0f);

        glm::vec3 corners[4];
        faceLatticeCorners(bx, by, bz, axis, positive, corners);

        const std::vector<glm::vec3> quad = {
            worldOf(corners[0]), worldOf(corners[1]),
            worldOf(corners[2]), worldOf(corners[3])
        };

        const glm::vec3& col = (axis == 0) ? kFaceX : ((axis == 1) ? kFaceTop : kFaceZ);
        glUniform3f(alphaColorLoc_, col.r, col.g, col.b);
        uploadAndDraw(GL_TRIANGLE_FAN, quad);
    }

    // Its twelve edges.
    glUseProgram(colorProgram3D);
    glUniformMatrix4fv(colorMvpLoc3D, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorColorLoc3D, kEdgeColor.r, kEdgeColor.g, kEdgeColor.b);
    glLineWidth(2.2f);
    uploadAndDraw(GL_LINES, edges);

    // The gizmo: one handle per face, the active one orange and larger.
    for (int i = 0; i < SubRegionBox::kHandleCount; ++i)
    {
        const bool active = (i == (int)b.activeHandle());
        const glm::vec3& col = active ? kHandleOn
                             : (i == hovered_) ? kHandleHover : kHandleOff;

        glUniform3f(colorColorLoc3D, col.r, col.g, col.b);
        glPointSize(active ? 14.0f : 9.0f);
        uploadAndDraw(GL_POINTS, { worldOf(faceCentre(b, i)) });
    }

    glLineWidth(1.0f);
    glPointSize(1.0f);

    // ---- 3) back to 2D: strips with the readouts ----------------------------
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, winW, winH);
    glBindVertexArray(0);

    const glm::vec3 stripColor(0.07f, 0.08f, 0.13f);
    drawQuad2D(0.0f, 0.0f, (float)winW, (float)kTopBandPx, stripColor, ortho2D);
    drawQuad2D(0.0f, (float)(winH - kBottomBandPx), (float)winW, (float)winH,
               stripColor, ortho2D);

    const SubRegionBox::Summary sum = b.summarize(layers_, cellBytes_);

    const glm::vec3 white(0.94f, 0.95f, 0.99f);
    const glm::vec3 dim(0.66f, 0.70f, 0.80f);
    const glm::vec3 accent(0.45f, 0.80f, 1.00f);

    const float left = (float)kSideMarginPx;
    const float step = 24.0f;

    drawText2D(textRenderer, "Region of the lattice",
               left, 26.0f, 0.46f, white, winW, winH);
    drawText2D(textRenderer,
               "Enter or Esc: back to the setup screen    (the region is applied as you edit)",
               left + 210.0f, 27.0f, 0.30f, dim, winW, winH);

    float y = (float)winH - (float)kBottomBandPx + 26.0f;

    drawText2D(textRenderer, "L = " + std::to_string(b.latticeSize()), left, y, 0.34f, white, winW, winH);
    drawText2D(textRenderer,
               std::string("face ") + b.handleName(b.activeHandle()) +
               "   (Up/Down moves it, Shift for five cells; Left/Right picks the face; Home: whole lattice)",
               left + 90.0f, y, 0.30f, accent, winW, winH);
    y += step;

    drawText2D(textRenderer, sum.bounds, left, y, 0.34f, white, winW, winH);
    y += step;

    drawText2D(textRenderer, sum.cells, left, y, 0.30f, white, winW, winH);
    y += step;

    drawText2D(textRenderer, sum.memory, left, y, 0.30f, white, winW, winH);
    y += step;

    drawText2D(textRenderer,
               "mouse: left-drag on a face moves it; left-drag elsewhere or middle-drag orbits; "
               "Ctrl+middle-drag pans; wheel zooms",
               left, y, 0.28f, dim, winW, winH);
}

