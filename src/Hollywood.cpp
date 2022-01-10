#include "Hollywood.hpp"

#include "main.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "custom-types/shared/register.hpp"

void Hollywood::initialize() {
    loggingFunction().debug("Initializing Hollywood");
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();
}
