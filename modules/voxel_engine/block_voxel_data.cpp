#include "voxel_constants.h"
#include <cstdint>
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "block_voxel_data.h"



void Voxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_is_voxel_valid"), &Voxel::get_is_voxel_valid);
	ClassDB::bind_method(D_METHOD("set_is_voxel_valid", "p_valid"), &Voxel::set_is_voxel_valid);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_voxel_valid"), "set_is_voxel_valid", "get_is_voxel_valid");
	
	ClassDB::bind_method(D_METHOD("get_density"), &Voxel::get_density);
	ClassDB::bind_method(D_METHOD("set_density", "p_density"), &Voxel::set_density);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "density"), "set_density", "get_density");
	
	ClassDB::bind_method(D_METHOD("get_material"), &Voxel::get_material);
	ClassDB::bind_method(D_METHOD("set_material", "p_material"), &Voxel::set_material);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material"), "set_material", "get_material");
}



BlockVoxelData::ResourceState BlockVoxelData::get_resource_state() {
	return current_resource_state;
}

void BlockVoxelData::initialize_voxel_data(BlockVoxelData::ResourceState resource_state) {
	switch (resource_state) {
		case BlockVoxelData::ResourceState::Uninitialized:
			current_resource_state = resource_state;
			
			voxel_index_lookup.clear();
			voxel_densities.clear();
			voxel_materials.clear();
			voxel_indices.clear();
			voxel_is_edited.clear();
			edited_voxel_count = 0;
			
			break;
		case BlockVoxelData::ResourceState::Empty:
			current_resource_state = resource_state;
			
			voxel_index_lookup.clear();
			voxel_densities.clear();
			voxel_materials.clear();
			voxel_indices.clear();
			voxel_is_edited.clear();
			edited_voxel_count = 0;
			
			break;
		case BlockVoxelData::ResourceState::Sparse:
			current_resource_state = resource_state;
			
			voxel_index_lookup.clear();
			voxel_densities.clear();
			voxel_materials.clear();
			voxel_indices.clear();
			voxel_is_edited.clear();
			edited_voxel_count = 0;
			
			break;
		case BlockVoxelData::ResourceState::Dense:
			current_resource_state = resource_state;
			
			voxel_index_lookup.clear();
			voxel_densities.clear();
			voxel_materials.clear();
			voxel_indices.clear();
			voxel_is_edited.clear();
			edited_voxel_count = 0;
			
			voxel_densities.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
			voxel_materials.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
			voxel_is_edited.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
			voxel_densities.fill(0);
			voxel_materials.fill(0);
			voxel_is_edited.fill(0);
			
			break;
	}
}



bool BlockVoxelData::set_voxel(int voxel_index, int voxel_density, int voxel_material, bool is_voxel_edited) {
	if (voxel_index < 0 || voxel_index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
		return false;
	}
	
	if (voxel_density < 0 || voxel_material < 0 || voxel_density > UINT8_MAX || voxel_material > UINT8_MAX) {
		return false;
	}
	
	if (current_resource_state == BlockVoxelData::ResourceState::Uninitialized || current_resource_state == BlockVoxelData::ResourceState::Empty) {
		return false;
	}
	
	if (current_resource_state == BlockVoxelData::ResourceState::Sparse) {
		int *voxel_array_index_ptr = voxel_index_lookup.getptr(voxel_index);
		
		if (voxel_array_index_ptr == nullptr) {
			int voxel_array_index = voxel_indices.size();
			
			voxel_densities.append(voxel_density);
			voxel_materials.append(voxel_material);
			voxel_indices.append(voxel_index);
			
			voxel_index_lookup[voxel_index] = voxel_array_index;
		} else {
			int voxel_array_index = *voxel_array_index_ptr;
			
			voxel_densities.set(voxel_array_index, voxel_density);
			voxel_materials.set(voxel_array_index, voxel_material);
		}
		
		return true;
	} else {
		voxel_densities.set(voxel_index, voxel_density);
		voxel_materials.set(voxel_index, voxel_material);
		
		if (is_voxel_edited) {
			request_edited_voxel_count_change(voxel_index, true);
			voxel_is_edited.set(voxel_index, 1);
		} else {
			request_edited_voxel_count_change(voxel_index, false);
			voxel_is_edited.set(voxel_index, 0);
		}
		
		return true;
	}
}

