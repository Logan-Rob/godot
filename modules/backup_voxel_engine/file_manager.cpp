#include "file_manager.h"
#include "voxel_constants.h"
#include "core/math/vector3i.h"
#include "core/variant/variant.h"
#include "core/object/class_db.h"


bool FileManager::save_voxels(Vector3i block_position, Ref<BlockVoxelResource> voxel_resource) {
	Ref<FileAccess> header_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	Ref<FileAccess> payload_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	
	BlockVoxelResource::BlockVoxelData &voxel_data = voxel_resource->voxel_data;
	
	if (!voxel_data.is_initialized) {
		header_file->close();
		payload_file->close();
		return false;
	}
	
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	int header_index;
	bool is_new_header = header_index_ptr == nullptr;
	
	if (is_new_header) {
		header_index = create_block_header(header_file, block_position);
	} else {
		header_index = *header_index_ptr;
	}
	
	if (header_index < 0) {
		header_file->close();
		payload_file->close();
		return false;
	}
	
	BlockVoxelResource::BlockVoxelData base_voxel_data;
	
	if (!read_voxels_from_disk(header_file, payload_file, header_index, base_voxel_data)) {
		header_file->close();
		payload_file->close();
		return false;
	}
	
	if (voxel_data.is_dense || is_new_header) {
		if (!write_voxels_to_disk(header_file, payload_file, header_index, voxel_data)) {
			header_file->close();
			payload_file->close();
			return false;
		}
	} else {
		if (!merge_voxel_data(*voxel_data.sparse_voxel_data_ptr, base_voxel_data)) {
			header_file->close();
			payload_file->close();
			return false;
		}
		
		if (!write_voxels_to_disk(header_file, payload_file, header_index, base_voxel_data)) {
			header_file->close();
			payload_file->close();
			return false;
		}
	}
	
	header_file->close();
	payload_file->close();
	return true;
}

bool FileManager::load_voxels(Vector3i block_position, Ref<BlockVoxelResource> voxel_resource) {
	Ref<FileAccess> header_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	Ref<FileAccess> payload_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	
	BlockVoxelResource::BlockVoxelData &voxel_data = voxel_resource->voxel_data;
	
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	
	if (header_index_ptr == nullptr) {
		header_file->close();
		payload_file->close();
		return false;
	}
	
	voxel_data.clear_voxel_data();
	const int header_index = *header_index_ptr;
	
	if (!read_voxels_from_disk(header_file, payload_file, header_index, voxel_data)) {
		header_file->close();
		payload_file->close();
		return false;
	}
	
	header_file->close();
	payload_file->close();
	return true;
}



