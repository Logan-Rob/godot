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
	
	PackedByteArray voxel_densities;
	PackedByteArray voxel_materials;
	PackedInt32Array voxel_indices;
	PackedByteArray voxel_is_edited;
	
	static void _bind_methods();
};

VARIANT_ENUM_CAST(BlockVoxelData::ResourceState);


/*
 * should i add a new resource state format? if so i have no idea what to call it.
 * the reason is that this is the memory representation of how blocks (chunks) are stored on the disk.
 * when you read a block from the disk it will return one BlockVoxelData object, and that object could be sparse or dense (there are 2 formats to save space on the disk) depending on whats on the disk
 * 
 * another bit of info to know is this BlockVoxelData object is the unit of storage of a block within memory for reading from the disk, but it is also the persistent representation of a block in memory until it is unloaded from memory.
 * but when it is actually being used as a loaded blocks representaion it MUST be dense and include all voxels
 * 
 * the pipeline for loading a chunk is check if its on the disk, if its not generate a dense BlockVoxelData object with the world generator module and use that as the loaded persistent representation in memory for the player to modify until it gets unloaded
 * if it is on the disk and its dense, its easy, just simply use the returned dense BlockVoxelData object as the persistent representation
 * if its sparse (containing only edited voxels) it needs to become dense and contain all voxels for the player to edit in memory.
 * 
 * there are 2 ways to do it, either generate a new dense BlockVoxelData object from the world generator and merge the sparse BlockVoxelData object from the disk ontop of it
 * the problem with this method is the world generator generates some voxels that are going to merged over with the sparse (this could be kindof expensive because i plan to have detailed planet generating overlaying multiple noise functions for features each biome)
 * 
 * the other way is i could make a hybrid sparse dense format that acts like a sparse one with the voxel_index_lookup and voxel_indices array still working the same and not being empty,
 * but instead of the voxel_densities, voxel_materials, and voxel_indices actually being sparse and appending, they are instead locked at a dense array size and filled with 0s with only the sparse indexes not being 0
 * that way its dense and the world generator can generate to the dense array, and it still has the knowledge of which voxels are sparse and skips over those preserving them, then i convert this intermediate format into a true dense BlockVoxelData object by changing the tag and shrinking the voxel_index_lookup and voxel_indices to zero to save space
 */