bool BlockVoxelData::remove_voxel(int voxel_index) {
	if (voxel_index < 0 || voxel_index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
		return false;
	}
	
	if (current_resource_state != BlockVoxelData::ResourceState::Sparse) {
		return false;
	}
	
	int *voxel_array_index_ptr = voxel_index_lookup.getptr(voxel_index);
	
	if (voxel_array_index_ptr == nullptr) {
		return false;
	}
	
	int voxel_array_index = *voxel_array_index_ptr;
	int last_voxel_index = voxel_indices.size() - 1;
	
	if (voxel_array_index != last_voxel_index) {
		int moved_voxel_index = voxel_indices[last_voxel_index];
		
		voxel_densities.set(voxel_array_index, voxel_densities[last_voxel_index]);
		voxel_materials.set(voxel_array_index, voxel_materials[last_voxel_index]);
		voxel_indices.set(voxel_array_index, moved_voxel_index);
		
		voxel_index_lookup[moved_voxel_index] = voxel_array_index;
	}
	
	voxel_index_lookup.erase(voxel_index);
	
	voxel_densities.resize(last_voxel_index);
	voxel_materials.resize(last_voxel_index);
	voxel_indices.resize(last_voxel_index);
	
	return true;
}

Ref<Voxel> BlockVoxelData::get_voxel(int voxel_index) {
	Ref<Voxel> voxel = memnew(Voxel);
	voxel->is_voxel_valid = false;
	voxel->density = 0;
	voxel->material = 0;
	
	if (voxel_index < 0 || voxel_index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
		return voxel;
	}
	
	if (current_resource_state == BlockVoxelData::ResourceState::Uninitialized || current_resource_state == BlockVoxelData::ResourceState::Empty) {
		return voxel;
	}
	
	if (current_resource_state == BlockVoxelData::ResourceState::Sparse) {
		int *voxel_array_index_ptr = voxel_index_lookup.getptr(voxel_index);
		
		if (voxel_array_index_ptr == nullptr) {
			return voxel;
		}
		
		int voxel_array_index = *voxel_array_index_ptr;
		
		voxel->is_voxel_valid = true;
		voxel->density = voxel_densities[voxel_array_index];
		voxel->material = voxel_materials[voxel_array_index];
		
		return voxel;
	} else {
		voxel->is_voxel_valid = true;
		voxel->density = voxel_densities[voxel_index];
		voxel->material = voxel_materials[voxel_index];
		
		return voxel;
	}
}

int BlockVoxelData::get_voxel_count() {
	switch (current_resource_state) {
		case BlockVoxelData::ResourceState::Uninitialized:
			return -1;
			break;
		case BlockVoxelData::ResourceState::Empty:
			return 0;
			break;
		case BlockVoxelData::ResourceState::Sparse:
			return voxel_indices.size();
			break;
		case BlockVoxelData::ResourceState::Dense:
			return VoxelConstants::BLOCK_VOXEL_VOLUME;
			break;
	}
	
	return -1;
}

PackedByteArray BlockVoxelData::get_voxel_densities() {
	return voxel_densities;
}

PackedByteArray BlockVoxelData::get_voxel_materials() {
	return voxel_materials;
}

PackedInt32Array BlockVoxelData::get_voxel_indices() {
	return voxel_indices;
}

PackedByteArray BlockVoxelData::get_edited_voxels() {
	return voxel_is_edited;
}

int BlockVoxelData::get_edited_voxel_count() {
	return edited_voxel_count;
}

bool BlockVoxelData::set_bulk_voxels_sparse(const PackedByteArray &densities, const PackedByteArray &materials, const PackedInt32Array &indices) {
	if (current_resource_state != BlockVoxelData::ResourceState::Sparse) {
		return false;
	}
	
	int voxel_density_count = densities.size();
	int voxel_material_count = materials.size();
	int voxel_index_count = indices.size();
	
	if (voxel_density_count <= 0 || voxel_material_count <= 0 || voxel_index_count <= 0 || voxel_density_count > VoxelConstants::BLOCK_VOXEL_VOLUME || voxel_material_count > VoxelConstants::BLOCK_VOXEL_VOLUME || voxel_index_count > VoxelConstants::BLOCK_VOXEL_VOLUME) {
		return false;
	}
	
	if (voxel_density_count != voxel_material_count || voxel_material_count != voxel_index_count) {
		return false;
	}
	
	HashMap<int, int> voxel_index_staging_lookup;
	
	for (int i = 0; i < voxel_index_count; i++) {
		int voxel_index = indices[i];
		
		if (voxel_index < 0 || voxel_index >= VoxelConstants::BLOCK_VOXEL_VOLUME || voxel_index_staging_lookup.has(voxel_index)) {
			return false;
		}
		
		voxel_index_staging_lookup[voxel_index] = i;
	}
	
	voxel_index_lookup = voxel_index_staging_lookup;
	voxel_densities = densities;
	voxel_materials = materials;
	voxel_indices = indices;
	
	return true;
}

