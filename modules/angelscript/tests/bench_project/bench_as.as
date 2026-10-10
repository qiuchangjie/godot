// godot_base: Node
// AngelScript 实现。方法名与 GDScript / C# 一致，校验和亦须一致。
//
// 句柄语义提醒：AS 里 `h = other`（无 @）对句柄是「对象值拷贝」，句柄重绑定必须写
// `@h = @other`。因此本文件刻意不在循环里重绑定句柄，避免踩这个坑。

class BenchItem {
	int a;
	int b;
}

class bench_as {
	// P2 隔离探针用的预置容器（仅在 BENCH_PROBE=1 下使用）。
	Array _probe_arr;
	PackedInt64Array _probe_packed;
	Dictionary _probe_dict;

	int bench_noop() {
		return 1;
	}

	int bench_arith(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			int t = i & 0x7FFF;
			s = (s + t * t) & 0x3FFFFFFF;
		}
		return s;
	}

	int add(int a, int b) {
		return a + b;
	}

	int bench_calls(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = add(s, i) & 0x3FFFFFFF;
		}
		return s;
	}

	int bench_class(int n) {
		BenchItem@ i0 = BenchItem();
		i0.a = 1;
		i0.b = 2;
		BenchItem@ i1 = BenchItem();
		i1.a = 2;
		i1.b = 4;
		BenchItem@ i2 = BenchItem();
		i2.a = 3;
		i2.b = 6;
		BenchItem@ i3 = BenchItem();
		i3.a = 4;
		i3.b = 8;
		int s = 0;
		for (int i = 0; i < n; i++) {
			int k = i & 3;
			if (k == 0) {
				i0.a = (i0.a + i0.b + 1) & 0x3FFFFFFF;
				s = (s + i0.a) & 0x3FFFFFFF;
			} else if (k == 1) {
				i1.a = (i1.a + i1.b + 1) & 0x3FFFFFFF;
				s = (s + i1.a) & 0x3FFFFFFF;
			} else if (k == 2) {
				i2.a = (i2.a + i2.b + 1) & 0x3FFFFFFF;
				s = (s + i2.a) & 0x3FFFFFFF;
			} else {
				i3.a = (i3.a + i3.b + 1) & 0x3FFFFFFF;
				s = (s + i3.a) & 0x3FFFFFFF;
			}
		}
		return s;
	}

	int bench_array(int n) {
		Array arr;
		for (int i = 0; i < n; i++) {
			arr.push_back(i);
		}
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(arr[i])) & 0x3FFFFFFF;
		}
		return s;
	}

	int bench_string(int n) {
		// AS 原生 string 只注册了赋值与 length()，没有拼接；字符串运算只能用 Godot String（以 Variant 存储）。
		String s = String();
		for (int i = 0; i < n; i++) {
			s = s + "x";
		}
		return int(s.length());
	}

	int bench_vector(int n) {
		Vector2 v(1.5, 2.5);
		Vector2 acc;
		for (int i = 0; i < n; i++) {
			acc = acc + v;
		}
		return int(acc.x + acc.y);
	}

	int bench_engine(int n) {
		// as_self() 返回 Object@，本模块不支持向下转换到 Node@，只能自建一个 Node 句柄来调用引擎方法。
		Node @selfn = Node();
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(selfn.get_child_count(false))) & 0x3FFFFFFF;
		}
		return s;
	}

	// ---------- P2：容器 / 引擎 API 路径隔离探针（仅 AS；不需跨语言校验和）----------
	// 每个方法只测一种绑定操作的边际成本；预热与重复由驱动器统一负责。

	void probe_prepare(int n) {
		_probe_arr = Array();
		_probe_arr.resize(n);
		for (int i = 0; i < n; i++) {
			_probe_arr[i] = i;
		}
		_probe_packed = PackedInt64Array();
		_probe_packed.resize(n);
		for (int i = 0; i < n; i++) {
			_probe_packed[i] = i;
		}
		_probe_dict = Dictionary();
		for (int i = 0; i < n; i++) {
			_probe_dict[i] = i;
		}
	}

	int probe_loop(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + i) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_array_push(int n) {
		Array arr;
		for (int i = 0; i < n; i++) {
			arr.push_back(i);
		}
		return int(arr.size());
	}

	int probe_array_read(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(_probe_arr[i])) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_array_write(int n) {
		for (int i = 0; i < n; i++) {
			_probe_arr[i] = i;
		}
		return int(_probe_arr.size());
	}

	int probe_array_size(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(_probe_arr.size())) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_packed_read(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(_probe_packed[i])) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_packed_write(int n) {
		for (int i = 0; i < n; i++) {
			_probe_packed[i] = i;
		}
		return int(_probe_packed.size());
	}

	int probe_dict_get(int n) {
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(_probe_dict[i])) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_dict_set(int n) {
		for (int i = 0; i < n; i++) {
			_probe_dict[i] = i;
		}
		return int(_probe_dict.size());
	}

	int probe_engine_scalar(int n) {
		Node @selfn = Node();
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + int(selfn.get_child_count(false))) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_engine_prop_get(int n) {
		Node @selfn = Node();
		int s = 0;
		for (int i = 0; i < n; i++) {
			s = (s + selfn.process_priority) & 0x3FFFFFFF;
		}
		return s;
	}

	int probe_engine_prop_set(int n) {
		Node @selfn = Node();
		for (int i = 0; i < n; i++) {
			selfn.process_priority = i & 1023;
		}
		return selfn.process_priority;
	}
}
