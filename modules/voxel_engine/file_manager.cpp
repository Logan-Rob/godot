#include "file_manager.h"
#include "core/math/vector3i.h"
#include "core/variant/variant.h"
#include "core/io/dir_access.h"
#include "modules/voxel_engine/block_voxel_data.h"
#include "modules/voxel_engine/voxel_constants.h"

#include "core/object/class_db.h"



bool FileManager::open_files(String selected_block_header_file_directory, String selected_payload_file_directory, String selected_temp_block_header_file_directory, String selected_temp_block_payload_file_directory) {
	String target_header_directory = selected_block_header_file_directory.get_base_dir();
	if (target_header_directory != "user://" && target_header_directory != "res://") {
		Error header_directory_error = DirAccess::make_dir_recursive_absolute(target_header_directory);
		if (header_directory_error != OK && header_directory_error != ERR_ALREADY_EXISTS) {
			ERR_PRINT("Failed to make directory: " + target_header_directory);
			return false;
		}
	}
	
	String target_payload_directory = selected_payload_file_directory.get_base_dir();
	if (target_payload_directory != "user://" && target_payload_directory != "res://") {
		Error payload_directory_error = DirAccess::make_dir_recursive_absolute(target_payload_directory);
		if (payload_directory_error != OK && payload_directory_error != ERR_ALREADY_EXISTS) {
			ERR_PRINT("Failed to make directory: " + target_payload_directory);
			return false;
		}
	}
	
	if (FileAccess::exists(selected_temp_block_header_file_directory)) {
		Error error = DirAccess::remove_absolute(selected_temp_block_header_file_directory);
		if (error != OK) {
			ERR_PRINT("Failed to remove temporary header file: " + selected_temp_block_header_file_directory);
			return false;
		}
		
		is_temp_header_file_initialized = false;
	}
	
	if (FileAccess::exists(selected_temp_block_payload_file_directory)) {
		Error error = DirAccess::remove_absolute(selected_temp_block_payload_file_directory);
		if (error != OK) {
			ERR_PRINT("Failed to remove temporary payload file: " + selected_temp_block_payload_file_directory);
			return false;
		}
		
		is_temp_payload_file_initialized = false;
	}
	
	bool new_block_header_file = !FileAccess::exists(selected_block_header_file_directory);
	bool new_payload_file = !FileAccess::exists(selected_payload_file_directory);
	
	Ref<FileAccess> header_file = FileAccess::open(selected_block_header_file_directory, new_block_header_file ? FileAccess::WRITE_READ : FileAccess::READ_WRITE);
	Ref<FileAccess> payload_file = FileAccess::open(selected_payload_file_directory, new_payload_file ? FileAccess::WRITE_READ : FileAccess::READ_WRITE);
	
	if (!header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		ERR_PRINT("Files are invalid or not open after request");
		return false;
	}
	
	if (new_block_header_file) { // TEMPORARY make file header
		PackedByteArray header_file_header;
		header_file_header.resize(HEADER_FILE_HEADER_LENGTH);
		header_file_header.fill(0);
		
		header_file->seek(0);
		if (header_file->get_error() != OK) {
			ERR_PRINT("Header file header seek error");
			return false;
		}
		
		header_file->store_buffer(header_file_header);
		if (header_file->get_error() != OK) {
			ERR_PRINT("Header file header store error");
			return false;
		}
		
		header_file->flush();
		if (header_file->get_error() != OK) {
			ERR_PRINT("Header file header flush error");
			return false;
		}
	} else { // TEMPORARY read file header
		int placeholder = 1;
	}
	
	if (new_payload_file) { // TEMPORARY make file header
		PackedByteArray payload_file_header;
		payload_file_header.resize(PAYLOAD_FILE_HEADER_LENGTH);
		payload_file_header.fill(0);
		
		payload_file->seek(0);
		if (payload_file->get_error() != OK) {
			ERR_PRINT("Payload file header seek error");
			return false;
		}
		
		payload_file->store_buffer(payload_file_header);
		if (payload_file->get_error() != OK) {
			ERR_PRINT("Payload file header store error");
			return false;
		}
		
		payload_file->flush();
		if (header_file->get_error() != OK) {
			ERR_PRINT("Payload file header flush error");
			return false;
		}
	} else { // TEMPORARY read file header
		int placeholder = 1;
	}
	
	if (!populate_header_indices(header_file)) {
		ERR_PRINT("Populate_header_indices_error");
		return false;
	}
	
	block_header_file_directory = selected_block_header_file_directory;
	block_payload_file_directory = selected_payload_file_directory;
	temp_block_header_file_directory = selected_temp_block_header_file_directory;
	temp_block_payload_file_directory = selected_temp_block_payload_file_directory;
	is_header_file_initialized = true;
	is_payload_file_initialized = true;
	
	header_file->close();
	payload_file->close();
	
	return true;
}

