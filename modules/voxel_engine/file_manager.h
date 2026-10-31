#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "voxel_constants.h"
#include "block_voxel_data.h"

class FileManager {
protected:
	static constexpr int HEADER_FILE_HEADER_LENGTH = 28; // 26 + 2 padding bytes
	static constexpr int HEADER_FILE_HEADER_VERSION_INDEX = 0;     // 2 Bytes
	static constexpr int HEADER_FILE_HEADER_MAGIC_NUMBER = 2;      // 4 Bytes
	static constexpr int HEADER_FILE_HEADER_CHECKSUM_INDEX = 6;    // 16 Bytes
	static constexpr int HEADER_FILE_HEADER_WORLD_SEED_INDEX = 22; // 4 Bytes

	static constexpr int PAYLOAD_FILE_HEADER_LENGTH = 28; // 26 + 2 padding bytes
	static constexpr int PAYLOAD_FILE_HEADER_VERSION_INDEX = 0;     // 2 Bytes
	static constexpr int PAYLOAD_FILE_HEADER_MAGIC_NUMBER = 2;      // 4 Bytes
	static constexpr int PAYLOAD_FILE_HEADER_CHECKSUM_INDEX = 6;    // 16 Bytes
	static constexpr int PAYLOAD_FILE_HEADER_WORLD_SEED_INDEX = 22; // 4 Bytes

	static constexpr int BLOCK_HEADER_LENGTH = 28;
	static constexpr int BLOCK_HEADER_POSITION_INDEX = 0;         // 12 Bytes
	static constexpr int BLOCK_HEADER_PAYLOAD_POINTER_INDEX = 12; // 8 Bytes
	static constexpr int BLOCK_HEADER_PAYLOAD_LENGTH_INDEX = 20;  // 4 Bytes
	static constexpr int BLOCK_HEADER_BOOLEAN_BITSET_INDEX = 24;  // 1 Byte
	static constexpr int BLOCK_HEADER_PADDING_BYTES_INDEX = 25;   // 1 Byte
	static constexpr int BLOCK_HEADER_VOXEL_COUNT_INDEX = 26;     // 2 Bytes

	static constexpr int SPARSE_DENSE_VOXEL_THRESHOLD = 16384;
	static constexpr int SPARSE_VOXEL_SIZE = 4;
	static constexpr int DENSE_VOXEL_SIZE = 2;

	struct BlockHeaderBitset {
		bool is_active = false;
		bool is_dense = false;
		bool reserved_2 = false;
		bool reserved_3 = false;
		bool reserved_4 = false;
		bool reserved_5 = false;
		bool reserved_6 = false;
		bool reserved_7 = false;
	};

	String block_header_file_directory = "user://saves/test_save/terrain.vbh";
	String block_payload_file_directory = "user://saves/test_save/terrain.vpl";
	bool is_header_file_initialized = false;
	bool is_payload_file_initialized = false;

	HashMap<Vector3i, int> header_index_lookup;
	PackedInt32Array free_header_indices;
	
	bool write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, const Ref<BlockVoxelData> &voxel_data);
	bool read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, const Ref<BlockVoxelData> &voxel_data);

	int create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position, bool is_dense);
	int remove_block_header(const Ref<FileAccess> &header_file, Vector3i block_position);
	bool trim_free_tail_headers(const Ref<FileAccess> &header_file);

	PackedByteArray convert_int16_array_to_packed_byte_array(const PackedInt32Array &int16_array);
	PackedInt32Array convert_packed_byte_array_to_int16_array(const PackedByteArray &byte_array);
	int get_total_block_header_count(const Ref<FileAccess> &header_file);
	int convert_header_index_to_byte_index(const int header_index);
	PackedByteArray get_block_header_buffer(const Ref<FileAccess> &header_file, const int header_byte_index);

	Vector3i get_header_position(const PackedByteArray &header_buffer) const;
	void store_header_position(PackedByteArray &header_buffer, const Vector3i &block_position);
	uint64_t get_header_payload_pointer(const PackedByteArray &header_buffer) const;
	void store_header_payload_pointer(PackedByteArray &header_buffer, uint64_t payload_pointer);
	uint32_t get_header_payload_length(const PackedByteArray &header_buffer) const;
	void store_header_payload_length(PackedByteArray &header_buffer, uint32_t payload_length);
	FileManager::BlockHeaderBitset get_header_bool_bitset(const PackedByteArray &header_buffer) const;
	void store_header_bool_bitset(PackedByteArray &header_buffer, const FileManager::BlockHeaderBitset &bool_bitset);
	uint8_t get_header_padding_bytes(const PackedByteArray &header_buffer) const;
	void store_header_padding_bytes(PackedByteArray &header_buffer, uint8_t padding_bytes);
	uint16_t get_header_voxel_count(const PackedByteArray &header_buffer) const;
	void store_header_voxel_count(PackedByteArray &header_buffer, uint16_t voxel_count);

public:
	bool save_voxels(Vector3i block_position, const Ref<BlockVoxelData> &voxel_data);
	Ref<BlockVoxelData> load_voxels(Vector3i block_position);

	bool compact_files();
};
