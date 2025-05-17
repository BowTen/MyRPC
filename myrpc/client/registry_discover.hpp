#pragma once
#include <unordered_set>
#include "client.hpp"

namespace myrpc {
namespace client {

// 服务注册、注销
class Provider {
   public:
    using ptr = std::shared_ptr<Provider>;
    Provider(const std::string& host, int port)
        : _client(std::make_shared<Client>(host, port)) {}
		
    bool registryMethod(const std::string& method, const Address& host) {
        auto msg_req = MessageFactory::create<ServiceRequest>();
        //msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        msg_req->setOptype(ServiceOptype::SERVICE_REGISTRY);
        BaseMessage::ptr msg_rsp;
        bool ret = _client->send(msg_req, msg_rsp);
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
		ILOG("服务 %s 注册成功", method.c_str());
        return true;
    }
    bool deregisterMethod(const std::string& method, const Address& host) {
        auto msg_req = MessageFactory::create<ServiceRequest>();
        //msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        msg_req->setOptype(ServiceOptype::SERVICE_DEREGISTER);
        BaseMessage::ptr msg_rsp;
        bool ret = _client->send(msg_req, msg_rsp);
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
		ILOG("服务 %s 注销成功", method.c_str());
        return true;
    }

   private:
	Client::ptr _client;
};

// 服务发现，处理服务更新请求
class Discoverer {
	public:
	using ptr = std::shared_ptr<Discoverer>;
	using onServiceFirstDiscover = std::function<void(const std::string&, const Address&)>;
	using onServiceUpdate = std::function<void(const std::string&, const Address&)>;
	using onServiceLapse = std::function<void(const std::string&)>;
	Discoverer(const std::string& host, int port)
		: _client(std::make_shared<Client>(host, port)) {
			auto svc_req_cb = std::bind(&Discoverer::onServiceUpdateRequest, this, 
				std::placeholders::_1, std::placeholders::_2);
			_client->registerHandler<ServiceRequest>(MType::REQ_SERVICE, svc_req_cb);
		}

	bool discover(const std::string& method, Address& host){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_method_host.count(method)){
			host = _method_host[method];
			return true;
		}
		if(_visted.count(method)){
			return false;
		}
		lock.unlock();
		auto ret = discoverService(method);
		lock.lock();
		if(ret){
			host = _method_host[method];
			return true;
		}else{
			return false;
		}
	}

	void setOnServiceFirstDiscover(const onServiceFirstDiscover& cb){
		_service_first_discover_cb = cb;
	}

	void setOnServiceUpdate(const onServiceUpdate& cb){
		_service_update_cb = cb;
	}

	void setOnServiceLapse(const onServiceLapse& cb){
		_service_lapse_cb = cb;
	}


	private:
	void onServiceUpdateRequest(const BaseConnection::ptr& conn, ServiceRequest::ptr& msg){
		auto optype = msg->optype();
		if(optype == ServiceOptype::SERVICE_UPDATE){
			auto method = msg->method();
			auto host = msg->host();
			ILOG("服务 %s 更新：%s:%d", method.c_str(), host.first.c_str(), host.second);
			std::unique_lock<std::mutex> lock(_mutex);
			_method_host[method] = host;
			if(_service_update_cb){
				_service_update_cb(method, host);
			}
		}else if(optype == ServiceOptype::SERVICE_OFFLINE){
			auto method = msg->method();
			ILOG("服务 %s 下线", method.c_str());
			std::unique_lock<std::mutex> lock(_mutex);
			_method_host.erase(method);
			if(_service_lapse_cb){
				_service_lapse_cb(method);
			}
		}else{
			ELOG("收到 %d 号请求，忽略", static_cast<int>(optype));
		}
	}

	bool discoverService(const std::string& method){
		auto msg_req = MessageFactory::create<ServiceRequest>();
		//msg_req->setId(UUID::uuid());
		msg_req->setMType(MType::REQ_SERVICE);
		msg_req->setMethod(method);
		msg_req->setOptype(ServiceOptype::SERVICE_DISCOVERY);
		BaseMessage::ptr msg_rsp;
		bool ret = _client->send(msg_req, msg_rsp);
		if(ret == false){
			ELOG("%s 服务发现请求发送失败！", method.c_str());
			return false;
		}
		auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
		if(service_rsp.get() == nullptr){
			ELOG("响应类型向下转换失败！");
			return false;
		}
		if(service_rsp->rcode() != RCode::RCODE_OK){
			ELOG("服务发现失败，原因：%s", errReason(service_rsp->rcode()).c_str());
			if(service_rsp->rcode() == RCode::RCODE_NOT_FOUND_SERVICE){
				std::unique_lock<std::mutex> lock(_mutex);
				_visted.insert(method);
			}
			return false;
		}
		std::unique_lock<std::mutex> lock(_mutex);
		_visted.insert(method);
		_method_host[method] = service_rsp->host();
		if(_service_first_discover_cb){
			_service_first_discover_cb(method, service_rsp->host());
		}
		return true;
	}

	std::mutex _mutex;
	Client::ptr _client;
	std::unordered_map<std::string, Address> _method_host;
	std::unordered_set<std::string> _visted;
	onServiceFirstDiscover _service_first_discover_cb;
	onServiceUpdate _service_update_cb;
	onServiceLapse _service_lapse_cb;
};

}  // namespace client
}  // namespace myrpc
