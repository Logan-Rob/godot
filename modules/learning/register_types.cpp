#include "register_types.h"
#include "core/object/class_db.h"

#include "moving_node_3d.h"

void initialize_learning_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    GDREGISTER_CLASS(MovingNode3D);
}

void uninitialize_learning_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
