#pragma once

#include "core/io/resource.h"
#include "voxel_constants.h"

class BlockVoxelResource : public Resource {
	GDCLASS(BlockVoxelResource, Resource);
	friend class FileManager;

private:
	struct DenseVoxelData {
		PackedByteArray densities;
		PackedByteArray materials;
		
		DenseVoxelData() {
			densities.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
			materials.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
		}
		
		void set_voxel(uint16_t index, uint8_t density, uint8_t material) {
			if (index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
				return;
			}
			
			densities.set(index, density);
			materials.set(index, material);
		}
	};

	struct SparseVoxelData {
		PackedByteArray densities;
		PackedByteArray materials;
		PackedInt32Array indices;
		HashMap<uint16_t, uint16_t> index_lookup;
		
		void set_voxel(uint16_t index, uint8_t density, uint8_t material) {
			if (index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
				return;
			}
			
			if (index_lookup.has(index)) {
				uint16_t *array_index = index_lookup.getptr(index);
				densities.set(*array_index, density);
				materials.set(*array_index, material);
			} else {
				index_lookup[index] = indices.size();
				densities.append(density);
				materials.append(material);
				indices.append(static_cast<int32_t>(index));
			}
		}
		
		void remove_voxel(uint16_t index) {
			if (index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
				return;
			}
			
			uint16_t *array_index_ptr = index_lookup.getptr(index);
			if (array_index_ptr == nullptr) {
				return;
			}
			
			const uint16_t array_index = *array_index_ptr;
			const uint16_t last_index = indices.size() - 1;
			
			if (array_index != last_index) {
				const uint16_t moved_voxel_index = static_cast<uint16_t>(indices[last_index]);
				
				densities.set(array_index, densities[last_index]);
				materials.set(array_index, materials[last_index]);
				indices.set(array_index, moved_voxel_index);
				
				index_lookup[moved_voxel_index] = static_cast<uint16_t>(array_index);
			}
			
			densities.resize(last_index);
			materials.resize(last_index);
			indices.resize(last_index);
			
			index_lookup.erase(index);
		}
		
		void set_bulk_voxels(const PackedByteArray &bulk_voxel_densities, const PackedByteArray &bulk_voxel_materials, const PackedInt32Array bulk_voxel_indices) {
			densities = bulk_voxel_densities;
			materials = bulk_voxel_materials;
			indices = bulk_voxel_indices;
			
			index_lookup.clear();
			
			for (uint16_t i = 0; i < indices.size(); i++) {
				uint16_t index = static_cast<uint16_t>(indices[i]);
				index_lookup[index] = i;
			}
		}
	};
	
	struct BlockVoxelData {
		bool is_initialized = false;
		bool is_dense = false;
		DenseVoxelData *dense_voxel_data_ptr = nullptr;
		SparseVoxelData *sparse_voxel_data_ptr = nullptr;
		
		BlockVoxelData() = default;
		
		BlockVoxelData(const BlockVoxelData &) = delete;
		BlockVoxelData &operator=(const BlockVoxelData &) = delete;
		BlockVoxelData(BlockVoxelData &&) = delete;
		BlockVoxelData &operator=(BlockVoxelData &&) = delete;
		
		void initialize_dense_voxel_data() {
			if (is_initialized) {
				return;
			}
			
			is_dense = true;
			dense_voxel_data_ptr = memnew(DenseVoxelData);
			is_initialized = true;
		}
		
		void initialize_sparse_voxel_data() {
			if (is_initialized) {
				return;
			}
			
			is_dense = false;
			sparse_voxel_data_ptr = memnew(SparseVoxelData);
			is_initialized = true;
		}
		
		void clear_voxel_data() {
			if (dense_voxel_data_ptr != nullptr) {
				memdelete(dense_voxel_data_ptr);
				dense_voxel_data_ptr = nullptr;
			}
			
			if (sparse_voxel_data_ptr != nullptr) {
				memdelete(sparse_voxel_data_ptr);
				sparse_voxel_data_ptr = nullptr;
			}
			
			is_dense = false;
			is_initialized = false;
		}
		
		~BlockVoxelData() {
			clear_voxel_data();
		}
	};
	
	BlockVoxelData voxel_data;

protected:
	static void _bind_methods();

public:
	
};
