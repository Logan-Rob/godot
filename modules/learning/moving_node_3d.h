#ifndef MOVING_NODE_3D_H
#define MOVING_NODE_3D_H

#include "scene/3d/node_3d.h"

class MovingNode3D : public Node3D {
	GDCLASS(MovingNode3D, Node3D);

protected: // what is bind methods
	static void _bind_methods();
	void _notification(int p_what);

public:
	MovingNode3D();
};

#endif