bool BlockVoxelData::set_bulk_voxels_dense(const PackedByteArray &densities, const PackedByteArray &materials, const PackedByteArray &edited_voxels) {
	if (current_resource_state != BlockVoxelData::ResourceState::Dense) {
		return false;
	}
	
	if (densities.size() != VoxelConstants::BLOCK_VOXEL_VOLUME || materials.size() != VoxelConstants::BLOCK_VOXEL_VOLUME || voxel_is_edited.size() != VoxelConstants::BLOCK_VOXEL_VOLUME) {
		return false;
	}
	
	voxel_densities = densities;
	voxel_materials = materials;
	voxel_is_edited = edited_voxels;
	edited_voxel_count = 0;
	
	for (int is_voxel_edited : voxel_is_edited) {
		edited_voxel_count += is_voxel_edited;
	}
	
	return true;
}

bool BlockVoxelData::merge_sparse_block_data_into_self(const Ref<BlockVoxelData> &sparse_block_voxel_data) {
	if (sparse_block_voxel_data.is_null()) {
		return false;
	}
	
	if (current_resource_state == BlockVoxelData::ResourceState::Uninitialized || current_resource_state ==  BlockVoxelData::ResourceState::Empty) {
		return false;
	}
	
	if (sparse_block_voxel_data->get_resource_state() != BlockVoxelData::ResourceState::Sparse) {
		return false;
	}
	
	const PackedInt32Array &source_indices = sparse_block_voxel_data->voxel_indices;
	const PackedByteArray &source_densities = sparse_block_voxel_data->voxel_densities;
	const PackedByteArray &source_materials = sparse_block_voxel_data->voxel_materials;
	
	int voxel_count = source_indices.size();
	
	for (int i = 0; i < voxel_count; i++) {
		int voxel_index = source_indices[i];
		uint8_t density = source_densities[i];
		uint8_t material = source_materials[i];
		
		set_voxel(voxel_index, density, material, true);
	}
	
	return true;
}

bool BlockVoxelData::generate_dense_block_data(bool use_terrain_generator) {
	initialize_voxel_data(BlockVoxelData::ResourceState::Dense);
	
	if (!use_terrain_generator) {
		return true;
	}
	
	/*for (int voxel_index = 0; voxel_index < VoxelConstants::BLOCK_VOXEL_VOLUME; voxel_index++) {
		// future code to generate voxel_index when i make the generator
	}*/
	
	return true;
}

bool BlockVoxelData::convert_sparse_to_dense(bool use_terrain_generator) {
	if (current_resource_state != BlockVoxelData::ResourceState::Sparse) {
		return false;
	}
	
	PackedByteArray saved_densities = voxel_densities;
	PackedByteArray saved_materials = voxel_materials;
	PackedInt32Array saved_indices = voxel_indices;
	
	initialize_voxel_data(BlockVoxelData::ResourceState::Dense);
	
	if (use_terrain_generator) {
		HashMap<int, int> saved_index_lookup;
		int sparse_voxel_count = saved_indices.size();
		for (int i = 0; i < sparse_voxel_count; i++) {
			saved_index_lookup[saved_indices[i]] = i;
		}
		
		for (int voxel_index = 0; voxel_index < VoxelConstants::BLOCK_VOXEL_VOLUME; voxel_index++) {
			if (saved_index_lookup.has(voxel_index)) {
				continue;
			}
			
			// future code to generate voxel_index when i make the generator
		}
	}
	
	int sparse_voxel_count = saved_indices.size();
	
	for (int i = 0; i < sparse_voxel_count; i++) {
		int voxel_index = saved_indices[i];
		uint8_t density = saved_densities[i];
		uint8_t material = saved_materials[i];
		
		set_voxel(voxel_index, density, material, true);
	}
	
	return true;
}

