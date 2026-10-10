// godot_base: Node
class binding_demo {
	void _ready() {
		// 热更脚本类本身不是 Node 子类（基类指令只决定节点实例类型，D2），
		// 因此绑定层用显式对象句柄来演示。
		Node @n = Node();
		// 对象方法 + String 值类型编组：Object.get_class() 返回 String。
		as_log_string(n.get_class());
		// 对象参数 + int64 返回值：Node.add_child(Node@, bool, int64) / get_child_count(bool)。
		Node @child = Node();
		n.add_child(child, false, 0);
		as_log_int(int(n.get_child_count(false)));
		// 属性读写（int64）。
		n.process_mode = 3;
		as_log_int(int(n.process_mode));
	}
}
