#include "GSRenderDemoApp.h"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string plyPath;
    if (argc > 1) {
        plyPath = argv[1];
    }
    if (plyPath.empty()) {
        const std::string defaultRel = "Examples/VK_GSRenderDemo/assets/cloudpoints/sample_robot.ply";
        const std::string altRel = "../../../Examples/VK_GSRenderDemo/assets/cloudpoints/sample_robot.ply";
        if (std::filesystem::exists(defaultRel)) {
            plyPath = defaultRel;
        } else if (std::filesystem::exists(altRel)) {
            plyPath = altRel;
        }
    }
    GSRenderDemoApp demo;
    try {
        if (!demo.initialize(plyPath)) {
            std::cerr << "Failed to initialize VK_GSRenderDemo." << std::endl;
            return -1;
        }
        demo.run();
        demo.shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Fatal unknown error." << std::endl;
        return -1;
    }
    return 0;
}