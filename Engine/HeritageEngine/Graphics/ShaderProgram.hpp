#pragma once

#include <glad/glad.h>

namespace heritage::graphics
{

GLuint buildShaderProgram(const char* vertexSource, const char* fragmentSource);

} // namespace heritage::graphics
