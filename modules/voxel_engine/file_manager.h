#pragma once

#include "core/io/file_access.h"
#include "core/math/vector3i.h"
#include "core/templates/hash_map.h"
#include "voxel_constants.h"

class FileManager {
private:
	static constexpr int HEADER_FILE_HEADER_LENGTH = 16;
	static constexpr int PAYLOAD_FILE_HEADER_LENGTH = 16;

	static constexpr int BLOCK_HEADER_LENGTH = 28;
    static constexpr int BLOCK_HEADER_POSITION_INDEX = 0;
    static constexpr int BLOCK_HEADER_PAYLOAD_POINTER_INDEX = 12;
    static constexpr int BLOCK_HEADER_PAYLOAD_LENGTH_INDEX = 20;
    static constexpr int BLOCK_HEADER_ACTIVE_BOOL_INDEX = 24;
    static constexpr int BLOCK_HEADER_FORMAT_BOOL_INDEX = 25;
    static constexpr int BLOCK_HEADER_EDITED_VOXEL_COUNT_INDEX = 26;

    static constexpr int SPARSE_DENSE_VOXEL_THRESHOLD = 16384;

	struct DenseVoxelData {
        PackedByteArray densities;
        PackedByteArray materials;

        DenseVoxelData() {
        	densities.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
         	materials.resize(VoxelConstants::BLOCK_VOXEL_VOLUME);
        }

        void set_voxel(int index, int density, int material) {
        	if (index < 0 || index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
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
        HashMap<int, int> index_lookup;

        void set_voxel(int index, int density, int material) {
       		if (index < 0 || index >= VoxelConstants::BLOCK_VOXEL_VOLUME) {
        		return;
        	}

         	if (index_lookup.has(index)) {
          		int *array_index = index_lookup.getptr(index);
            	densities.set(*array_index, density);
             	materials.set(*array_index, material);
          	} else {
           		index_lookup[index] = indices.size();
           		densities.append(density);
             	materials.append(material);
              	indices.append(index);
           	}
        }

        void remove_voxel(int index) {
        	int *array_index_ptr = index_lookup.getptr(index);
            if (array_index_ptr == nullptr) {
                return;
            }

			int array_index = *array_index_ptr;
			int last_index = indices.size() - 1;

			if (array_index != last_index) {
			    int moved_voxel_index = indices[last_index];

			    densities.set(array_index, densities[last_index]);
			    materials.set(array_index, materials[last_index]);
				indices.set(array_index, moved_voxel_index);

			    index_lookup[moved_voxel_index] = array_index;
			}

			densities.resize(last_index);
			materials.resize(last_index);
			indices.resize(last_index);

			index_lookup.erase(index);
        }
    };

    String block_header_file_directory = "user://saves/test_save/terrain.vbh";
	String block_payload_file_directory = "user://saves/test_save/terrain.vpl";
	bool is_header_file_initialized = false;
	bool is_payload_file_initialized = false;

	HashMap<Vector3i, int> header_index_lookup;
	PackedInt32Array free_header_indices;

	void create_header_file_header();
	void create_payload_file_header();
	void compact_files();
	void store_block_header_indices();
	void create_block_header();
	void remove_block_header();

	void move_cursor_to_header_property();
	int ensure_header_existance();


public:
	void save_block_data(Vector3i block_position, const SparseVoxelData &voxel_data);
	SparseVoxelData load_block_data(Vector3i block_position);
};