bool FileManager::populate_header_indices(const Ref<FileAccess> &header_file) {
	int header_count = get_total_block_header_count(header_file);
	if (header_file->get_error() != OK) {
		ERR_PRINT("Unable to get total header count");
		return false;
	}
	
	header_index_lookup.clear();
	free_header_indices.clear();
	
	if (header_count <= 0) {
		return true;
	}
	
	for (int header_index = 0; header_index < header_count; header_index++) {
		int header_byte_index = convert_header_index_to_byte_index(header_index);
		
		PackedByteArray header_buffer = get_block_header_buffer(header_file, header_byte_index);
		if (header_file->get_error() != OK) {
			ERR_PRINT("Unable to get total header buffer");
			return false;
		}
		
		FileManager::BlockHeaderBitset bitset = get_header_bool_bitset(header_buffer);
		
		if (bitset.is_active) {
			header_index_lookup[get_header_position(header_buffer)] = header_index;
		} else {
			free_header_indices.append(header_index);
		}
	}
	
	return true;
}



bool FileManager::save_voxels(Vector3i block_position, const Ref<BlockVoxelData> &voxel_data) {
	if (!is_header_file_initialized || !is_payload_file_initialized) {
		ERR_PRINT("Payload or header file not initialized");
		return false;
	}
	
	BlockVoxelData::ResourceState resource_state = voxel_data->get_resource_state();
	
	if (resource_state == BlockVoxelData::ResourceState::Empty) {
		return true;
	}
	
	if (resource_state == BlockVoxelData::ResourceState::Uninitialized) {
		ERR_PRINT("Attempted to sace uninitialized BlockVoxelData");
		return false;
	}
	
	Ref<FileAccess> header_file = FileAccess::open(block_header_file_directory, FileAccess::READ_WRITE);
	Ref<FileAccess> payload_file = FileAccess::open(block_payload_file_directory, FileAccess::READ_WRITE);
	
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	int header_index;
	bool is_dense;
	
	if (header_index_ptr == nullptr) {
		if (resource_state == BlockVoxelData::ResourceState::Sparse) {
			if (voxel_data->get_voxel_count() > SPARSE_DENSE_VOXEL_THRESHOLD) {
				if (!voxel_data->convert_sparse_to_dense(true)) {
					header_file->close();
					payload_file->close();
					ERR_PRINT("Failed sparse -> dense conversion for BlockVoxelData");
					return false;
				}
				
				is_dense = true;
			} else {
				is_dense = false;
			}
		} else {
			if (voxel_data->get_edited_voxel_count() <= SPARSE_DENSE_VOXEL_THRESHOLD) {
				if (!voxel_data->convert_dense_to_sparse()) {
					header_file->close();
					payload_file->close();
					ERR_PRINT("Failed sparse -> dense conversion for BlockVoxelData");
					return false;
				}
				
				is_dense = false;
			} else {
				is_dense = true;
			}
		}
		
		header_index = create_block_header(header_file, block_position, is_dense);
		
		if (!write_voxels_to_disk(header_file, payload_file, header_index, voxel_data)) {
			header_file->close();
			payload_file->close();
			ERR_PRINT("Failed to write voxels to disk");
			return false;
		}
	} else {
		header_index = *header_index_ptr;
		Ref<BlockVoxelData> base_voxel_data = memnew(BlockVoxelData);
		if (!read_voxels_from_disk(header_file, payload_file, header_index, base_voxel_data)) {
			header_file->close();
			payload_file->close();
			ERR_PRINT("Failed to read voxels from disk");
			return false;
		}
		
		if (resource_state == BlockVoxelData::ResourceState::Dense) {
			if (!write_voxels_to_disk(header_file, payload_file, header_index, voxel_data)) {
				header_file->close();
				payload_file->close();
				ERR_PRINT("Failed to write voxels to disk");
				return false;
			}
			
			header_file->close();
			payload_file->close();
			return true;
		}
		
		BlockVoxelData::ResourceState base_resource_state = base_voxel_data->get_resource_state();
		
		if (base_resource_state == BlockVoxelData::ResourceState::Uninitialized) {
			header_file->close();
			payload_file->close();
			ERR_PRINT("Base voxel data is uninitialized");
			return false;
		}
		
		if (!base_voxel_data->merge_sparse_block_data_into_self(voxel_data)) {
			header_file->close();
			payload_file->close();
			ERR_PRINT("Failed to merge sparse block data into self");
			return false;
		}
		
		if (base_resource_state == BlockVoxelData::ResourceState::Sparse) {
			if (base_voxel_data->get_voxel_count() > SPARSE_DENSE_VOXEL_THRESHOLD) {
				if (!base_voxel_data->convert_sparse_to_dense(true)) {
					header_file->close();
					payload_file->close();
					ERR_PRINT("Failed sparse -> dense conversion for BlockVoxelData");
					return false;
				}
				
				is_dense = true;
			} else {
				is_dense = false;
			}
		} else {
			if (base_voxel_data->get_edited_voxel_count() <= SPARSE_DENSE_VOXEL_THRESHOLD) {
				if (!base_voxel_data->convert_dense_to_sparse()) {
					header_file->close();
					payload_file->close();
					ERR_PRINT("Failed dense -> sparse conversion for BlockVoxelData");
					return false;
				}
				
				is_dense = false;
			} else {
				is_dense = true;
			}
		}
		
		if (!write_voxels_to_disk(header_file, payload_file, header_index, base_voxel_data)) {
			header_file->close();
			payload_file->close();
			ERR_PRINT("Failed to write voxels to disk");
			return false;
		}
	}
	
	header_file->close();
	payload_file->close();
	
	return true;
}

