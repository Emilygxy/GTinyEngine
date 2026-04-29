#include "GSSceneLoader.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class PlyScalarType {
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64,
    Unknown
};

struct PlyProperty {
    std::string name;
    PlyScalarType type = PlyScalarType::Unknown;
};

struct PlyHeader {
    int numVertices = 0;
    int numFaces = 0;
    bool isBinaryLittleEndian = false;
    std::vector<PlyProperty> vertexProperties;
};

PlyScalarType parseScalarType(const std::string& token) {
    if (token == "char" || token == "int8") return PlyScalarType::Int8;
    if (token == "uchar" || token == "uint8") return PlyScalarType::UInt8;
    if (token == "short" || token == "int16") return PlyScalarType::Int16;
    if (token == "ushort" || token == "uint16") return PlyScalarType::UInt16;
    if (token == "int" || token == "int32") return PlyScalarType::Int32;
    if (token == "uint" || token == "uint32") return PlyScalarType::UInt32;
    if (token == "float" || token == "float32") return PlyScalarType::Float32;
    if (token == "double" || token == "float64") return PlyScalarType::Float64;
    return PlyScalarType::Unknown;
}

template <typename T>
T readBinaryValue(std::ifstream& plyFile) {
    T value{};
    plyFile.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!plyFile.good()) {
        throw std::runtime_error("Failed to read binary PLY payload.");
    }
    return value;
}

float readBinaryScalarAsFloat(std::ifstream& plyFile, PlyScalarType type) {
    switch (type) {
    case PlyScalarType::Int8: return static_cast<float>(readBinaryValue<int8_t>(plyFile));
    case PlyScalarType::UInt8: return static_cast<float>(readBinaryValue<uint8_t>(plyFile));
    case PlyScalarType::Int16: return static_cast<float>(readBinaryValue<int16_t>(plyFile));
    case PlyScalarType::UInt16: return static_cast<float>(readBinaryValue<uint16_t>(plyFile));
    case PlyScalarType::Int32: return static_cast<float>(readBinaryValue<int32_t>(plyFile));
    case PlyScalarType::UInt32: return static_cast<float>(readBinaryValue<uint32_t>(plyFile));
    case PlyScalarType::Float32: return readBinaryValue<float>(plyFile);
    case PlyScalarType::Float64: return static_cast<float>(readBinaryValue<double>(plyFile));
    case PlyScalarType::Unknown:
    default:
        throw std::runtime_error("Unsupported PLY scalar property type.");
    }
}

