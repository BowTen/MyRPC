#pragma once
#include <functional>
#include <future>
#include "../common/message.hpp"
#include "../common/net.hpp"

namespace btrpc {
namespace client {

using RequestCallback = std::function<void(const BaseMessage::ptr&)>;
using AsyncResponse = std::future<BaseMessage::ptr>;
	
class Requestor {
   public:
    using ptr = std::shared_ptr<Requestor>;
    struct RequestDescribe {
        using ptr = std::shared_ptr<RequestDescribe>;
        BaseMessage::ptr request;
        RType rtype;
        std::promise<BaseMessage::ptr> response;
        RequestCallback callback;
    };
    void onResponse(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) {
		DLOG("requestor收到响应 rid=%s", msg->rid().c_str());
        std::string rid = msg->rid();
        RequestDescribe::ptr rdp = getDescribe(rid);
        if (rdp.get() == nullptr) {
            ELOG("收到响应 - %s，但是未找到对应的请求描述！", rid.c_str());
            return;
        }
		DLOG("requestor找到响应对应的请求");
        if (rdp->rtype == RType::REQ_ASYNC) {
			DLOG("requestor设置异步结果");
            rdp->response.set_value(msg);
			DLOG("requestor设置异步成功");
        } else if (rdp->rtype == RType::REQ_CALLBACK) {
            if (rdp->callback)
                rdp->callback(msg);
        } else {
            ELOG("请求类型未知！！");
        }
        delDescribe(rid);
    }
    bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, AsyncResponse& async_rsp) {
        RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_ASYNC);
        if (rdp.get() == nullptr) {
            ELOG("构造请求描述对象失败！");
            return false;
        }
        conn->send(req);
        async_rsp = rdp->response.get_future();
        return true;
    }
    bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, BaseMessage::ptr& rsp) {
        DLOG("开始同步调用...");
        AsyncResponse rsp_future;
        bool ret = send(conn, req, rsp_future);
        if (ret == false) {
            return false;
        }
		DLOG("异步调用成功")
        rsp = rsp_future.get();
		DLOG("同步调用成功 rsp_rid=%s", rsp->rid().c_str());
        return true;
    }
    bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, const RequestCallback& cb) {
        RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_CALLBACK, cb);
        if (rdp.get() == nullptr) {
            ELOG("构造请求描述对象失败！");
            return false;
        }
        conn->send(req);
        return true;
    }

   private:
    RequestDescribe::ptr newDescribe(const BaseMessage::ptr& req, RType rtype, const RequestCallback& cb = RequestCallback()) {
        std::unique_lock<std::mutex> lock(_mutex);
        RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
        rd->request = req;
        rd->rtype = rtype;
        if (rtype == RType::REQ_CALLBACK && cb) {
            rd->callback = cb;
        }
        _request_desc.insert(std::make_pair(req->rid(), rd));
        return rd;
    }
    RequestDescribe::ptr getDescribe(const std::string& rid) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _request_desc.find(rid);
        if (it == _request_desc.end()) {
            return RequestDescribe::ptr();
        }
        return it->second;
    }
    void delDescribe(const std::string& rid) {
        std::unique_lock<std::mutex> lock(_mutex);
        _request_desc.erase(rid);
    }

   private:
    std::mutex _mutex;
    std::unordered_map<std::string, RequestDescribe::ptr> _request_desc;
};
}  // namespace client
}  // namespace btrpc