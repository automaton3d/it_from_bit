// camera.cpp
#include "camera.h"
#include "projection_manager.h"

glm::vec3 OrbitCamera::getPosition() const {
    return target + glm::vec3(
        distance * cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        distance * sin(glm::radians(pitch)),
        distance * sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    );
}

glm::mat4 OrbitCamera::GetViewMatrix() const {
    return glm::lookAt(getPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 OrbitCamera::GetProjectionMatrix(float aspect) const {
    if (Orthographic) {
        float s = OrthoSize;
        return glm::ortho(-s * aspect, s * aspect, -s, s, 0.1f, 200.0f);
    }
//    return glm::perspective(glm::radians(Zoom), aspect, 0.1f, 200.0f);



    return ProjectionManager::instance().get3DPerspective();

}

void OrbitCamera::ProcessMiddleMouseOrbit(float xoffset, float yoffset) {
    yaw   += xoffset * orbitSpeed;
    pitch += yoffset * orbitSpeed;
    pitch = glm::clamp(pitch, -89.9f, 89.9f);
}

void OrbitCamera::ProcessMiddleMousePan(float xoffset, float yoffset) {
    glm::vec3 front = glm::normalize(target - getPosition());
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);
    target += right * (-xoffset * panSpeed * distance);
    target += up    * ( yoffset * panSpeed * distance);
}

void OrbitCamera::ProcessMouseScroll(float yoffset) {
    if (Orthographic) {
        OrthoSize = glm::max(OrthoSize - yoffset * 0.5f, 0.5f);
    } else {
        distance = glm::clamp(distance - yoffset * zoomSpeed, minDistance, maxDistance);
    }
}

void OrbitCamera::ToggleProjection() {
    Orthographic = !Orthographic;
}
