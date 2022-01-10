#pragma once

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"  
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"

#include "beatsaber-hook/shared/utils/utils.h"  
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"


static ModInfo modInfo{
    ID,
    VERSION
};

Logger& loggingFunction();

#define log(...) loggingFunction().info(__VA_ARGS__)