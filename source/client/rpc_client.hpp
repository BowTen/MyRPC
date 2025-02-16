#pragma once
#include "client.hpp"

namespace btrpc {
namespace client {

class RpcClient : public Client {
   public:
    using ptr = std::shared_ptr<RpcClient>;
    RpcClient(const std::string& sip, int sport) : Client(sip, sport) {}

	bool call(const std::string& method, const Json::Value& params, Json::Value& result) {
        DLOG("开始同步rpc调用...");
        // 1. 组织请求
        auto req_msg = MessageFactory::create<RpcRequest>();
        //req_msg->setId(UUID::uuid());
        req_msg->setMType(MType::REQ_RPC);
        req_msg->setMethod(method);
        req_msg->setParams(params);
		DLOG("rpc请求 id=%s", req_msg->rid().c_str());
        BaseMessage::ptr rsp_msg;
        // 2. 发送请求
        bool ret = send(std::dynamic_pointer_cast<BaseMessage>(req_msg), rsp_msg);
        if (ret == false) {
            ELOG("同步Rpc请求失败！");
            return false;
        }
        DLOG("收到响应，进行解析，获取结果!");
        // 3. 等待响应
        auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
        if (!rpc_rsp_msg) {
            ELOG("rpc响应，向下类型转换失败！");
            return false;
        }
        if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) {
            ELOG("rpc请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
            return false;
        }
        result = rpc_rsp_msg->result();
        DLOG("结果设置完毕！");
        return true;
    }

    void onConnectMessage(const BaseConnection::ptr& conn, ConnectResponse::ptr& msg) {
        ELOG("服务端连接信息：%s", errReason(msg->rcode()).c_str());
		if(msg->rcode() == RCode::RCODE_CONNECT_OVERFLOW){
			exit(0);
		}
    }
};

}  // namespace client
}  // namespace btrpc