bool FileManager::write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelResource::BlockVoxelData &voxel_data) {
	if (!voxel_data.is_initialized || !is_header_file_initialized || !is_payload_file_initialized || !header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		return false;
	}
	
	int header_byte_index = convert_header_index_to_byte_index(header_index);
	uint16_t block_voxel_count;
	PackedByteArray voxel_byte_array;
	
	BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
	bool old_format_boolean = bool_bitset.is_dense;
	bool_bitset.is_write_in_progress = true;
	store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
	
	if (voxel_data.is_dense) {
		bool_bitset.is_dense = true;
		
		voxel_byte_array = voxel_data.dense_voxel_data_ptr->densities;
		voxel_byte_array.append_array(voxel_data.dense_voxel_data_ptr->materials);
		
		block_voxel_count = VoxelConstants::BLOCK_VOXEL_VOLUME;
	} else {
		bool_bitset.is_dense = false;
		
		voxel_byte_array = voxel_data.sparse_voxel_data_ptr->densities;
		voxel_byte_array.append_array(voxel_data.sparse_voxel_data_ptr->materials);
		
		const PackedInt32Array &indices = voxel_data.sparse_voxel_data_ptr->indices;
		const int64_t voxel_index_count = indices.size();
		PackedByteArray voxel_index_bytes;
		voxel_index_bytes.resize(voxel_index_count * 2);
		uint8_t *voxel_index_bytes_ptr = voxel_index_bytes.ptrw();
		
		for (int64_t i = 0; i < voxel_index_count; i++) {
			if (indices[i] < 0 || indices[i] > UINT16_MAX) {
				bool_bitset.is_dense = old_format_boolean;
				bool_bitset.is_write_in_progress = false;
				store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
				return false;
			}
			
			const uint16_t value = static_cast<uint16_t>(indices[i]);
			voxel_index_bytes_ptr[i * 2] = static_cast<uint8_t>(value & 0xFF);
			voxel_index_bytes_ptr[i * 2 + 1] = static_cast<uint8_t>(value >> 8);
		}
		
		voxel_byte_array.append_array(voxel_index_bytes);
		block_voxel_count = voxel_index_count;
	}
	
	PackedByteArray compressed_voxel_byte_array;
	compressed_voxel_byte_array.resize(Compression::get_max_compressed_buffer_size(voxel_byte_array.size(), Compression::MODE_ZSTD));
	int64_t compressed_size = Compression::compress(compressed_voxel_byte_array.ptrw(), voxel_byte_array.ptr(), voxel_byte_array.size(), Compression::MODE_ZSTD);
	
	if (compressed_size < 0) {
		bool_bitset.is_dense = old_format_boolean;
		bool_bitset.is_write_in_progress = false;
		store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
		return false;
	}
	
	compressed_voxel_byte_array.resize(compressed_size);
	const int64_t padded_byte_count = (8 - (compressed_size % 8)) % 8;
	const int64_t padded_compressed_size = compressed_size + padded_byte_count;
	compressed_voxel_byte_array.resize(padded_compressed_size);
	
	const uint64_t old_payload_pointer = get_header_payload_pointer(header_file, header_byte_index);
	const uint32_t old_payload_length = get_header_payload_length(header_file, header_byte_index);
	
	if (padded_compressed_size > old_payload_length || old_payload_pointer <= HEADER_FILE_HEADER_LENGTH) {
		store_header_padding_bytes(header_file, header_byte_index, padded_byte_count);
		store_header_voxel_count(header_file, header_byte_index, block_voxel_count);
		store_header_payload_pointer(header_file, header_byte_index, payload_file->get_length());
		store_header_payload_length(header_file, header_byte_index, padded_compressed_size);
		payload_file->seek_end();
	} else {
		store_header_voxel_count(header_file, header_byte_index, block_voxel_count);
		store_header_padding_bytes(header_file, header_byte_index, padded_byte_count);
		store_header_payload_length(header_file, header_byte_index, padded_compressed_size);
		payload_file->seek(old_payload_pointer);
	}
	
	payload_file->store_buffer(compressed_voxel_byte_array);
	
	bool_bitset.is_write_in_progress = false;
	store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
	return true;
}

