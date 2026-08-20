#include "ShaderProgram.hpp"

#include <iostream>

namespace heritage::graphics
{
namespace
{

GLuint compileShader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint succeeded = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
    if (succeeded == GL_FALSE)
    {
        char log[512] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation failed:\n" << log << '\n';
    }
    return shader;
}

} // namespace

GLuint buildShaderProgram(const char* vertexSource, const char* fragmentSource)
{
    return buildShaderProgram(vertexSource, nullptr, fragmentSource);
}

GLuint buildShaderProgram(
    const char* vertexSource,
    const char* geometrySource,
    const char* fragmentSource)
{
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint geometryShader = geometrySource != nullptr
        ? compileShader(GL_GEOMETRY_SHADER, geometrySource)
        : 0;
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    if (geometryShader != 0)
        glAttachShader(program, geometryShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint succeeded = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &succeeded);
    if (succeeded == GL_FALSE)
    {
        char log[512] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader program link failed:\n" << log << '\n';
    }

    glDeleteShader(vertexShader);
    if (geometryShader != 0)
        glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);
    return program;
}

GLuint buildComputeShaderProgram(const char* computeSource)
{
    const GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);
    const GLuint program = glCreateProgram();
    glAttachShader(program, computeShader);
    glLinkProgram(program);

    GLint succeeded = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &succeeded);
    if (succeeded == GL_FALSE)
    {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Compute shader program link failed:\n" << log << '\n';
        glDeleteShader(computeShader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(computeShader);
    return program;
}

} // namespace heritage::graphics