Ref<BlockVoxelData> FileManager::load_voxels(Vector3i block_position) {
	Ref<BlockVoxelData> voxel_data = memnew(BlockVoxelData);
	
	if (!is_header_file_initialized || !is_payload_file_initialized) {
		ERR_PRINT("Payload or header file not initialized");
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		return voxel_data;
	}
	
	int *header_index_ptr = header_index_lookup.getptr(block_position);
	
	if (header_index_ptr == nullptr) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Empty);
		return voxel_data;
	}
	
	Ref<FileAccess> header_file = FileAccess::open(block_header_file_directory, FileAccess::READ);
	Ref<FileAccess> payload_file = FileAccess::open(block_payload_file_directory, FileAccess::READ);
	
	if (!header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		ERR_PRINT("Files are invalid or not open after request");
		header_file->close();
		payload_file->close();
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		return voxel_data;
	}
	
	const int header_index = *header_index_ptr;
	if (!read_voxels_from_disk(header_file, payload_file, header_index, voxel_data)) {
		ERR_PRINT("Failed to read voxels from disk");
		header_file->close();
		payload_file->close();
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		return voxel_data;
	}
	
	header_file->close();
	payload_file->close();
	
	return voxel_data;
}



bool FileManager::write_voxels_to_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, const Ref<BlockVoxelData> &voxel_data) {
	if (!is_header_file_initialized || !is_payload_file_initialized || !header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		ERR_PRINT("Files are not initialized, invalid, or not open");
		return false;
	}
	
	BlockVoxelData::ResourceState block_state = voxel_data->get_resource_state();
	int voxel_count = voxel_data->get_voxel_count();
	PackedByteArray uncompressed_new_voxel_data;
	PackedByteArray new_block_payload;
	
	if (block_state == BlockVoxelData::ResourceState::Uninitialized || block_state == BlockVoxelData::ResourceState::Empty) {
		ERR_PRINT("Tried to write uninitialized or empty BlockVoxelData to disk");
		return false;
	}
	
	int header_byte_index = convert_header_index_to_byte_index(header_index);
	PackedByteArray header_buffer = get_block_header_buffer(header_file, header_byte_index);
	if (header_file->get_error() != OK) {
		ERR_PRINT("Unable to get total header buffer");
		return false;
	}
	if (header_buffer.size() != BLOCK_HEADER_LENGTH) {
		ERR_PRINT("Invalid header buffer size");
		return false;
	}
	FileManager::BlockHeaderBitset bitset = get_header_bool_bitset(header_buffer);
	
	if (block_state == BlockVoxelData::ResourceState::Sparse) {
		bitset.is_dense = false;
		store_header_bool_bitset(header_buffer, bitset);
		
		uncompressed_new_voxel_data.append_array(voxel_data->voxel_densities);
		uncompressed_new_voxel_data.append_array(voxel_data->voxel_materials);
		uncompressed_new_voxel_data.append_array(convert_int16_array_to_packed_byte_array(voxel_data->voxel_indices));
		
		if (uncompressed_new_voxel_data.size() != voxel_count * SPARSE_VOXEL_SIZE) {
			ERR_PRINT("Uncompressed voxel size is not the sparse expected size");
			return false;
		}
	} else {
		bitset.is_dense = true;
		store_header_bool_bitset(header_buffer, bitset);
		
		uncompressed_new_voxel_data.append_array(voxel_data->voxel_densities);
		uncompressed_new_voxel_data.append_array(voxel_data->voxel_materials);
		
		if (uncompressed_new_voxel_data.size() != VoxelConstants::BLOCK_VOXEL_VOLUME * DENSE_VOXEL_SIZE) {
			ERR_PRINT("Uncompressed voxel size is not the dense expected size");
			return false;
		}
	}
	
	new_block_payload.resize(Compression::get_max_compressed_buffer_size(uncompressed_new_voxel_data.size(), Compression::MODE_ZSTD));
	int64_t compressed_size = Compression::compress(new_block_payload.ptrw(), uncompressed_new_voxel_data.ptr(), uncompressed_new_voxel_data.size(), Compression::MODE_ZSTD);
	
	if (compressed_size < 0) {
		ERR_PRINT(vformat("Compression error, returned size code: %d", compressed_size));
		return false;
	}
	
	const int64_t padded_byte_count = (8 - (compressed_size % 8)) % 8;
	const int64_t padded_compressed_size = compressed_size + padded_byte_count;
	new_block_payload.resize(padded_compressed_size);
	int new_payload_pointer = payload_file->get_length();
	
	payload_file->seek_end();
	if (payload_file->get_error() != OK) {
		ERR_PRINT("Payload file seek error");
		return false;
	}
	payload_file->store_buffer(new_block_payload);
	if (payload_file->get_error() != OK) {
		ERR_PRINT("Payload file store error");
		return false;
	}
	payload_file->flush();
	if (payload_file->get_error() != OK) {
		ERR_PRINT("Payload file flush error");
		return false;
	}
	
	store_header_payload_pointer(header_buffer, new_payload_pointer);
	store_header_payload_length(header_buffer, padded_compressed_size);
	store_header_padding_bytes(header_buffer, padded_byte_count);
	store_header_voxel_count(header_buffer, voxel_count);
	
	header_file->seek(header_byte_index);
	if (header_file->get_error() != OK) {
		ERR_PRINT("Header file seek error");
		return false;
	}
	
	header_file->store_buffer(header_buffer);
	if (header_file->get_error() != OK) {
		ERR_PRINT("Header file store error");
		return false;
	}
	header_file->flush();
	if (header_file->get_error() != OK) {
		ERR_PRINT("Header file flush error");
		return false;
	}
	
	return true;
}