bool FileManager::read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelResource::BlockVoxelData &voxel_data) {
	if (voxel_data.is_initialized || !is_header_file_initialized || !is_payload_file_initialized || !header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		return false;
	}
	
	int header_byte_index = convert_header_index_to_byte_index(header_index);
	BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
	
	if (bool_bitset.is_write_in_progress || !bool_bitset.is_active) {
		return false;
	}
	
	uint32_t compressed_payload_length = get_header_payload_length(header_file, header_byte_index) - get_header_padding_bytes(header_file, header_byte_index);
	payload_file->seek(get_header_payload_pointer(header_file, header_byte_index));
	PackedByteArray compressed_payload_bytes = payload_file->get_buffer(compressed_payload_length);
	
	if (compressed_payload_bytes.size() != compressed_payload_length) {
		return false;
	}
	
	if (bool_bitset.is_dense) {
		constexpr uint32_t uncompressed_payload_length = VoxelConstants::BLOCK_VOXEL_VOLUME * 2;
		
		PackedByteArray uncompressed_voxel_byte_array;
		uncompressed_voxel_byte_array.resize(uncompressed_payload_length);
		const int64_t decompressed_length = Compression::decompress(uncompressed_voxel_byte_array.ptrw(), uncompressed_payload_length, compressed_payload_bytes.ptr(), compressed_payload_length, Compression::MODE_ZSTD);
		
		if (decompressed_length != uncompressed_payload_length) {
			return false;
		}
		
		voxel_data.initialize_dense_voxel_data();
		
		if (voxel_data.dense_voxel_data_ptr == nullptr) {
			return false;
		}
		
		voxel_data.dense_voxel_data_ptr->densities = uncompressed_voxel_byte_array.slice(0, VoxelConstants::BLOCK_VOXEL_VOLUME);
		voxel_data.dense_voxel_data_ptr->materials = uncompressed_voxel_byte_array.slice(VoxelConstants::BLOCK_VOXEL_VOLUME, uncompressed_payload_length);
	} else {
		uint16_t voxel_count = get_header_voxel_count(header_file, header_byte_index);
		uint32_t uncompressed_payload_length = voxel_count * 4;
		
		PackedByteArray uncompressed_voxel_byte_array;
		uncompressed_voxel_byte_array.resize(uncompressed_payload_length);
		const int64_t decompressed_length = Compression::decompress(uncompressed_voxel_byte_array.ptrw(), uncompressed_payload_length, compressed_payload_bytes.ptr(), compressed_payload_length, Compression::MODE_ZSTD);
		
		if (decompressed_length != uncompressed_payload_length) {
			return false;
		}
		
		voxel_data.initialize_sparse_voxel_data();
		
		if (voxel_data.sparse_voxel_data_ptr == nullptr) {
			return false;
		}
		
		PackedByteArray voxel_densities = uncompressed_voxel_byte_array.slice(0, voxel_count);
		PackedByteArray voxel_materials = uncompressed_voxel_byte_array.slice(voxel_count, voxel_count * 2);
		PackedByteArray voxel_index_bytes = uncompressed_voxel_byte_array.slice(voxel_count * 2, voxel_count * 4); // how to turn this into a PackedInt32Array of uint16_t intagers
		
		PackedInt32Array voxel_indices;
		voxel_indices.resize(voxel_count);
		
		for (int32_t i = 0; i < voxel_count; i++) {
			const int32_t byte_index = i * 2;
			const uint16_t voxel_index = static_cast<uint16_t>(voxel_index_bytes[byte_index]) | (static_cast<uint16_t>(voxel_index_bytes[byte_index + 1]) << 8);
			
			voxel_indices.set(i, static_cast<int32_t>(voxel_index));
		}
		
		voxel_data.sparse_voxel_data_ptr->set_bulk_voxels(voxel_densities, voxel_materials, voxel_indices);
	}
	
	return true;
}



int FileManager::create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position) {
	if (header_index_lookup.has(block_position)) {
		return -1;
	}
	
	int header_index;
	int header_byte_index;
	
	if (free_header_indices.is_empty()) {
		header_index = get_total_block_header_count(header_file);
		header_byte_index = convert_header_index_to_byte_index(header_index);
		
		store_header_payload_pointer(header_file, header_byte_index, 0);
		store_header_payload_length(header_file, header_byte_index, 0);
	} else {
		header_index = free_header_indices[free_header_indices.size() - 1];
		free_header_indices.resize(free_header_indices.size() - 1);
		header_byte_index = convert_header_index_to_byte_index(header_index);
	}
	
	BlockHeaderBitset bool_bitset;
	bool_bitset.is_active = true;
	
	store_header_position(header_file, header_byte_index, block_position);
	store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
	store_header_padding_bytes(header_file, header_byte_index, 0);
	store_header_voxel_count(header_file, header_byte_index, 0);
	
	header_index_lookup[block_position] = header_index;
	return header_index;
}

