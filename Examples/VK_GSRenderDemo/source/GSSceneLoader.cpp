#include "GSSceneLoader.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct PlyHeader {
    int numVertices = 0;
    int numFaces = 0;
};

struct VertexStorage {
    glm::vec3 position;
    glm::vec3 normal;
    float shs[48];
    float opacity;
    glm::vec3 scale;
    glm::vec4 rotation;
};

static_assert(sizeof(VertexStorage) == 62 * sizeof(float), "Unexpected PLY vertex payload layout.");

PlyHeader loadPlyHeader(std::ifstream& plyFile) {
    PlyHeader header{};
    std::string line;
    bool headerEnd = false;

    while (std::getline(plyFile, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token == "element") {
            iss >> token;
            if (token == "vertex") {
                iss >> header.numVertices;
            } else if (token == "face") {
                iss >> header.numFaces;
            }
        } else if (token == "end_header") {
            headerEnd = true;
            break;
        }
    }

    if (!headerEnd) {
        throw std::runtime_error("Invalid PLY file: end_header not found.");
    }
    return header;
}

std::vector<GSVertex> loadPlyVertices(const std::string& filename) {
    std::ifstream plyFile(filename, std::ios::binary);
    if (!plyFile.is_open()) {
        throw std::runtime_error("Could not open PLY: " + filename);
    }

    const auto header = loadPlyHeader(plyFile);
    if (header.numVertices <= 0) {
        throw std::runtime_error("PLY has no vertices: " + filename);
    }

    std::vector<GSVertex> vertices(static_cast<size_t>(header.numVertices));
    for (int i = 0; i < header.numVertices; i++) {
        VertexStorage vs{};
        plyFile.read(reinterpret_cast<char*>(&vs), sizeof(VertexStorage));
        if (!plyFile.good()) {
            throw std::runtime_error("Failed to read PLY vertex payload");
        }

        vertices[i].position = glm::vec4(vs.position, 1.0f);
        vertices[i].scale_opacity = glm::vec4(
            glm::exp(vs.scale.x),
            glm::exp(vs.scale.y),
            glm::exp(vs.scale.z),
            1.0f / (1.0f + std::exp(-vs.opacity)));
        // Keep the same convention as 3DGS + shader `rotationFromQuaternion`:
        // payload stores quaternion as (w, x, y, z).
        const glm::vec4 rotationWxyz(vs.rotation.x, vs.rotation.y, vs.rotation.z, vs.rotation.w);
        vertices[i].rotation = glm::normalize(rotationWxyz);
        vertices[i].sh[0] = vs.shs[0];
        vertices[i].sh[1] = vs.shs[1];
        vertices[i].sh[2] = vs.shs[2];

        constexpr int shN = 16;
        for (int j = 1; j < shN; j++) {
            vertices[i].sh[j * 3 + 0] = vs.shs[(j - 1) + 3];
            vertices[i].sh[j * 3 + 1] = vs.shs[(j - 1) + shN + 2];
            vertices[i].sh[j * 3 + 2] = vs.shs[(j - 1) + shN * 2 + 1];
        }
    }
    return vertices;
}

std::vector<GSVertex> createFallbackScene() {
    GSVertex v{};
    v.position = glm::vec4(0.0f, 0.0f, -2.0f, 1.0f);
    v.scale_opacity = glm::vec4(100.0f, 100.0f, 100.0f, 0.8f);
    // Identity quaternion in (w, x, y, z).
    v.rotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 48; i++) {
        v.sh[i] = 0.0f;
    }
    v.sh[0] = 1.0f;
    v.sh[1] = 0.6f;
    v.sh[2] = 0.2f;
    return {v};
}

} // namespace

std::vector<GSVertex> GSSceneLoader::load(const std::string& plyPath) const {
    if (plyPath.empty()) {
        return createFallbackScene();
    }
    return loadPlyVertices(plyPath);
}