bool FileManager::read_voxels_from_disk(const Ref<FileAccess> &header_file, const Ref<FileAccess> &payload_file, int header_index, const Ref<BlockVoxelData> &voxel_data) {
	if (!is_header_file_initialized || !is_payload_file_initialized || !header_file.is_valid() || !payload_file.is_valid() || !header_file->is_open() || !payload_file->is_open()) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		ERR_PRINT("Files are not initialized, invalid, or not open");
		return false;
	}
	
	int header_byte_index = convert_header_index_to_byte_index(header_index);
	PackedByteArray header_buffer = get_block_header_buffer(header_file, header_byte_index);
	if (header_file->get_error() != OK) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		ERR_PRINT("Unable to get total header buffer");
		return false;
	}
	if (header_buffer.size() != BLOCK_HEADER_LENGTH) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		ERR_PRINT("Invalid header buffer size");
		return false;
	}
	int payload_pointer = get_header_payload_pointer(header_buffer);
	int payload_length = get_header_payload_length(header_buffer) - get_header_padding_bytes(header_buffer);
	FileManager::BlockHeaderBitset bitset = get_header_bool_bitset(header_buffer);
	
	if (!bitset.is_active || payload_pointer < HEADER_FILE_HEADER_LENGTH || payload_length <= 0) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Empty);
		return true;
	}
	
	payload_file->seek(payload_pointer);
	if (payload_file->get_error() != OK) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		ERR_PRINT("Payload file seek error");
		return false;
	}
	
	PackedByteArray compressed_payload = payload_file->get_buffer(payload_length);
	if (payload_file->get_error() != OK) {
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
		ERR_PRINT("Payload file get buffer error");
		return false; 
	}
	
	int voxel_count = get_header_voxel_count(header_buffer);
	
	if (bitset.is_dense) {
		int decompressed_payload_length = voxel_count * DENSE_VOXEL_SIZE;
		
		PackedByteArray decompressed_payload;
		decompressed_payload.resize(decompressed_payload_length);
		const int64_t decompressed_length = Compression::decompress(decompressed_payload.ptrw(), decompressed_payload_length, compressed_payload.ptr(), compressed_payload.size(), Compression::MODE_ZSTD);
		
		if (decompressed_length != decompressed_payload_length) {
			ERR_PRINT("Uncompressed voxel size is not the dense expected size");
			return false;
		}
		
		
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Dense);
		PackedByteArray densities = decompressed_payload.slice(0, VoxelConstants::BLOCK_VOXEL_VOLUME);
		PackedByteArray materials = decompressed_payload.slice(VoxelConstants::BLOCK_VOXEL_VOLUME, decompressed_length);
		PackedByteArray edited_voxels;
		edited_voxels.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
		edited_voxels.fill(0);
		
		if (!voxel_data->set_bulk_voxels_dense(densities, materials, edited_voxels)) {
			voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
			ERR_PRINT("Failed to set bulk voxels dense");
			return false; 
		}
	} else {
		int decompressed_payload_length = voxel_count * SPARSE_VOXEL_SIZE;
		
		PackedByteArray decompressed_payload;
		decompressed_payload.resize(decompressed_payload_length);
		const int64_t decompressed_length = Compression::decompress(decompressed_payload.ptrw(), decompressed_payload_length, compressed_payload.ptr(), compressed_payload.size(), Compression::MODE_ZSTD);
		
		if (decompressed_length != decompressed_payload_length) {
			ERR_PRINT("Uncompressed voxel size is not the sparse expected size");
			return false;
		}
		
		voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Sparse);
		PackedByteArray densities = decompressed_payload.slice(0, voxel_count);
		PackedByteArray materials = decompressed_payload.slice(voxel_count, voxel_count * 2);
		PackedInt32Array indices = convert_packed_byte_array_to_int16_array(decompressed_payload.slice(voxel_count * 2, voxel_count * 4));
		
		if (!voxel_data->set_bulk_voxels_sparse(densities, materials, indices)) {
			voxel_data->initialize_voxel_data(BlockVoxelData::ResourceState::Uninitialized);
			ERR_PRINT("Failed to set bulk voxels sparse");
			return false; 
		}
	}
	
	return true;
}



