#pragma
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

class Shader;

class Skybox
{
public:
    Skybox(const std::vector<std::string>& faces);
    ~Skybox();
    void Draw(const glm::mat4& view, const glm::mat4& projection);

private:
    unsigned int mCubemapTexture{0};
    unsigned int mVAO{ 0 };
    unsigned int mVBO{ 0 };
    std::shared_ptr<Shader> mShader{ nullptr };
};
