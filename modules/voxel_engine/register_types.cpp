#include "register_types.h"
#include "core/object/class_db.h"

#include "voxel_engine.h"

void initialize_voxel_engine_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(VoxelEngine);
}

void uninitialize_voxel_engine_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
