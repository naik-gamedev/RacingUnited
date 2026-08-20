#pragma once

#include <glad/glad.h>

namespace heritage::graphics
{

GLuint buildShaderProgram(const char* vertexSource, const char* fragmentSource);
GLuint buildShaderProgram(
    const char* vertexSource,
    const char* geometrySource,
    const char* fragmentSource);
GLuint buildComputeShaderProgram(const char* computeSource);

} // namespace heritage::graphics
