#include "Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

namespace heritage::graphics
{

Mesh loadObjMesh(const std::string& path)
{
    Mesh mesh;
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::map<std::pair<int, int>, unsigned int> cache;

    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Could not open OBJ: " << path << '\n';
        return mesh;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream stream(line);
        std::string token;
        stream >> token;

        if (token == "v")
        {
            float x, y, z;
            stream >> x >> y >> z;
            positions.push_back({ x, y, z });
        }
        else if (token == "vn")
        {
            float x, y, z;
            stream >> x >> y >> z;
            normals.push_back({ x, y, z });
        }
        else if (token == "f")
        {
            std::vector<std::pair<int, int>> face;
            std::string chunk;
            while (stream >> chunk)
            {
                int positionIndex = 0;
                int normalIndex = 0;
                const size_t firstSlash = chunk.find('/');
                if (firstSlash == std::string::npos)
                {
                    positionIndex = std::stoi(chunk);
                }
                else
                {
                    positionIndex = std::stoi(chunk.substr(0, firstSlash));
                    const size_t secondSlash = chunk.find('/', firstSlash + 1);
                    if (secondSlash != std::string::npos)
                        normalIndex = std::stoi(chunk.substr(secondSlash + 1));
                }

                if (positionIndex < 0)
                    positionIndex = static_cast<int>(positions.size()) + positionIndex + 1;
                if (normalIndex < 0)
                    normalIndex = static_cast<int>(normals.size()) + normalIndex + 1;
                face.push_back({ positionIndex, normalIndex });
            }

            for (int triangle = 1; triangle + 1 < static_cast<int>(face.size()); ++triangle)
            {
                for (const auto& facePoint : { face[0], face[triangle], face[triangle + 1] })
                {
                    const auto cached = cache.find(facePoint);
                    if (cached != cache.end())
                    {
                        mesh.indices.push_back(cached->second);
                        continue;
                    }

                    const unsigned int vertexIndex = static_cast<unsigned int>(mesh.vertices.size() / 6);
                    cache[facePoint] = vertexIndex;
                    const auto& position = positions[facePoint.first - 1];
                    mesh.vertices.insert(mesh.vertices.end(), { position[0], position[1], position[2] });

                    const int normalIndex = facePoint.second - 1;
                    if (normalIndex >= 0 && normalIndex < static_cast<int>(normals.size()))
                    {
                        const auto& normal = normals[normalIndex];
                        mesh.vertices.insert(mesh.vertices.end(), { normal[0], normal[1], normal[2] });
                    }
                    else
                    {
                        mesh.vertices.insert(mesh.vertices.end(), { 0.0f, 1.0f, 0.0f });
                    }
                    mesh.indices.push_back(vertexIndex);
                }
            }
        }
    }

    if (!mesh.vertices.empty())
    {
        float minX = mesh.vertices[0], maxX = mesh.vertices[0];
        float minY = mesh.vertices[1], maxY = mesh.vertices[1];
        float minZ = mesh.vertices[2], maxZ = mesh.vertices[2];
        for (size_t index = 0; index < mesh.vertices.size(); index += 6)
        {
            minX = std::min(minX, mesh.vertices[index]);
            maxX = std::max(maxX, mesh.vertices[index]);
            minY = std::min(minY, mesh.vertices[index + 1]);
            maxY = std::max(maxY, mesh.vertices[index + 1]);
            minZ = std::min(minZ, mesh.vertices[index + 2]);
            maxZ = std::max(maxZ, mesh.vertices[index + 2]);
        }

        const float centerX = (minX + maxX) * 0.5f;
        const float centerY = (minY + maxY) * 0.5f;
        const float centerZ = (minZ + maxZ) * 0.5f;
        const float scale = 2.0f / std::max({ maxX - minX, maxY - minY, maxZ - minZ });
        for (size_t index = 0; index < mesh.vertices.size(); index += 6)
        {
            mesh.vertices[index] = (mesh.vertices[index] - centerX) * scale;
            mesh.vertices[index + 1] = (mesh.vertices[index + 1] - centerY) * scale;
            mesh.vertices[index + 2] = (mesh.vertices[index + 2] - centerZ) * scale;
        }
    }

    std::cout << "OBJ loaded: " << mesh.indices.size() / 3 << " triangles\n";
    return mesh;
}

void uploadMesh(Mesh& mesh)
{
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

} // namespace heritage::graphics