int FileManager::remove_block_header(const Ref<FileAccess> &header_file, Vector3i block_position) {
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	
	if (header_index_ptr == nullptr) {
		return -1;
	}
	
	int header_index = *header_index_ptr;
	int header_byte_index = convert_header_index_to_byte_index(header_index);
	
	if (header_index == get_total_block_header_count(header_file) - 1) {
		header_file->resize(header_byte_index);
		trim_free_tail_headers(header_file);
	} else {
		BlockHeaderBitset bool_bitset;
		bool_bitset.is_active = false;
		store_header_bool_bitset(header_file, header_byte_index, bool_bitset);
		
		free_header_indices.append(header_index);
	}
	
	header_index_lookup.erase(block_position);
	return header_index;
}

void FileManager::trim_free_tail_headers(const Ref<FileAccess> &header_file) {
	int total_header_count = get_total_block_header_count(header_file);
	int free_end_header_bytes = 0;
	
	for (int back_header_index : free_header_indices) {
		int header_index = total_header_count - 1 - back_header_index;
		int header_byte_index = convert_header_index_to_byte_index(header_index);
		
		BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
		
		if (bool_bitset.is_active) {
			break;
		}
		
		free_header_indices.erase(header_index);
		free_end_header_bytes += BLOCK_HEADER_LENGTH;
	}
	
	if (free_end_header_bytes != 0) {
		int active_header_bytes = header_file->get_length() - free_end_header_bytes;
		header_file->resize(active_header_bytes);
	}
}



void FileManager::compact_files() {
	Ref<FileAccess> header_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	Ref<FileAccess> payload_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	
	compact_header_file(header_file);
	compact_payload_file(header_file, payload_file);
	
	header_file->close();
	payload_file->close();
	
	store_header_indices();
}

void FileManager::compact_header_file(const Ref<FileAccess> &header_file) {
	int total_header_count = get_total_block_header_count(header_file);
	int active_header_bytes = HEADER_FILE_HEADER_LENGTH;
	
	PackedByteArray header_byte_buffer;
	header_byte_buffer.resize(BLOCK_HEADER_LENGTH);
	
	for (int i = 0; i < total_header_count; i++) {
		int header_byte_index = convert_header_index_to_byte_index(i);
		BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
		
		if (!bool_bitset.is_active) {
			continue;
		}
		
		int desired_header_byte_index = active_header_bytes;
		
		if (header_byte_index != desired_header_byte_index) {
			header_file->seek(header_byte_index);
			header_byte_buffer = header_file->get_buffer(BLOCK_HEADER_LENGTH);
			
			header_file->seek(desired_header_byte_index);
			header_file->store_buffer(header_byte_buffer);
		}
		
		active_header_bytes += BLOCK_HEADER_LENGTH;
	}
	
	header_file->resize(active_header_bytes);
}

void FileManager::compact_payload_file(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file) {
	int total_header_count = get_total_block_header_count(header_file);
	int active_payload_bytes = PAYLOAD_FILE_HEADER_LENGTH;
	
	PackedByteArray payload_byte_buffer;
	HashMap<int, Vector2i> header_payload_pointer_lookup;
	
	for (int i = 0; i < total_header_count; i++) {
		int header_byte_index = convert_header_index_to_byte_index(i);
		BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
		
		if (!bool_bitset.is_active) {
			continue;
		}
		
		header_payload_pointer_lookup[get_header_payload_pointer(header_file, header_byte_index)] = Vector2i(header_byte_index, get_header_payload_length(header_file, header_byte_index));
	}
	
	Vector<int> sorted_payload_pointers;
	
	for (const KeyValue<int, Vector2i> &key_value_ptr : header_payload_pointer_lookup) {
		sorted_payload_pointers.push_back(key_value_ptr.key);
	}
	
	sorted_payload_pointers.sort();
	
	for (int payload_pointer : sorted_payload_pointers) {
		Vector2i value_pair = header_payload_pointer_lookup.get(payload_pointer);
		int header_byte_index = value_pair.x;
		int payload_length = value_pair.y;
		
		if (payload_pointer <= PAYLOAD_FILE_HEADER_LENGTH ||payload_length <= 0) {
			continue;
		}
		
		int desired_payload_byte_index = active_payload_bytes;
		
		if (payload_pointer != desired_payload_byte_index) {
			payload_file->seek(payload_pointer);
			payload_byte_buffer = payload_file->get_buffer(payload_length);
			
			payload_file->seek(desired_payload_byte_index);
			payload_file->store_buffer(payload_byte_buffer);
			
			store_header_payload_pointer(header_file, header_byte_index, desired_payload_byte_index);
		}
		
		active_payload_bytes += payload_length;
	}
	
	payload_file->resize(active_payload_bytes);
}

