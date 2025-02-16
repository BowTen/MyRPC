#pragma once
#include "../common/dispatcher.hpp"
#include "../common/net.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"

namespace btrpc {
namespace client {

class Client : public MuduoClient {
   public:
    using ptr = std::shared_ptr<Client>;
    Client(const std::string& sip, int sport)
        : MuduoClient(sip, sport),
          _dispatcher(std::make_shared<Dispatcher>()),
          _requestor(std::make_shared<Requestor>()) {
        auto msg_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                                std::placeholders::_1, std::placeholders::_2);
        setMessageCallback(msg_cb);

        auto rsp_cb = std::bind(&btrpc::client::Requestor::onResponse, _requestor.get(),
                                std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);
        _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

        connect();
        _conn = connection();
        if (_conn == nullptr) {
            DLOG("_conn为空");
            exit(0);
        }
    }

	template <typename T>
	void registerHandler(MType mtype, std::function<void(const BaseConnection::ptr&, std::shared_ptr<T>&)> func){
		_dispatcher->registerHandler<T>(mtype, func);
	}

    bool send(const BaseMessage::ptr& req, AsyncResponse& async_rsp) {
        return _requestor->send(_conn, req, async_rsp);
    }
    bool send(const BaseMessage::ptr& req, BaseMessage::ptr& rsp) {
        return _requestor->send(_conn, req, rsp);
    }
    bool send(const BaseMessage::ptr& req, const RequestCallback& cb) {
        return _requestor->send(_conn, req, cb);
    }

	Address getHost(){
		return _conn->getHost();
	}

   private:
    Dispatcher::ptr _dispatcher;
    Requestor::ptr _requestor;
    BaseConnection::ptr _conn;
};

}  // namespace client
}  // namespace btrpc