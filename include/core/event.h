#pragma once

#include <map>
#include <utility>

namespace rollback_netcode {

template <typename Func>
class Event {
public:
	// 注册观察者，支持右值引用
	int connect(Func &&f) { return Assign(f); }

	// 注册观察者，左值
	int connect(const Func &f) { return Assign(f); }

	// 移除观察者
	void disconnect(int key) { m_connections.erase(key); }

	// 通知所有观察者
	template <typename... Args>
	void invoke(Args &&...args) {
		for (auto &it : m_connections) {
			auto &func = it.second;
			func(std::forward<Args>(args)...);
		}
	}

	// 禁用复制构造函数
	Event(const Event &n) = delete; // deleted
	// 禁用赋值构造函数
	Event &operator=(const Event &n) = delete; // deleted
	Event() = default; // available
private:
	// 保存观察者并分配观察者编号
	template <typename F>
	int Assign(F &&f) {
		int k = m_observerId++;
		m_connections.emplace(k, std::forward<F>(f));
		return k;
	}

	int m_observerId = 0; // 观察者对应编号
	std::map<int, Func> m_connections; // 观察者列表
};

} //namespace rollback_netcode