#include "register_types.h"
#include "core/object/class_db.h"

#include "block_voxel_data.h"
#include "voxel_engine.h"
#include "file_manager.h"

void initialize_voxel_engine_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	
	GDREGISTER_CLASS(BlockVoxelData);
	GDREGISTER_CLASS(Voxel);
	GDREGISTER_CLASS(VoxelEngine);
	GDREGISTER_CLASS(FileManager);
}

void uninitialize_voxel_engine_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
