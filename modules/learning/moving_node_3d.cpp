#include "core/config/engine.h"
#include "moving_node_3d.h"

void MovingNode3D::_bind_methods() {
}

void MovingNode3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		print_line("MovingNode3D process");

		Vector3 new_position = get_position();
		new_position.y += get_process_delta_time();
		set_position(new_position);
	}
}

MovingNode3D::MovingNode3D() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		set_process(true);
	}
}