int FileManager::create_block_header(const Ref<FileAccess> &header_file, Vector3i block_position, bool is_dense) {
	if (header_index_lookup.has(block_position)) {
		return -1;
	}
	
	PackedByteArray header_buffer;
	header_buffer.resize(BLOCK_HEADER_LENGTH);
	header_buffer.fill(0);
	
	BlockHeaderBitset bitset;
	bitset.is_active = true;
	bitset.is_dense = is_dense;
	
	store_header_position(header_buffer, block_position);
	store_header_payload_pointer(header_buffer, 0);
	store_header_payload_length(header_buffer, 0);
	store_header_bool_bitset(header_buffer, bitset);
	store_header_padding_bytes(header_buffer, 0);
	store_header_voxel_count(header_buffer, 0);
	
	int header_index;
	int header_byte_index;
	
	if (free_header_indices.is_empty()) {
		header_index = get_total_block_header_count(header_file);
		header_byte_index = convert_header_index_to_byte_index(header_index);
	} else {
		header_index = free_header_indices[free_header_indices.size() - 1];
		free_header_indices.resize(free_header_indices.size() - 1);
		header_byte_index = convert_header_index_to_byte_index(header_index);
	}
	
	header_file->seek(header_byte_index);
	if (header_file->get_error() != OK) {
		return -2;
	}
	header_file->store_buffer(header_buffer);
	if (header_file->get_error() != OK) {
		return -2;
	}
	header_file->flush();
	if (header_file->get_error() != OK) {
		return -2;
	}
	
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
		if (header_file->get_error() != OK) {
			return -2;
		}
		header_file->flush();
		if (header_file->get_error() != OK) {
			return -2;
		}
		
		trim_free_tail_headers(header_file);
	} else {
		PackedByteArray header_buffer;
		header_buffer.resize(BLOCK_HEADER_LENGTH);
		header_buffer.fill(0);
		
		BlockHeaderBitset bitset;
		bitset.is_active = false;
		
		store_header_position(header_buffer, block_position);
		store_header_payload_pointer(header_buffer, 0);
		store_header_payload_length(header_buffer, 0);
		store_header_bool_bitset(header_buffer, bitset);
		store_header_padding_bytes(header_buffer, 0);
		store_header_voxel_count(header_buffer, 0);
		
		header_file->seek(header_byte_index);
		if (header_file->get_error() != OK) {
			return -2;
		}
		header_file->store_buffer(header_buffer);
		if (header_file->get_error() != OK) {
			return -2;
		}
		header_file->flush();
		if (header_file->get_error() != OK) {
			return -2;
		}
		
		free_header_indices.append(header_index);
	}
	
	header_index_lookup.erase(block_position);
	return header_index;
}