void FileManager::store_header_indices() {
	Ref<FileAccess> header_file = FileAccess::open("user://headers.bin", FileAccess::READ_WRITE);
	
	header_index_lookup.clear();
	free_header_indices.clear();
	
	int total_header_count = get_total_block_header_count(header_file);
	
	for (int i = 0; i < total_header_count; i++) {
		int header_byte_index = convert_header_index_to_byte_index(i);
		BlockHeaderBitset bool_bitset = get_header_bool_bitset(header_file, header_byte_index);
		
		if (bool_bitset.is_active) {
			Vector3i block_position = get_header_position(header_file, header_byte_index);
			header_index_lookup[block_position] = i;
		} else {
			free_header_indices.append(i);
		}
	}
	
	header_file->close();
}



bool FileManager::merge_voxel_data(BlockVoxelResource::SparseVoxelData &input_voxel_data, BlockVoxelResource::BlockVoxelData &dst_base_voxel_data) {
	if (!dst_base_voxel_data.is_initialized) {
		return false;
	}
	
	if (dst_base_voxel_data.is_dense) {
		if (dst_base_voxel_data.dense_voxel_data_ptr == nullptr) {
			return false;
		}
		
		for (int i = 0; i < input_voxel_data.indices.size(); i++) {
			dst_base_voxel_data.dense_voxel_data_ptr->set_voxel(input_voxel_data.indices[i], input_voxel_data.densities[i], input_voxel_data.materials[i]);
		}
	} else {
		if (dst_base_voxel_data.sparse_voxel_data_ptr == nullptr) {
			return false;
		}
		
		for (int i = 0; i < input_voxel_data.indices.size(); i++) {
			dst_base_voxel_data.sparse_voxel_data_ptr->set_voxel(input_voxel_data.indices[i], input_voxel_data.densities[i], input_voxel_data.materials[i]);
		}
	}
	
	return true;
}

int FileManager::get_total_block_header_count(const Ref<FileAccess> &header_file) {
	const int block_header_count = (header_file->get_length() - HEADER_FILE_HEADER_LENGTH) / BLOCK_HEADER_LENGTH;
	
	return block_header_count;
}



int FileManager::convert_header_index_to_byte_index(const int header_index) {
	const int header_byte_index = HEADER_FILE_HEADER_LENGTH + header_index * BLOCK_HEADER_LENGTH;
	
	return header_byte_index;
}

void FileManager::move_cursor_to_header_property(const Ref<FileAccess> &header_file, const int header_byte_index, const int header_property) {
	const int property_byte_index = header_byte_index + header_property;
	
	header_file->seek(property_byte_index);
}

Vector3i FileManager::get_header_position(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_POSITION_INDEX);
	
	Vector3i block_position;
	block_position.x = static_cast<int32_t>(header_file->get_32());
    block_position.y = static_cast<int32_t>(header_file->get_32());
    block_position.z = static_cast<int32_t>(header_file->get_32());
	
	return block_position;
}

void FileManager::store_header_position(const Ref<FileAccess> &header_file, const int header_byte_index, const Vector3i block_position) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_POSITION_INDEX);
	
	header_file->store_32(block_position.x);
	header_file->store_32(block_position.y);
	header_file->store_32(block_position.z);
}

uint64_t FileManager::get_header_payload_pointer(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PAYLOAD_POINTER_INDEX);
	
	return static_cast<uint64_t>(header_file->get_64());
}

