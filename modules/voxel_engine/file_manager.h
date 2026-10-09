#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "core/io/file_access.h"

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

	String block_header_file_directory = "user://saves/test_save/terrain.vbh";
	String block_payload_file_directory = "user://saves/test_save/terrain.vpl";
};

#endif
