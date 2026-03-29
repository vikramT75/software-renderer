#pragma once
#include "../pipeline/vertex.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <array>

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string name;
};

// Moved to file scope — local structs can't be used as template arguments in GCC 14
struct FaceVertex
{
    int vi, ti, ni;
};

inline Mesh loadOBJ(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("OBJ: cannot open file: " + path);

    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    std::vector<std::array<FaceVertex, 3>> faces;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v")
        {
            Vec3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (token == "vt")
        {
            Vec2 uv;
            ss >> uv.x >> uv.y;
            uv.y = 1.f - uv.y;
            uvs.push_back(uv);
        }
        else if (token == "vn")
        {
            Vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (token == "f")
        {
            std::array<FaceVertex, 3> tri;
            for (int i = 0; i < 3; ++i)
            {
                std::string chunk;
                ss >> chunk;
                FaceVertex fv = {-1, -1, -1};

                size_t s1 = chunk.find('/');
                if (s1 == std::string::npos)
                {
                    fv.vi = std::stoi(chunk) - 1;
                }
                else
                {
                    fv.vi = std::stoi(chunk.substr(0, s1)) - 1;
                    size_t s2 = chunk.find('/', s1 + 1);
                    if (s2 == std::string::npos)
                    {
                        fv.ti = std::stoi(chunk.substr(s1 + 1)) - 1;
                    }
                    else
                    {
                        if (s2 != s1 + 1)
                            fv.ti = std::stoi(chunk.substr(s1 + 1, s2 - s1 - 1)) - 1;
                        if (s2 + 1 < chunk.size())
                            fv.ni = std::stoi(chunk.substr(s2 + 1)) - 1;
                    }
                }
                tri[i] = fv;
            }
            faces.push_back(tri);
        }
    }

    Mesh mesh;
    mesh.vertices.reserve(faces.size() * 3);
    mesh.indices.reserve(faces.size() * 3);

    for (size_t fi = 0; fi < faces.size(); ++fi)
    {
        const auto &face = faces[fi];

        Vec3 flatNormal = {0.f, 0.f, 0.f};
        if (normals.empty() || face[0].ni < 0)
        {
            Vec3 a = positions[face[0].vi];
            Vec3 b = positions[face[1].vi];
            Vec3 c = positions[face[2].vi];
            flatNormal = (b - a).cross(c - a).normalized();
        }

        for (int i = 0; i < 3; ++i)
        {
            const FaceVertex &fv = face[i];
            Vertex v;
            v.position = positions[fv.vi];
            v.normal = (fv.ni >= 0 && !normals.empty())
                           ? normals[fv.ni]
                           : flatNormal;
            v.uv = (fv.ti >= 0 && !uvs.empty())
                       ? uvs[fv.ti]
                       : Vec2{0.f, 0.f};
            mesh.vertices.push_back(v);
            mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
        }
    }

    return mesh;
}