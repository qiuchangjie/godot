// godot_base: Node
// M3 信号闭环：声明（signal_ping）→ 连接（as_callable）→ 发射（as_emit_signal）→ 回调（on_ping）。
class signal_demo {
	void signal_ping(int value) {}

	void _ready() {
		// Object.connect 的形参是 StringName，字面量 "ping" 是 AS string 且二者之间没有
		// 注册隐式转换；同时 flags 形参没有默认值，必须显式传 0。
		as_self().connect(StringName("ping"), as_callable(as_self(), "on_ping"), 0);

		Array args;
		args.push_back(Variant(7));
		as_emit_signal(as_self(), "ping", args);
	}

	void on_ping(int value) {
		as_log_int(int(value));
	}
}
