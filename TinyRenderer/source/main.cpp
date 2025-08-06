#include "shader.h"
#include <memory>

#include "Renderer.h"
#include "RenderAgent.h"

RenderAgent gRenderAgent;

int main()
{
    gRenderAgent.InitGL();
    // build and compile our shader program
    // prepare mesh
    gRenderAgent.SetupRenderer();

    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    gRenderAgent.RenderFrameBegin();

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    //
    gRenderAgent.RenderFrameEnd();
    
    return 0;
}