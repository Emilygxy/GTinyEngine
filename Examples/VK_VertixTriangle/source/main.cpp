#include "GlfwGeneral.h"
// learning link: https://easyvulkan.github.io/index.html

int main() {
    using namespace easy_vk;
    
    if (!InitializeWindow({ 1280, 720 }))
        return -1; //return error value
    while (!glfwWindowShouldClose(pWindow)) {
        
        /*render loop*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}