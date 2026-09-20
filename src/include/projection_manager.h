// ============================================================
// projection_manager.h - NEW FILE
// ============================================================
#ifndef PROJECTION_MANAGER_H
#define PROJECTION_MANAGER_H

#include <glm/glm.hpp>

/**
 * Centralized projection matrix management for the entire application.
 * All 2D UI rendering should use get2DOrtho().
 * All 3D rendering should use get3DPerspective().
 */
class ProjectionManager {
public:
    static ProjectionManager& instance() {
        static ProjectionManager inst;
        return inst;
    }

    ProjectionManager() = default;

    // Update viewport dimensions (call from resize callback)
    void setViewport(int width, int height);

    // Get 2D orthographic projection for ALL UI rendering
    // Uses top-left origin: (0,0) = top-left corner
    const glm::mat4& get2DOrtho() const { return ortho2D_; }

    // Get 3D perspective projection for scene rendering
    glm::mat4 get3DPerspective(float fov = 45.0f, float near = 0.01f, float far = 100.0f) const;

    // Viewport dimensions
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    float getAspect() const { return aspect_; }

private:

    int width_ = 800;
    int height_ = 600;
    float aspect_ = 800.0f / 600.0f;
    glm::mat4 ortho2D_{1.0f};
};

#endif // PROJECTION_MANAGER_H