bool FileManager::trim_free_tail_headers(const Ref<FileAccess> &header_file) {
	int total_headers = get_total_block_header_count(header_file);
	int trailing_inactive_count = 0;
	
	for (int i = total_headers - 1; i >= 0; i--) {
		int byte_index = convert_header_index_to_byte_index(i);
		PackedByteArray buffer = get_block_header_buffer(header_file, byte_index);
		BlockHeaderBitset bitset = get_header_bool_bitset(buffer);
		
		if (bitset.is_active) {
			break;
		}
		
		trailing_inactive_count++;
		free_header_indices.erase(i);
	}
	
	if (trailing_inactive_count > 0) {
		int new_total = total_headers - trailing_inactive_count;
		int new_file_size = convert_header_index_to_byte_index(new_total);
		
		header_file->resize(new_file_size);
		if (header_file->get_error() != OK) {
			return false;
		}
		header_file->flush();
		if (header_file->get_error() != OK) {
			return false;
		}
	}
	
	return true;
}



bool FileManager::compact_files() {
	return true;
}



PackedByteArray FileManager::convert_int16_array_to_packed_byte_array(const PackedInt32Array &int16_array) {
	int int16_index_count = int16_array.size();
	
	PackedByteArray byte_array;
	byte_array.resize(int16_index_count * 2);
	uint8_t *byte_array_ptr = byte_array.ptrw();
	const int32_t *int16_array_ptr = int16_array.ptr();
	
	for (int i = 0; i < int16_index_count; i++) {
		const uint16_t int16_value = static_cast<uint16_t>(int16_array_ptr[i]);
		byte_array_ptr[i * 2] = static_cast<uint8_t>(int16_value & 0xFF);
		byte_array_ptr[i * 2 + 1] = static_cast<uint8_t>(int16_value >> 8);
	}
	
	return byte_array;
}

