#include "file_manager.h"
#include "core/math/vector3i.h"
#include "core/io/compression.h"
#include "core/variant/variant.h"
#include <cstdint>


bool FileManager::save_voxels(BlockVoxelData voxel_data, Vector3i block_position) {

}

FileManager::BlockVoxelData FileManager::load_voxels(Vector3i block_position) {

}


bool FileManager::write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, BlockVoxelData &voxel_data) {
	if (!voxel_data.is_initialized) {
		return false;
	}
	
	PackedByteArray voxel_byte_array;
	
	if (voxel_data.is_dense) {
		move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_FORMAT_BOOL_INDEX);
		header_file->store_8(1);
		voxel_byte_array = voxel_data.dense_voxel_data_ptr->densities;
		voxel_byte_array.append_array(voxel_data.dense_voxel_data_ptr->materials);
	} else {
		move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_FORMAT_BOOL_INDEX);
		header_file->store_8(0);
		voxel_byte_array = voxel_data.sparse_voxel_data_ptr->densities;
		voxel_byte_array.append_array(voxel_data.sparse_voxel_data_ptr->materials);
		
		const PackedInt32Array &indices = voxel_data.sparse_voxel_data_ptr->indices;
		const int64_t voxel_index_count = indices.size();
		PackedByteArray voxel_index_bytes;
		voxel_index_bytes.resize(voxel_index_count * 2);
		uint8_t *voxel_index_bytes_ptr = voxel_index_bytes.ptrw();
		
		for (int64_t i = 0; i < voxel_index_count; i++) {
			if (indices[i] < 0 || indices[i] > UINT16_MAX) {
				return false;
			}
			const uint16_t value = static_cast<uint16_t>(indices[i]);
			voxel_index_bytes_ptr[i * 2] = static_cast<uint8_t>(value & 0xFF);
			voxel_index_bytes_ptr[i * 2 + 1] = static_cast<uint8_t>(value >> 8);
		}
		
		voxel_byte_array.append_array(voxel_index_bytes);
	}
	
	PackedByteArray compressed_voxel_byte_array;
	compressed_voxel_byte_array.resize(Compression::get_max_compressed_buffer_size(voxel_byte_array.size(), Compression::MODE_ZSTD));
	int64_t compressed_size = Compression::compress(compressed_voxel_byte_array.ptrw(), voxel_byte_array.ptr(), voxel_byte_array.size(), Compression::MODE_ZSTD);
	
	if (compressed_size < 0) {
		return false;
	}
	
	compressed_voxel_byte_array.resize(compressed_size);
	
	const int64_t padded_byte_count = (8 - (compressed_size % 8)) % 8;
	const int64_t padded_compressed_size = compressed_size + padded_byte_count;
	compressed_voxel_byte_array.resize(padded_compressed_size);
	
	uint8_t *compressed_data = compressed_voxel_byte_array.ptrw();
	for (int64_t i = compressed_size; i < padded_compressed_size; i++) {
		compressed_data[i] = 0;
	}
	
	move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_PAYLOAD_POINTER_INDEX);
	int64_t old_payload_pointer = header_file->get_64();
	int64_t old_payload_length = header_file->get_32();
	
	move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_PADDING_BYTES);
	header_file->store_16(padded_byte_count);
	
	if (padded_compressed_size > old_payload_length || old_payload_pointer < PAYLOAD_FILE_HEADER_LENGTH) {
		move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_PAYLOAD_POINTER_INDEX);
		header_file->store_64(payload_file->get_length());
		header_file->store_32(padded_compressed_size);
		payload_file->seek_end();
	} else {
		move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_PAYLOAD_LENGTH_INDEX);
		header_file->store_32(padded_compressed_size);
		payload_file->seek(old_payload_pointer);
	}
	
	payload_file->store_buffer(compressed_voxel_byte_array);
	return true;
}

FileManager::BlockVoxelData FileManager::read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index) {

}


int FileManager::create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position) {
	if (header_index_lookup.has(block_position)) {
		return -1;
	}
	
	int header_index;
	
	if (free_header_indices.is_empty()) {
		header_index = get_total_block_header_count(header_file);
	} else {
		header_index = free_header_indices[free_header_indices.size() - 1];
		free_header_indices.resize(free_header_indices.size() - 1);
	}
	
	store_block_position_vector3i(header_file, header_index, block_position);
	header_file->store_64(0);
	header_file->store_32(0);
	header_file->store_8(1);
	header_file->store_8(0);
	header_file->store_16(0);
	
	header_index_lookup[block_position] = header_index;
	return header_index;
}

int FileManager::remove_block_header(const Ref<FileAccess> &header_file, Vector3i block_position) {
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	
	if (header_index_ptr == nullptr) {
		return -1;
	}
	
	int header_index = *header_index_ptr;
	
	if (header_index == get_total_block_header_count(header_file) - 1) {
		header_file->resize(convert_header_index_to_byte_index(header_index));
		trim_free_tail_headers(header_file);
	} else {
		move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_ACTIVE_BOOL_INDEX);
		header_file->store_8(0);
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
		if (is_header_active(header_file, header_index)) {
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

void FileManager::move_cursor_to_header_property(const Ref<FileAccess> &header_file, int header_index, int header_property) {
	if (header_file.is_null()) {
		return;
	}
	
	const int byte_position = convert_header_index_to_byte_index(header_index) + header_property;
	
	header_file->seek(byte_position);
}

int FileManager::convert_header_index_to_byte_index(int header_index) {
	const int header_byte_index = HEADER_FILE_HEADER_LENGTH + header_index * BLOCK_HEADER_LENGTH;
	
	return header_byte_index;
}

int FileManager::get_total_block_header_count(const Ref<FileAccess> &header_file) {
	const int block_header_count = (header_file->get_length() - HEADER_FILE_HEADER_LENGTH) / HEADER_FILE_HEADER_LENGTH;
	
	return block_header_count;
}

void FileManager::store_block_position_vector3i(const Ref<FileAccess> &header_file, int header_index, Vector3i block_position) {
	move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_POSITION_INDEX);
	header_file->store_32(block_position.x);
	header_file->store_32(block_position.y);
	header_file->store_32(block_position.z);
}

Vector3i FileManager::read_block_position_vector3i(const Ref<FileAccess> &header_file, int header_index) {
	Vector3i block_position;
	
	move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_POSITION_INDEX);
	block_position.x = header_file->get_32();
	block_position.y = header_file->get_32();
	block_position.z = header_file->get_32();
	
	return block_position;
}

bool FileManager::is_header_active(const Ref<FileAccess> &header_file, int header_index) {
	move_cursor_to_header_property(header_file, header_index, BLOCK_HEADER_ACTIVE_BOOL_INDEX);
	
	if (header_file->get_8() == 1) {
		return true;
	} else {
		return false;
	}
}
