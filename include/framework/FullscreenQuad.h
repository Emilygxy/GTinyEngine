#pragma once

#include <glad/glad.h>

namespace te
{
    class FullscreenQuad
    {
    public:
        FullscreenQuad();
        ~FullscreenQuad();

        void Draw();

    private:
        GLuint mVAO;
        GLuint mVBO;
    };
}
