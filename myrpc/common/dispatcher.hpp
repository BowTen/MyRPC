#pragma once
#include "message.hpp"
#include "net.hpp"

namespace myrpc {

template <typename T>
void MessageCallbackAdapter(const BaseConnection::ptr& conn, BaseMessage::ptr& msg,
					   std::function<void(const BaseConnection::ptr&, std::shared_ptr<T>&)> func){
	auto type_msg = std::dynamic_pointer_cast<T>(msg);
	func(conn, type_msg);
}

class Dispatcher {
   public:
    using ptr = std::shared_ptr<Dispatcher>;

	template <typename T>
	void registerHandler(MType mtype, std::function<void(const BaseConnection::ptr&, std::shared_ptr<T>&)> func){
		std::unique_lock<std::mutex> lock(_mutex);
		auto cb = std::bind(MessageCallbackAdapter<T>, std::placeholders::_1, std::placeholders::_2, func);
		_handlers.insert(std::make_pair(mtype, cb));
	}

    void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) {
		// 找到消息类型对应的业务处理函数，进行调用即可
        std::unique_lock<std::mutex> lock(_mutex);
		//DLOG("dispatcher收到消息，rid=%s", msg->rid().c_str());
		auto it = _handlers.find(msg->mtype());
		if(it != _handlers.end()){
			return (it->second)(conn, msg);
		}
        // 没有找到指定类型的处理回调--因为客户端和服务端都是我们自己设计的，因此不可能出现这种情况
        ELOG("收到未知类型的消息: %d！", static_cast<int>(msg->mtype()));
        conn->shutdown();
    }

   private:
    std::mutex _mutex;
    std::unordered_map<MType, myrpc::MessageCallback> _handlers;

};
}  // namespace bitrpc

