#include "Renderer2D.h"
#include "shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Renderer2D {

static GLuint shaderProgram = 0;
static GLint uMVP = -1;
static GLint uColor = -1;

void init()
{
    const char* vs = R"(
        #version 330 core

        layout(location = 0) in vec2 aPos;

        uniform mat4 uMVP;

        void main()
        {
            gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* fs = R"(
        #version 330 core

        uniform vec3 uColor;

        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(uColor, 1.0);
        }
    )";

    shaderProgram = compileShader(vs, fs);

    uMVP   = glGetUniformLocation(shaderProgram, "uMVP");
    uColor = glGetUniformLocation(shaderProgram, "uColor");
}

void use()
{
    glUseProgram(shaderProgram);
}

void setMVP(const glm::mat4& mvp)
{
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
}

void setColor(const glm::vec3& color)
{
    glUniform3f(uColor, color.r, color.g, color.b);
}

GLuint program()
{
    return shaderProgram;
}

}