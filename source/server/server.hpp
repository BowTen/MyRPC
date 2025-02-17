#pragma once

#include "../common/dispatcher.hpp"
#include "../common/net.hpp"

namespace myrpc {
namespace server {

class Server : public MuduoServer {
   public:
    using prt = std::shared_ptr<Server>;
    Server(int port, int max_connections = (1 << 16))
        : MuduoServer(port, max_connections),
		_dispatcher(std::make_shared<Dispatcher>()) {
        auto msg_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
        setMessageCallback(msg_cb);
    }

	template <typename T>
	void registerHandler(MType mtype, std::function<void(const BaseConnection::ptr&, std::shared_ptr<T>&)> func){
		return _dispatcher->registerHandler<T>(mtype, func);
	}

   private:
    Dispatcher::ptr _dispatcher;
};

}  // namespace server
}  // namespace myrpc