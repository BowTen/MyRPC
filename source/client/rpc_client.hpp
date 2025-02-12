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

        auto rsp_conn_cb = std::bind(&RpcClient::onConnectMessage, this,
                                     std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<ConnectResponse>(MType::RSP_CONNECT, rsp_conn_cb);

            _client->connect();
        _conn = _client->connection();
		if(_conn == nullptr){
			DLOG("_conn为空");
			exit(0);
		}
    }

    bool call(const std::string& method, const Json::Value& params, Json::Value& reslut) {
        return _caller->call(_conn, method, params, reslut);
    }

    void onConnectMessage(const BaseConnection::ptr& conn, ConnectResponse::ptr& msg) {
        ELOG("服务端连接信息：%s", errReason(msg->rcode()).c_str());
		if(msg->rcode() == RCode::RCODE_CONNECT_OVERFLOW){
			exit(0);
		}
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