// godot_base: Node
class main {
	int ready_count = 0;

	void _ready() {
		ready_count = 42;
		as_log_int(ready_count);
	}
}