bool BlockVoxelData::convert_dense_to_sparse() {
	if (current_resource_state != BlockVoxelData::ResourceState::Dense) {
		return false;
	}
	
	PackedByteArray saved_densities = voxel_densities;
	PackedByteArray saved_materials = voxel_materials;
	PackedByteArray saved_edited_voxels = voxel_is_edited;
	
	initialize_voxel_data(BlockVoxelData::ResourceState::Sparse);
	
	for (int i = 0; i < VoxelConstants::BLOCK_VOXEL_VOLUME; i++) {
		int voxel_index = i;
		
		if (saved_edited_voxels[voxel_index] == 0) {
			continue;
		}
		
		if (voxel_index < 0 || voxel_index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
			continue;
		}
		
		uint8_t density = saved_densities[voxel_index];
		uint8_t material = saved_materials[voxel_index];
		
		set_voxel(voxel_index, density, material, true);
	}
	
	return true;
}



void BlockVoxelData::request_edited_voxel_count_change(int voxel_index, bool is_adding) {
	if (current_resource_state != BlockVoxelData::ResourceState::Dense) {
		return;
	}
	
	if (is_adding) {
		if (voxel_is_edited[voxel_index] == 1) {
			return;
		}
		
		if (edited_voxel_count >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
			edited_voxel_count = VoxelConstants::BLOCK_VOXEL_VOLUME;
			return;
		}
		
		edited_voxel_count += 1;
	} else {
		if (voxel_is_edited[voxel_index] == 0) {
			return;
		}
		
		if (edited_voxel_count <= 0) {
			edited_voxel_count = 0;
			return;
		}
		
		edited_voxel_count -= 1;
	}
}

void BlockVoxelData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_resource_state"), &BlockVoxelData::get_resource_state);
	ClassDB::bind_method(D_METHOD("initialize_voxel_data", "resource_state"), &BlockVoxelData::initialize_voxel_data);
	ClassDB::bind_method(D_METHOD("set_voxel", "voxel_index", "voxel_density", "voxel_material", "is_voxel_edited"), &BlockVoxelData::set_voxel, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("remove_voxel", "voxel_index"), &BlockVoxelData::remove_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel", "voxel_index"), &BlockVoxelData::get_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel_count"), &BlockVoxelData::get_voxel_count);
	ClassDB::bind_method(D_METHOD("get_voxel_densities"), &BlockVoxelData::get_voxel_densities);
	ClassDB::bind_method(D_METHOD("get_voxel_materials"), &BlockVoxelData::get_voxel_materials);
	ClassDB::bind_method(D_METHOD("get_voxel_indices"), &BlockVoxelData::get_voxel_indices);
	ClassDB::bind_method(D_METHOD("get_edited_voxels"), &BlockVoxelData::get_edited_voxels);
	ClassDB::bind_method(D_METHOD("get_edited_voxel_count"), &BlockVoxelData::get_edited_voxel_count);
	ClassDB::bind_method(D_METHOD("set_bulk_voxels_sparse", "densities", "materials", "indices"), &BlockVoxelData::set_bulk_voxels_sparse);
	ClassDB::bind_method(D_METHOD("set_bulk_voxels_dense", "densities", "materials", "edited_voxels"), &BlockVoxelData::set_bulk_voxels_dense);
	ClassDB::bind_method(D_METHOD("merge_sparse_block_data_into_self", "sparse_block_voxel_data"), &BlockVoxelData::merge_sparse_block_data_into_self);
	ClassDB::bind_method(D_METHOD("generate_dense_block_data", "use_terrain_generator"), &BlockVoxelData::generate_dense_block_data);
	ClassDB::bind_method(D_METHOD("convert_sparse_to_dense", "use_terrain_generator"), &BlockVoxelData::convert_sparse_to_dense);
	ClassDB::bind_method(D_METHOD("convert_dense_to_sparse"), &BlockVoxelData::convert_dense_to_sparse);
	
	BIND_ENUM_CONSTANT(Uninitialized);
	BIND_ENUM_CONSTANT(Empty);
	BIND_ENUM_CONSTANT(Sparse);
	BIND_ENUM_CONSTANT(Dense);
}
