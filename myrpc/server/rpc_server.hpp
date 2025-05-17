#pragma once
#include "../common/dispatcher.hpp"
#include "../common/net.hpp"
#include "rpc_router.hpp"
#include "server.hpp"

namespace myrpc {
namespace server {

class RpcServer : public Server {
   public:
    using prt = std::shared_ptr<RpcServer>;
    RpcServer(int port, int max_connections = (1 << 16), int overflow = 5)
        : Server(port, max_connections + overflow),
          _max_connections(max_connections),
          _router(std::make_shared<RpcRouter>()) {
        auto rpc_req_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(),
                                    std::placeholders::_1, std::placeholders::_2);
        registerHandler<RpcRequest>(MType::REQ_RPC, rpc_req_cb);
        auto svc_req_cb = std::bind(&RpcServer::onServiceRequest, this,
                                    std::placeholders::_1, std::placeholders::_2);
        registerHandler<ServiceRequest>(MType::REQ_SERVICE, svc_req_cb);
    }

    void registerMethod(const MethodDescribe::ptr& service) {
        _router->registerMethod(service);
    }

	void onServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& req){
		auto optype = req->optype();
		if(optype == ServiceOptype::SERVICE_DETECT){
			DLOG("收到心跳检测，idle=%d", idleCount());
			return responseIdle(conn, req);
		}else{
			ELOG("无法处理的操作类型: %d", (int)optype);
		}
	}

	void responseIdle(const BaseConnection::ptr& conn, const ServiceRequest::ptr& req){
		auto rsp = std::make_shared<ServiceResponse>();
		rsp->setId(req->rid());
		rsp->setRCode(RCode::RCODE_OK);
		rsp->setIdleCount(idleCount());
		DLOG("回复心跳检测，idle=%d， msgidle=%d, rid=%s", idleCount(), rsp->idleCount(), rsp->rid().c_str());
		conn->send(std::dynamic_pointer_cast<BaseMessage>(rsp));
	}


    int idleCount() {
        return _max_connections - connectionNumber();
    }

   private:
    int _max_connections;
    int _overflow;
    RpcRouter::ptr _router;
};

}  // namespace server
}  // namespace myrpc