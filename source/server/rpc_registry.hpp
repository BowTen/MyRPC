#pragma once
#include <unordered_map>
#include <unordered_set>
#include "../common/message.hpp"
#include "../common/net.hpp"

// 提供者注册、注销方法  provider
// 提供者下线时删除注销他的所有方法  provider
// 请求者发现方法  requester
// 方法注册、注销时通知所有发现过该方法的请求者  requester

namespace btrpc {
namespace server {

class ProviderManager {
   public:
    using ptr = std::shared_ptr<ProviderManager>;
    bool registry(const BaseConnection::ptr& conn, std::string& method, Address& host) {
        std::unique_lock<std::mutex> lock(_mutex);
        // 检查是否注册过
        auto conn_it = _connections.find(conn);
        auto pr = std::make_pair(method, host);
        if (conn_it != _connections.end()) {
            if (conn_it->second.find(pr) == conn_it->second.end())
                return false;
        }
        _connections[conn].insert(pr);
        _methods[method].insert(host);
        return true;
    }
    bool deregister(const BaseConnection::ptr& conn, std::string& method, Address& host) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto pr = std::make_pair(method, host);
        auto conn_it = _connections.find(conn);
        if (conn_it != _connections.end()) {
            conn_it->second.erase(pr);
            if (conn_it->second.empty())
                _connections.erase(conn_it);
        }
        auto meth_it = _methods.find(method);
        if (meth_it != _methods.end()) {
            meth_it->second.erase(host);
            if (conn_it->second.empty())
                _methods.erase(meth_it);
        }
        if (_methods.count(method) && _methods[method].count(host))
            return false;
        return true;
    }
    std::vector<std::pair<std::string, Address>> logoff(const BaseConnection::ptr& conn) {
        std::unique_lock<std::mutex> lock(_mutex);
        std::vector<std::pair<std::string, Address>> offlines;
        for (auto& pr : _connections[conn]) {
            auto& addrs = _methods[pr.first];
            addrs.erase(addrs.find(pr.second));
            if (addrs.count(pr.second) == 0)
                offlines.push_back(pr);
        }
        _connections.erase(conn);
        return offlines;
    }
    std::vector<Address> hosts(std::string& method) {
        std::unique_lock<std::mutex> lock(_mutex);
        std::vector<Address> hosts;
        if (_methods.count(method) == 0) {
            return hosts;
        }
        for (auto& host : _methods[method])
            hosts.push_back(host);
        std::sort(hosts.begin(), hosts.end());
        hosts.erase(std::unique(hosts.begin(), hosts.end()), hosts.end());
        return hosts;
    }

   private:
    std::mutex _mutex;
    std::unordered_map<std::string, std::unordered_multiset<Address>> _methods;
    std::unordered_map<BaseConnection::ptr, std::unordered_set<std::pair<std::string, Address>>> _connections;
};

class DiscovererManager {
   public:
    using ptr = std::shared_ptr<DiscovererManager>;
    void discover(const BaseConnection::ptr& conn, const std::string method) {
        std::unique_lock<std::mutex> lock(_mutex);
        _methods[method].insert(conn);
        _connections[conn].insert(method);
    }
    void registry(const std::string& method, const Address& host) {
        return notify(method, host, ServiceOptype::SERVICE_ONLINE);
    }
    void deregister(const std::string& method, const Address& host) {
        return notify(method, host, ServiceOptype::SERVICE_OFFLINE);
    }
    void logoff(const BaseConnection::ptr& conn) {
        std::unique_lock<std::mutex> lock(_mutex);
        for (auto& method : _connections[conn]) {
            _methods[method].erase(conn);
        }
        _connections.erase(conn);
    }

   private:
    void notify(const std::string& method, const Address& host, ServiceOptype optype) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _methods.find(method);
        if (it == _methods.end()) {
            // 这代表这个服务当前没有发现者
            DLOG("%s 没有发现者", method);
            return;
        }
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        msg_req->setOptype(optype);
        for (auto& conn : it->second) {
            conn->send(msg_req);
        }
        DLOG("向 %d 个请求者通知 %s %d", it->second.size(), method.c_str(), (int)optype);
    }
    std::mutex _mutex;
    std::unordered_map<std::string, std::unordered_set<BaseConnection::ptr>> _methods;
    std::unordered_map<BaseConnection::ptr, std::unordered_set<std::string>> _connections;
};

class Registry {
   public:
    using ptr = std::shared_ptr<Registry>;
    Registry()
        : _providers(std::make_shared<ProviderManager>()),
          _requesters(std::make_shared<DiscovererManager>()) {}
    void OnServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg) {
        auto optype = msg->optype();
        if (optype == ServiceOptype::SERVICE_REGISTRY) {
            auto method = msg->method();
            auto host = msg->host();
            ILOG("注册服务 %s host@%s:%d", method, host.first, host.second);
            if (_providers->registry(conn, method, host) == false) {
                ILOG("服务 %s host@%s:%d 重复注册", method, host.first, host.second);
				return registryResponse(conn, msg, RCode::RCODE_DUPLICATE_REGISTRY, ServiceOptype::SERVICE_REGISTRY);
            } else {
				_requesters->registry(method, host);
				return registryResponse(conn, msg, RCode::RCODE_OK, ServiceOptype::SERVICE_REGISTRY);
            }
        } else if (optype == ServiceOptype::SERVICE_DEREGISTER) {
			auto method = msg->method();
            auto host = msg->host();
            ILOG("注销服务 %s host@%s:%d", method, host.first, host.second);
            if (_providers->deregister(conn, method, host)) {
				_requesters->deregister(method, host);
            } else {
                ILOG("服务 %s host@%s:%d 仍有其他提供者注册", method, host.first, host.second);
            }
			return registryResponse(conn, msg, RCode::RCODE_OK, ServiceOptype::SERVICE_DEREGISTER);
        } else if (optype == ServiceOptype::SERVICE_DISCOVERY) {
            auto method = msg->method();
            _requesters->discover(conn, method);
            auto hosts = _providers->hosts(method);
            ILOG("客户端搜索服务 %s, 存在 %d 个提供者", method, hosts.size());
			return discoveryResponse(conn, msg, hosts);
        } else {
            ELOG("收到错误的 ServiceRequest 类型 %d", (int)optype);
			return errorResponse(conn, msg);
        }
    }
    void onConnShutdown(const BaseConnection::ptr& conn) {
		auto offLines = _providers->logoff(conn);
		ILOG("连接断开，%d个服务下线", offLines.size());
		for(auto &[method, host] : offLines) _requesters->deregister(method, host);
		_requesters->logoff(conn);
    }

   private:
    void errorResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
        msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
        conn->send(msg_rsp);
    }
    void registryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg, 
						  RCode rcode, ServiceOptype optype) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setRCode(rcode);
        msg_rsp->setOptype(optype);
        conn->send(msg_rsp);
    }
    void discoveryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg, const std::vector<Address> &hosts) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
        if (hosts.empty()) {
            msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
            return conn->send(msg_rsp);
        }
        msg_rsp->setRCode(RCode::RCODE_OK);
        msg_rsp->setMethod(msg->method());
        msg_rsp->setHost(hosts);
        return conn->send(msg_rsp);
    }

    ProviderManager::ptr _providers;
    DiscovererManager::ptr _requesters;
};

}  // namespace server
}  // namespace btrpc