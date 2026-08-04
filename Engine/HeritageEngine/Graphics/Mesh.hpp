#pragma once

#include <glad/glad.h>

#include <string>
#include <vector>

namespace heritage::graphics
{

struct Mesh
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
};

Mesh loadObjMesh(const std::string& path);
void uploadMesh(Mesh& mesh);

} // namespace heritage::graphics
