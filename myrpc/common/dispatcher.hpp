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
		//DLOG("dispatcher收到消息，rid=%s", msg->rid().c_str());
		myrpc::MessageCallback* callback = nullptr;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			auto it = _handlers.find(msg->mtype());
			if(it != _handlers.end()) callback = &(it->second);
		}
		if(callback) return (*callback)(conn, msg);
        ELOG("收到未知类型的消息: %d！", static_cast<int>(msg->mtype()));
        conn->shutdown();
    }

   private:
    std::mutex _mutex;
    std::unordered_map<MType, myrpc::MessageCallback> _handlers;

};
}  // namespace bitrpc