PackedInt32Array FileManager::convert_packed_byte_array_to_int16_array(const PackedByteArray &byte_array) {
	int byte_count = byte_array.size();
	int int16_index_count = byte_count / 2;
	
	PackedInt32Array int16_array;
	int16_array.resize(int16_index_count);
	int32_t *int16_array_ptr = int16_array.ptrw();
	const uint8_t *byte_array_ptr = byte_array.ptr();
	
	for (int i = 0; i < int16_index_count; i++) {
		uint16_t low = byte_array_ptr[i * 2];
		uint16_t high = byte_array_ptr[i * 2 + 1];
		uint16_t int16_value = low | (high << 8);
		
		int16_array_ptr[i] = static_cast<int32_t>(int16_value);
	}
	
	return int16_array;
}

int FileManager::get_total_block_header_count(const Ref<FileAccess> &header_file) {
	const int block_header_count = (header_file->get_length() - HEADER_FILE_HEADER_LENGTH) / BLOCK_HEADER_LENGTH;
	
	return block_header_count;
}

int FileManager::convert_header_index_to_byte_index(const int header_index) {
	const int header_byte_index = HEADER_FILE_HEADER_LENGTH + header_index * BLOCK_HEADER_LENGTH;
	
	return header_byte_index;
}

PackedByteArray FileManager::get_block_header_buffer(const Ref<FileAccess> &header_file, const int header_byte_index) {
	PackedByteArray header_buffer;
	header_buffer.resize(BLOCK_HEADER_LENGTH);
	
	header_file->seek(header_byte_index);
	header_file->get_buffer(header_buffer.ptrw(), BLOCK_HEADER_LENGTH);
	
	return header_buffer;
}



Vector3i FileManager::get_header_position(const PackedByteArray &header_buffer) const {
	const uint8_t *ptr = header_buffer.ptr() + BLOCK_HEADER_POSITION_INDEX;
	
	Vector3i block_position;
	memcpy(&block_position.x, ptr + 0, sizeof(int32_t));
	memcpy(&block_position.y, ptr + 4, sizeof(int32_t));
	memcpy(&block_position.z, ptr + 8, sizeof(int32_t));
	
	return block_position;
}

void FileManager::store_header_position(PackedByteArray &header_buffer, const Vector3i &block_position) {
	uint8_t *ptr = header_buffer.ptrw() + BLOCK_HEADER_POSITION_INDEX;
	
	memcpy(ptr + 0, &block_position.x, sizeof(int32_t));
	memcpy(ptr + 4, &block_position.y, sizeof(int32_t));
	memcpy(ptr + 8, &block_position.z, sizeof(int32_t));
}

uint64_t FileManager::get_header_payload_pointer(const PackedByteArray &header_buffer) const {
	const uint8_t *ptr = header_buffer.ptr() + BLOCK_HEADER_PAYLOAD_POINTER_INDEX;
	
	uint64_t value;
	memcpy(&value, ptr, sizeof(uint64_t));
	return value;
}

void FileManager::store_header_payload_pointer(PackedByteArray &header_buffer, uint64_t payload_pointer) {
	uint8_t *ptr = header_buffer.ptrw() + BLOCK_HEADER_PAYLOAD_POINTER_INDEX;
	
	memcpy(ptr, &payload_pointer, sizeof(uint64_t));
}