void FileManager::store_header_payload_pointer(const Ref<FileAccess> &header_file, const int header_byte_index, const uint64_t payload_pointer) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PAYLOAD_POINTER_INDEX);
	
	header_file->store_64(payload_pointer);
}

uint32_t FileManager::get_header_payload_length(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PAYLOAD_LENGTH_INDEX);
	
	return static_cast<uint32_t>(header_file->get_32());
}

void FileManager::store_header_payload_length(const Ref<FileAccess> &header_file, const int header_byte_index, const uint32_t payload_length) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PAYLOAD_LENGTH_INDEX);
	
	header_file->store_32(payload_length);
}

FileManager::BlockHeaderBitset FileManager::get_header_bool_bitset(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_BOOLEAN_BITSET_INDEX);
	
	const uint8_t bits = header_file->get_8();
	
	BlockHeaderBitset bool_bitset;
	bool_bitset.is_active = (bits & (1u << 0)) != 0;
	bool_bitset.is_dense = (bits & (1u << 1)) != 0;
	bool_bitset.is_write_in_progress = (bits & (1u << 2)) != 0;
	bool_bitset.reserved_3 = (bits & (1u << 3)) != 0;
	bool_bitset.reserved_4 = (bits & (1u << 4)) != 0;
	bool_bitset.reserved_5 = (bits & (1u << 5)) != 0;
	bool_bitset.reserved_6 = (bits & (1u << 6)) != 0;
	bool_bitset.reserved_7 = (bits & (1u << 7)) != 0;
	
    return bool_bitset;
}

void FileManager::store_header_bool_bitset(const Ref<FileAccess> &header_file, const int header_byte_index, const FileManager::BlockHeaderBitset &bool_bitset) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_BOOLEAN_BITSET_INDEX);

	uint8_t bits = 0;
	
	bits |= static_cast<uint8_t>(bool_bitset.is_active) << 0;
	bits |= static_cast<uint8_t>(bool_bitset.is_dense) << 1;
	bits |= static_cast<uint8_t>(bool_bitset.is_write_in_progress) << 2;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_3) << 3;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_4) << 4;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_5) << 5;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_6) << 6;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_7) << 7;
	
	header_file->store_8(bits);
}

uint8_t FileManager::get_header_padding_bytes(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PADDING_BYTES_INDEX);
	
	return static_cast<uint8_t>(header_file->get_8());
}

void FileManager::store_header_padding_bytes(const Ref<FileAccess> &header_file, const int header_byte_index, const uint8_t padding_bytes) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_PADDING_BYTES_INDEX);
	
	header_file->store_8(padding_bytes);
}

uint16_t FileManager::get_header_voxel_count(const Ref<FileAccess> &header_file, const int header_byte_index) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_VOXEL_COUNT_INDEX);
	
	return static_cast<uint16_t>(header_file->get_16());
}

void FileManager::store_header_voxel_count(const Ref<FileAccess> &header_file, const int header_byte_index, const uint16_t voxel_count) {
	move_cursor_to_header_property(header_file, header_byte_index, BLOCK_HEADER_VOXEL_COUNT_INDEX);
	
	header_file->store_16(voxel_count);
}



void FileManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("save_voxels", "block_position", "input_voxel_data"), &FileManager::save_voxels);
	ClassDB::bind_method(D_METHOD("load_voxels", "block_position", "output_voxel_data"), &FileManager::load_voxels);
	ClassDB::bind_method(D_METHOD("compact_files"), &FileManager::compact_files);
	ClassDB::bind_method(D_METHOD("store_header_indices"), &FileManager::store_header_indices);
	//ClassDB::bind_method(D_METHOD("get_block_header_file_directory"), &FileManager::get_block_header_file_directory);
	//ClassDB::bind_method(D_METHOD("get_block_payload_file_directory"), &FileManager::get_block_payload_file_directory);
	//ClassDB::bind_method(D_METHOD("initialize_file_storage", "header_file_directory", "payload_file_directory"), &FileManager::initialize_file_storage);
}
