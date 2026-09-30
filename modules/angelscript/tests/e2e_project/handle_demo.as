// godot_base: Node
class handle_demo {
	void _ready() {
		// M3 句柄存活路径：非拥有句柄经真实绑定往返后仍可正常指向存活对象。
		// 失效路径由单测覆盖：AS 2.38 脚本层没有异常捕获语法，脚本内无法断言"抛异常"。
		Node @child = Node();
		// 静态类型 Object@ ⇒ 非拥有句柄（槽里存 ObjectID），赋值不改变它指向的对象。
		// 这里的 `!is null` 会真正走一次"槽 → ObjectDB"解码，而不是比较裸指针。
		Object @as_weak = child;
		if (as_weak !is null) {
			as_log_string("M3:handle-ok");
		}
	}
}
