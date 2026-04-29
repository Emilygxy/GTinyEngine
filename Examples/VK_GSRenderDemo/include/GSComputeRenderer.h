#pragma once

#include <vector>

#include "GSRenderTypes.h"

class GSComputeRenderer {
public:
    bool initialize(const std::vector<GSVertex>& vertices);
    void run();
    void shutdown();
};

