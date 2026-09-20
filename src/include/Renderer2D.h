#pragma once

#include <glm/glm.hpp>

namespace Renderer2D {

    void init();
    void use();

    void setMVP(const glm::mat4& mvp);
    void setColor(const glm::vec3& color);

    unsigned int program();
}