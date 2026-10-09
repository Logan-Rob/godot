#ifndef VOXEL_ENGINE_H
#define VOXEL_ENGINE_H

#include "scene/3d/node_3d.h"

class VoxelEngine : public Node3D {
	GDCLASS(VoxelEngine, Node3D);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	VoxelEngine();
};

#endif
