#include "../common/dispatcher.hpp"
#include "../common/net.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"

namespace btrpc {
namespace client {

class RpcClient {
   public:
    using ptr = std::shared_ptr<RpcClient>;
    RpcClient(const std::string& sip, int sport)
        : _client(ClientFactory::create<MyClient>(sip, sport)),
          _dispatcher(std::make_shared<Dispatcher>()),
          _requestor(std::make_shared<Requestor>()),
          _caller(std::make_shared<RpcCaller>(_requestor)) {
        auto msg_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                                std::placeholders::_1, std::placeholders::_2);
        _client->setMessageCallback(msg_cb);

        auto rsp_cb = std::bind(&btrpc::client::Requestor::onResponse, _requestor.get(),
                                std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);

        _client->connect();
        _conn = _client->connection();
    }

	bool call(const std::string& method, const Json::Value &params, Json::Value &reslut){
		return _caller->call(_conn, method, params, reslut);
	}

   private:
    MyClient::ptr _client;
    Dispatcher::ptr _dispatcher;
    Requestor::ptr _requestor;
    RpcCaller::ptr _caller;
    BaseConnection::ptr _conn;
};

}  // namespace client
}  // namespace btrpc