std::optional<int> parseIndexedProperty(const std::string& name, const std::string& prefix) {
    if (name.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    const std::string suffix = name.substr(prefix.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    int value = 0;
    for (const char ch : suffix) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<int>(ch - '0');
    }
    return value;
}

PlyHeader loadPlyHeader(std::ifstream& plyFile) {
    PlyHeader header{};
    std::string line;
    bool headerEnd = false;
    std::string currentElement;
    bool formatParsed = false;

    while (std::getline(plyFile, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token.empty() || token == "comment") {
            continue;
        }

        if (token == "format") {
            std::string formatName;
            std::string formatVersion;
            iss >> formatName >> formatVersion;
            if (formatName != "binary_little_endian") {
                throw std::runtime_error("Unsupported PLY format: only binary_little_endian is supported.");
            }
            header.isBinaryLittleEndian = true;
            formatParsed = true;
        } else if (token == "element") {
            iss >> currentElement;
            if (currentElement == "vertex") {
                iss >> header.numVertices;
            } else if (currentElement == "face") {
                iss >> header.numFaces;
            }
        } else if (token == "property") {
            if (currentElement != "vertex") {
                continue;
            }

            std::string typeToken;
            iss >> typeToken;
            if (typeToken == "list") {
                throw std::runtime_error("Unsupported PLY vertex property: list type.");
            }

            std::string nameToken;
            iss >> nameToken;
            const PlyScalarType scalarType = parseScalarType(typeToken);
            if (scalarType == PlyScalarType::Unknown) {
                throw std::runtime_error("Unsupported PLY vertex property scalar type: " + typeToken);
            }

            PlyProperty property;
            property.name = nameToken;
            property.type = scalarType;
            header.vertexProperties.push_back(property);
        } else if (token == "end_header") {
            headerEnd = true;
            break;
        }
    }

    if (!headerEnd) {
        throw std::runtime_error("Invalid PLY file: end_header not found.");
    }
    if (!formatParsed || !header.isBinaryLittleEndian) {
        throw std::runtime_error("Invalid PLY file: missing or unsupported format declaration.");
    }
    if (header.vertexProperties.empty()) {
        throw std::runtime_error("Invalid PLY file: no vertex properties found.");
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

    std::map<std::string, size_t> propertyIndices;
    std::map<int, size_t> restPropertyIndices;
    for (size_t i = 0; i < header.vertexProperties.size(); i++) {
        const auto& property = header.vertexProperties[i];
        propertyIndices[property.name] = i;
        if (const auto restIndex = parseIndexedProperty(property.name, "f_rest_"); restIndex.has_value()) {
            restPropertyIndices[*restIndex] = i;
        }
    }

    const auto requireProperty = [&](const std::string& propertyName) -> size_t {
        const auto it = propertyIndices.find(propertyName);
        if (it == propertyIndices.end()) {
            throw std::runtime_error("PLY is missing required vertex property: " + propertyName);
        }
        return it->second;
    };

    const size_t xIdx = requireProperty("x");
    const size_t yIdx = requireProperty("y");
    const size_t zIdx = requireProperty("z");

    const auto findProperty = [&](const std::string& propertyName) -> std::optional<size_t> {
        const auto it = propertyIndices.find(propertyName);
        return it == propertyIndices.end() ? std::nullopt : std::optional<size_t>(it->second);
    };

    const auto opacityIdx = findProperty("opacity");
    const auto scale0Idx = findProperty("scale_0");
    const auto scale1Idx = findProperty("scale_1");
    const auto scale2Idx = findProperty("scale_2");
    const auto rot0Idx = findProperty("rot_0");
    const auto rot1Idx = findProperty("rot_1");
    const auto rot2Idx = findProperty("rot_2");
    const auto rot3Idx = findProperty("rot_3");
    const auto dc0Idx = findProperty("f_dc_0");
    const auto dc1Idx = findProperty("f_dc_1");
    const auto dc2Idx = findProperty("f_dc_2");

    std::vector<GSVertex> vertices(static_cast<size_t>(header.numVertices));
    for (int i = 0; i < header.numVertices; i++) {
        std::vector<float> values(header.vertexProperties.size(), 0.0f);
        for (size_t propIndex = 0; propIndex < header.vertexProperties.size(); propIndex++) {
            values[propIndex] = readBinaryScalarAsFloat(plyFile, header.vertexProperties[propIndex].type);
        }

        auto& vertex = vertices[i];
        vertex.position = glm::vec4(values[xIdx], values[yIdx], values[zIdx], 1.0f);

        const float opacity = opacityIdx.has_value() ? values[*opacityIdx] : 0.0f;
        const float sx = scale0Idx.has_value() ? values[*scale0Idx] : 0.0f;
        const float sy = scale1Idx.has_value() ? values[*scale1Idx] : 0.0f;
        const float sz = scale2Idx.has_value() ? values[*scale2Idx] : 0.0f;
        vertices[i].scale_opacity = glm::vec4(
            glm::exp(sx),
            glm::exp(sy),
            glm::exp(sz),
            1.0f / (1.0f + std::exp(-opacity)));

        // Keep the same convention as 3DGS + shader `rotationFromQuaternion`:
        // payload stores quaternion as (w, x, y, z).
        const float rw = rot0Idx.has_value() ? values[*rot0Idx] : 1.0f;
        const float rx = rot1Idx.has_value() ? values[*rot1Idx] : 0.0f;
        const float ry = rot2Idx.has_value() ? values[*rot2Idx] : 0.0f;
        const float rz = rot3Idx.has_value() ? values[*rot3Idx] : 0.0f;
        const glm::vec4 rotationWxyz(rw, rx, ry, rz);
        const float rotationNorm = glm::length(rotationWxyz);
        vertex.rotation = rotationNorm > std::numeric_limits<float>::epsilon()
            ? (rotationWxyz / rotationNorm)
            : glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

        for (float& shCoeff : vertex.sh) {
            shCoeff = 0.0f;
        }
        if (dc0Idx.has_value()) vertex.sh[0] = values[*dc0Idx];
        if (dc1Idx.has_value()) vertex.sh[1] = values[*dc1Idx];
        if (dc2Idx.has_value()) vertex.sh[2] = values[*dc2Idx];

        // Runtime vertex layout supports up to SH degree 3 (16 basis functions * RGB).
        // Populate available f_rest_* coefficients and safely ignore any higher-degree tail.
        for (const auto& [restIndex, propertyIndex] : restPropertyIndices) {
            if (restIndex < 0 || restIndex >= 45) {
                continue;
            }
            if (restIndex < 15) {
                vertex.sh[(restIndex + 1) * 3 + 0] = values[propertyIndex];
            } else if (restIndex < 30) {
                vertex.sh[(restIndex - 14) * 3 + 1] = values[propertyIndex];
            } else {
                vertex.sh[(restIndex - 29) * 3 + 2] = values[propertyIndex];
            }
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

