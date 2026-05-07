#pragma once

#include <string>
#include <vector>

#include "GSRenderTypes.h"

class GSSceneLoader {
public:
    std::vector<GSVertex> load(const std::string& plyPath) const;
};

