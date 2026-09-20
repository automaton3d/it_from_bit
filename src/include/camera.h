// camera.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class OrbitCamera {
public:
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);  // Look at origin, not (0,1,0)
    float distance   = 1.0f;  // Changed from 10.0f to 2.0f (closer by 5×)
    float yaw        = -90.0f;
    float pitch      = 0.0f;

    float panSpeed    = 0.01f;
    float orbitSpeed  = 0.2f;
    float zoomSpeed   = 0.05f;
    float minDistance = 0.1f;
    float maxDistance = 200.0f;

    bool  Orthographic = false;
    float OrthoSize    = 1.0f;  // Also adjust ortho size from 10.0f to 2.0f
    float Zoom         = 45.0f;

    glm::vec3 getPosition() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;

    void ProcessMiddleMouseOrbit(float xoffset, float yoffset);
    void ProcessMiddleMousePan(float xoffset, float yoffset);
    void ProcessMouseScroll(float yoffset);
    void ToggleProjection();
};