uint32_t FileManager::get_header_payload_length(const PackedByteArray &header_buffer) const {
	const uint8_t *ptr = header_buffer.ptr() + BLOCK_HEADER_PAYLOAD_LENGTH_INDEX;
	
	uint32_t value;
	memcpy(&value, ptr, sizeof(uint32_t));
	return value;
}

void FileManager::store_header_payload_length(PackedByteArray &header_buffer, uint32_t payload_length) {
	uint8_t *ptr = header_buffer.ptrw() + BLOCK_HEADER_PAYLOAD_LENGTH_INDEX;
	
	memcpy(ptr, &payload_length, sizeof(uint32_t));
}

FileManager::BlockHeaderBitset FileManager::get_header_bool_bitset(const PackedByteArray &header_buffer) const {
	uint8_t bits = header_buffer[BLOCK_HEADER_BOOLEAN_BITSET_INDEX];
	
	BlockHeaderBitset bool_bitset;
	bool_bitset.is_active   = (bits & (1u << 0)) != 0;
	bool_bitset.is_dense    = (bits & (1u << 1)) != 0;
	bool_bitset.reserved_2  = (bits & (1u << 2)) != 0;
	bool_bitset.reserved_3  = (bits & (1u << 3)) != 0;
	bool_bitset.reserved_4  = (bits & (1u << 4)) != 0;
	bool_bitset.reserved_5  = (bits & (1u << 5)) != 0;
	bool_bitset.reserved_6  = (bits & (1u << 6)) != 0;
	bool_bitset.reserved_7  = (bits & (1u << 7)) != 0;
	
	return bool_bitset;
}

void FileManager::store_header_bool_bitset(PackedByteArray &header_buffer, const FileManager::BlockHeaderBitset &bool_bitset) {
	uint8_t bits = 0;
	
	bits |= static_cast<uint8_t>(bool_bitset.is_active)  << 0;
	bits |= static_cast<uint8_t>(bool_bitset.is_dense)   << 1;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_2) << 2;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_3) << 3;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_4) << 4;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_5) << 5;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_6) << 6;
	bits |= static_cast<uint8_t>(bool_bitset.reserved_7) << 7;
	
	header_buffer.ptrw()[BLOCK_HEADER_BOOLEAN_BITSET_INDEX] = bits;
}

uint8_t FileManager::get_header_padding_bytes(const PackedByteArray &header_buffer) const {
	return header_buffer[BLOCK_HEADER_PADDING_BYTES_INDEX];
}

void FileManager::store_header_padding_bytes(PackedByteArray &header_buffer, uint8_t padding_bytes) {
	header_buffer.ptrw()[BLOCK_HEADER_PADDING_BYTES_INDEX] = padding_bytes;
}

uint16_t FileManager::get_header_voxel_count(const PackedByteArray &header_buffer) const {
	const uint8_t *ptr = header_buffer.ptr() + BLOCK_HEADER_VOXEL_COUNT_INDEX;
	
	uint16_t value;
	memcpy(&value, ptr, sizeof(uint16_t));
	return value;
}

void FileManager::store_header_voxel_count(PackedByteArray &header_buffer, uint16_t voxel_count) {
	uint8_t *ptr = header_buffer.ptrw() + BLOCK_HEADER_VOXEL_COUNT_INDEX;
	
	memcpy(ptr, &voxel_count, sizeof(uint16_t));
}









void FileManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_files", "block_header_file_directory", "block_payload_file_directory", "temp_block_header_file_directory", "temp_block_payload_file_directory"), &FileManager::open_files);
	ClassDB::bind_method(D_METHOD("save_voxels", "block_position", "voxel_data"), &FileManager::save_voxels);
	ClassDB::bind_method(D_METHOD("load_voxels", "block_position"), &FileManager::load_voxels);
	ClassDB::bind_method(D_METHOD("compact_files"), &FileManager::compact_files);
}
