#pragma once
#include <unordered_set>
#include "requestor.hpp"

namespace btrpc {
namespace client {

class Provider {
   public:
    using ptr = std::shared_ptr<Provider>;
    Provider(const Requestor::ptr& requestor)
        : _requestor(requestor) {}
    bool registryMethod(const BaseConnection::ptr& conn, const std::string& method, const Address& host) {
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        msg_req->setOptype(ServiceOptype::SERVICE_REGISTRY);
        BaseMessage::ptr msg_rsp;
        bool ret = _requestor->send(conn, msg_req, msg_rsp);
        if (ret == false) {
            ELOG("%s 服务注册请求发送失败！", method.c_str());
            return false;
        }
        auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
        if (service_rsp.get() == nullptr) {
            ELOG("响应类型向下转换失败！");
            return false;
        }
        if (service_rsp->rcode() != RCode::RCODE_OK) {
            ELOG("服务注册失败，原因：%s", errReason(service_rsp->rcode()).c_str());
            return false;
        }
        return true;
    }
    bool deregisterMethod(const BaseConnection::ptr& conn, const std::string& method, const Address& host) {
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        msg_req->setOptype(ServiceOptype::SERVICE_DEREGISTER);
        BaseMessage::ptr msg_rsp;
        bool ret = _requestor->send(conn, msg_req, msg_rsp);
        if (ret == false) {
            ELOG("%s 服务注销请求发送失败！", method.c_str());
            return false;
        }
        auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
        if (service_rsp.get() == nullptr) {
            ELOG("响应类型向下转换失败！");
            return false;
        }
        if (service_rsp->rcode() != RCode::RCODE_OK) {
            ELOG("服务注销失败，原因：%s", errReason(service_rsp->rcode()).c_str());
            return false;
        }
        return true;
    }

   private:
    Requestor::ptr _requestor;
};

class MethodHost {
   public:
    using ptr = std::shared_ptr<MethodHost>;
    bool empty(const std::string& method) { 
		if(_hosts.count(method) == 0 || _hosts[method].empty()) {
			_hosts.erase(method);
			return true;
		}
		return false;
	}
    Address chose(const std::string& method) {
        if (_hosts.count(method) == 0) {
            ELOG("没有提供服务的主机");
            return Address("127.0.0.1", 0);
        }
        return _hosts[method].chose();
    }
    void remove(const std::string& method, const Address& host) { if(_hosts.count(method)) _hosts[method].remove(host); }
    void add(const std::string& method, const Address& host) { _hosts[method].add(host); }
    void add(const std::string& method, const std::vector<Address>& hosts) {
		if(hosts.empty()) return;
		for(auto &host : hosts) add(method, host);
	}
   private:
    class RRHost {
       public:
        using ptr = std::shared_ptr<MethodHost>;
        bool empty() {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_que.empty())
                return true;
            while (_rm_list.size() && _rm_list.count(_que.front())) {
                _rm_list.erase(_que.front());
                _que.pop_front();
            }
            return _que.empty();
        }
        Address chose() {
            std::unique_lock<std::mutex> lock(_mutex);
            if (empty()) {
                ELOG("没有提供服务的主机");
                return Address("127.0.0.1", 0);
            }
            _que.push_back(_que.front());
            _que.pop_front();
            return _que.back();
        }
        void remove(const Address& host) {
            std::unique_lock<std::mutex> lock(_mutex);
            _rm_list.insert(host);
        }
        void add(const Address& host) {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_rm_list.count(host)) {
                _rm_list.erase(host);
                return;
            }
            _que.push_back(host);
        }

       private:
        std::mutex _mutex;
        std::deque<Address> _que;
        std::unordered_set<Address> _rm_list;
    };
    std::unordered_map<std::string, RRHost> _hosts;
};
class Discoverer {
   public:
    using OfflineCallback = std::function<void(const Address&)>;
    using ptr = std::shared_ptr<Discoverer>;
    Discoverer(const Requestor::ptr& requestor, const OfflineCallback& cb)
        : _requestor(requestor), _offline_callback(cb) {}
    bool serviceDiscovery(const BaseConnection::ptr& conn, const std::string& method, Address& host) {
        // 当前所保管的提供者信息存在，则直接返回地址
		if(!_method_hosts->empty(method)){
			host = _method_hosts->chose(method);
			return true;
		}
		// 当前服务的提供者为空
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setOptype(ServiceOptype::SERVICE_DISCOVERY);
        BaseMessage::ptr msg_rsp;
        bool ret = _requestor->send(conn, msg_req, msg_rsp);
        if (ret == false) {
            ELOG("服务发现失败！");
            return false;
        }
        auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
        if (!service_rsp) {
            ELOG("服务发现失败！响应类型转换失败！");
            return false;
        }
        if (service_rsp->rcode() != RCode::RCODE_OK) {
            ELOG("服务发现失败！%s", errReason(service_rsp->rcode()).c_str());
            return false;
        }
		_method_hosts->add(method, service_rsp->hosts());
        if (_method_hosts->empty(method)) {
            ELOG("%s 服务发现失败！没有能够提供服务的主机！", method.c_str());
            return false;
        }
		host = _method_hosts->chose(method);
		return true;
    }
    // 这个接口是提供给Dispatcher模块进行服务上线下线请求处理的回调函数
    void onServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg) {
        auto optype = msg->optype();
        std::string method = msg->method();
        std::unique_lock<std::mutex> lock(_mutex);
        if (optype == ServiceOptype::SERVICE_ONLINE) {
			_method_hosts->add(method, msg->host());
		} else if (optype == ServiceOptype::SERVICE_OFFLINE) {
			_method_hosts->remove(method, msg->host());
        }else {
			ELOG("错误的请求类型: %d", (int)optype);
		}
    }

   private:
    OfflineCallback _offline_callback;
    std::mutex _mutex;
    MethodHost::ptr _method_hosts;
    Requestor::ptr _requestor;
};

}  // namespace client
}  // namespace btrpc
