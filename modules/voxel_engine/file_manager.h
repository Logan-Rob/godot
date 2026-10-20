#pragma once

#include "voxel_constants.h"
#include "core/variant/variant.h"
#include "core/io/file_access.h"
#include "core/math/vector3i.h"
#include <cstdint>

class FileManager {
public:
	
private:
	static constexpr int HEADER_FILE_HEADER_LENGTH = 16;
	static constexpr int PAYLOAD_FILE_HEADER_LENGTH = 16;

	static constexpr int BLOCK_HEADER_LENGTH = 28;
	static constexpr int BLOCK_HEADER_POSITION_INDEX = 0; // 12 Bytes
	static constexpr int BLOCK_HEADER_PAYLOAD_POINTER_INDEX = 12;// 8 Bytes
	static constexpr int BLOCK_HEADER_PAYLOAD_LENGTH_INDEX = 20; // 4 Bytes
	static constexpr int BLOCK_HEADER_BOOLEAN_BITSET_INDEX = 24; // 1 Byte
	static constexpr int BLOCK_HEADER_PADDING_BYTES_INDEX = 25; // 1 Byte
	static constexpr int BLOCK_HEADER_VOXEL_COUNT_INDEX = 26; // 2 Bytes

	static constexpr int SPARSE_DENSE_VOXEL_THRESHOLD = 16384;

	String block_header_file_directory = "user://saves/test_save/terrain.vbh";
	String block_payload_file_directory = "user://saves/test_save/terrain.vpl";
	bool is_header_file_initialized = false;
	bool is_payload_file_initialized = false;

	HashMap<Vector3i, int> header_index_lookup;
	PackedInt32Array free_header_indices;

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

	struct BlockHeaderBitset {
		bool is_active = false;
		bool is_dense = false;
		bool is_write_in_progress = false;
		bool reserved_3 = false;
		bool reserved_4 = false;
		bool reserved_5 = false;
		bool reserved_6 = false;
		bool reserved_7 = false;
	};

	bool save_voxels(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, Vector3i block_position, BlockVoxelData &voxel_data);
	bool load_voxels(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, Vector3i block_position, BlockVoxelData &voxel_data);

	bool write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelData &voxel_data);
	bool read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelData &voxel_data);

	int create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position);
	int remove_block_header(const Ref<FileAccess> &header_file, Vector3i block_position);
	void trim_free_tail_headers(const Ref<FileAccess> &header_file);

	bool merge_voxel_data(SparseVoxelData &input_voxel_data, BlockVoxelData &dst_base_voxel_data);
	int get_total_block_header_count(const Ref<FileAccess> &header_file);

	int convert_header_index_to_byte_index(const int header_index);
	void move_cursor_to_header_property(const Ref<FileAccess> &header_file, const int header_byte_index, const int header_property);
	Vector3i get_header_position(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_position(const Ref<FileAccess> &header_file, const int header_byte_index, const Vector3i block_position);
	uint64_t get_header_payload_pointer(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_payload_pointer(const Ref<FileAccess> &header_file, const int header_byte_index, const uint64_t payload_pointer);
	uint32_t get_header_payload_length(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_payload_length(const Ref<FileAccess> &header_file, const int header_byte_index, const uint32_t payload_length);
	BlockHeaderBitset get_header_bool_bitset(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_bool_bitset(const Ref<FileAccess> &header_file, const int header_byte_index, const BlockHeaderBitset &bool_bitset);
	uint8_t get_header_padding_bytes(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_padding_bytes(const Ref<FileAccess> &header_file, const int header_byte_index, const uint8_t padding_bytes);
	uint16_t get_header_voxel_count(const Ref<FileAccess> &header_file, const int header_byte_index);
	void store_header_voxel_count(const Ref<FileAccess> &header_file, const int header_byte_index, const uint16_t voxel_count);
};
