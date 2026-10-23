#pragma once

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include <cstdint>


class Voxel : public RefCounted {
	GDCLASS(Voxel, RefCounted);

public:
	bool is_voxel_valid = false;
	uint8_t density = 0;
	uint8_t material = 0;

	bool get_is_voxel_valid() const { return is_voxel_valid; }
	void set_is_voxel_valid(bool p_valid) { is_voxel_valid = p_valid; }

	int get_density() const { return density; }
	void set_density(int p_density) { density = p_density; }

	int get_material() const { return material; }
	void set_material(int p_material) { material = p_material; }
	
protected:
	static void _bind_methods();
};



class BlockVoxelData : public Resource {
	GDCLASS(BlockVoxelData, Resource);

public:
	enum ResourceState {
		Uninitialized,
		Empty,
		Sparse,
		Dense
	};
	
	ResourceState get_resource_state();
	void initialize_voxel_data(ResourceState resource_state);
	
	bool set_voxel(int voxel_index, int voxel_density, int voxel_material, bool is_voxel_edited = true);
	bool remove_voxel(int voxel_index);
	Ref<Voxel> get_voxel(int voxel_index);
	int get_voxel_count();
	PackedByteArray get_voxel_densities();
	PackedByteArray get_voxel_materials();
	PackedInt32Array get_voxel_indices();
	PackedByteArray get_edited_voxels();
	bool set_bulk_voxels_sparse(const PackedByteArray &densities, const PackedByteArray &materials, const PackedInt32Array &indices);
	bool set_bulk_voxels_dense(const PackedByteArray &densities, const PackedByteArray &materials, const PackedByteArray &edited_voxels);
	bool merge_sparse_block_data_into_self(const Ref<BlockVoxelData> &sparse_block_voxel_data);
	bool generate_dense_block_data(bool use_terrain_generator);
	bool convert_sparse_to_dense(bool use_terrain_generator);
	bool convert_dense_to_sparse();

protected:
	ResourceState current_resource_state = ResourceState::Uninitialized;
	
	HashMap<int, int> voxel_index_lookup;
	int edited_voxel_count;
	
	PackedByteArray voxel_densities;
	PackedByteArray voxel_materials;
	PackedInt32Array voxel_indices;
	PackedByteArray voxel_is_edited;
	
	void request_edited_voxel_count_change(int voxel_index, bool is_adding);
	static void _bind_methods();
};

VARIANT_ENUM_CAST(BlockVoxelData::ResourceState);
