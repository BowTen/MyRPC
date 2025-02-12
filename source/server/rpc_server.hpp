#include "../common/dispatcher.hpp"
#include "../common/net.hpp"
#include "rpc_router.hpp"

namespace btrpc {
namespace server {

class RpcServer {
   public:
    using prt = std::shared_ptr<RpcServer>;
    RpcServer(int port, int max_connections = (1 << 16), int overflow = 5)
        : _max_connections(max_connections),
          _server(ClientFactory::create<MyServer>(port, max_connections + overflow)),
          _dispatcher(std::make_shared<Dispatcher>()),
          _router(std::make_shared<RpcRouter>()) {
        auto msg_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
        _server->setMessageCallback(msg_cb);
        auto rpc_req_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(),
                                    std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<RpcRequest>(MType::REQ_RPC, rpc_req_cb);
    }

	void start(){
        _server->start();
	}

    void registerMethod(const MethodDescribe::ptr& service) {
        _router->registerMethod(service);
    }

    int allowance() {
        return _max_connections - _server->connectionNumber();
    }

   private:
    int _max_connections;
    int _overflow;
    MyServer::ptr _server;
    Dispatcher::ptr _dispatcher;
    RpcRouter::ptr _router;
};

}  // namespace server
}  // namespace btrpc