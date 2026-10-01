// godot_base: Node
class handle_demo {
	void _ready() {
		// M3 句柄存活路径：非拥有句柄经真实绑定往返后仍可正常指向存活对象。
		// 失效路径由单测覆盖：AS 2.38 脚本层没有异常捕获语法，脚本内无法断言"抛异常"。
		Node @child = Node();
		// 静态类型 Object@ ⇒ 非拥有句柄（槽里存 ObjectID），赋值不改变它指向的对象。
		// 注意：`is null` 只比较槽原值、不做存活校验，对象释放后槽值不变（非 0），弱句柄不会变 null；
		// 失效只能靠"访问"（调方法/读写属性）暴露为异常。这里判的是"句柄非空"，对存活对象成立。
		Object @as_weak = child;
		if (as_weak !is null) {
			as_log_string("M3:handle-ok");
		}
	}
}
