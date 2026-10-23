#pragma once

#include "voxel_constants.h"
#include "core/variant/variant.h"
#include "core/io/file_access.h"
#include "core/math/vector3i.h"
#include <cstdint>
#include "block_voxel_resource.h"

#include "scene/3d/node_3d.h"

class FileManager : public Node3D{
	GDCLASS(FileManager, Node3D);

protected:
	static void _bind_methods();

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

	bool write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelResource::BlockVoxelData &voxel_data);
	bool read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelResource::BlockVoxelData &voxel_data);

	int create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position);
	int remove_block_header(const Ref<FileAccess> &header_file, Vector3i block_position);
	void trim_free_tail_headers(const Ref<FileAccess> &header_file);

	void compact_header_file(const Ref<FileAccess> &header_file);
	void compact_payload_file(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file);

	bool merge_voxel_data(BlockVoxelResource::SparseVoxelData &input_voxel_data, BlockVoxelResource::BlockVoxelData &dst_base_voxel_data);
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

public:
	bool save_voxels(Vector3i block_position, Ref<BlockVoxelResource> voxel_resource);
	bool load_voxels(Vector3i block_position, Ref<BlockVoxelResource> voxel_resource);

	void compact_files();
	void store_header_indices();

	//String get_block_header_file_directory();
	//String get_block_payload_file_directory();
	//bool initialize_file_storage(String header_file_directory, String payload_file_directory);
};
