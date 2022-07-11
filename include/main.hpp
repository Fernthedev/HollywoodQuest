#pragma once

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"  
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"

#include "beatsaber-hook/shared/utils/utils.h"  
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"

#include "paper/shared/logger.hpp"

static ModInfo modInfo{
    MOD_ID,
    VERSION
};

extern Paper::ConstLoggerContext<10> HLogger;