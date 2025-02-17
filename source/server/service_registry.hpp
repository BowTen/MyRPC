#pragma once

#include <chrono>
#include <ctime>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include "../client/client.hpp"
#include "server.hpp"
#include "../common/detail.hpp"
#include "../common/fields.hpp"

namespace myrpc {
namespace server {
	

class ServiceManager {
   public:
   	using ServiceAppearCallback = std::function<void(const std::string&)>;
	using ServiceLapseCallback = std::function<void(const std::string&, const Address&)>;
    using ptr = std::shared_ptr<ServiceManager>;
    using tim_cli = std::pair<std::chrono::system_clock::time_point, client::Client::ptr>;

    ServiceManager() {
		tim_cli keepper = std::make_pair(std::chrono::system_clock::now(), nullptr);
		_que.push(keepper);
		std::thread detecter([this](){
			this->loop();
		});
		detecter.detach();
    }

    RCode registry(const std::string& method, const Address& host) {
		DLOG("注册方法 %s %s:%d", method.c_str(), host.first.c_str(), host.second);
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 判断是否重复注册
            if (_host_info.count(host) && _host_info[host]->_methods.count(method)) {
                ELOG("方法 %s@%s:%d 重复注册", method.c_str(), host.first.c_str(), host.second);
                return RCode::RCODE_DUPLICATE_REGISTRY;
            }
        }
		if(add(method, host)){
			DLOG("添加服务 %s:%d 成功", host.first.c_str(), host.second);
			return RCode::RCODE_OK;
		}else{
			ELOG("添加服务 %s:%d 失败", host.first.c_str(), host.second);
			return RCode::RCODE_INTERNAL_ERROR;
		}
    }

    RCode deregister(const std::string& method, const Address& host) {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 判断是否被注册
            if (_host_info.count(host) == 0 || _host_info[host]->_methods.count(method) == 0) {
                ELOG("方法 %s@%s:%d 没有注册", method.c_str(), host.first.c_str(), host.second);
                return RCode::RCODE_NOT_FOUND_SERVICE;
            }
        }
		MethodHost mhost;
		if(!popMethodHost(method, host, mhost)){
			return RCode::RCODE_INTERNAL_ERROR;
		}
		{
            std::unique_lock<std::mutex> lock(_mutex);
			_host_info[host]->_methods.erase(method);
			if(_host_info[host]->_methods.empty()){
				_host_info[host]->_client->shutdown();
				_host_info.erase(host);
			}
		}
		if(_service_lapse_cb){
			std::thread(_service_lapse_cb, method, host).detach();
		}
		return RCode::RCODE_OK;
    }

	bool discover(const std::string& method, Address& host){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_method_hosts.count(method) == 0 || _method_hosts[method].empty() || _method_hosts[method].rbegin()->first <= 0){
			ELOG("服务 %s 没有找到可用主机", method.c_str());
			return false;
		}
		MethodHost mhost;
		pt_set(method, _method_hosts[method]);
		lock.unlock();
		auto ret = choseMethodHost(method, mhost);
		lock.lock();
		if(ret){
			host = mhost.second;
			ILOG("收到发现请求 %s 分配主机 %s:%d", method.c_str(), host.first.c_str(), host.second);
			pt_set(method, _method_hosts[method]);
			return true;
		}
		return false;
	}

	void setServiceAppearCallback(const ServiceAppearCallback& cb){
		std::unique_lock<std::mutex> lock(_mutex);
		_service_appear_cb = cb;
	}

	void setServiceLapseCallback(const ServiceLapseCallback& cb){
		std::unique_lock<std::mutex> lock(_mutex);
		_service_lapse_cb = cb;
	}

	void setHeartbeatSec(int sec){
		HEARTBEAT_SEC = sec;
	}

	void wait(const std::string& method){
		std::unique_lock<std::mutex> lock(_mutex);
		_wait.insert(method);
	}

	void stopWait(const std::string& method){
		std::unique_lock<std::mutex> lock(_mutex);
		_wait.erase(method);
	}

   private:
	using MethodHost = std::pair<int, Address>;
    struct HostInfo {
        using ptr = std::shared_ptr<HostInfo>;

        int _idle;
        client::Client::ptr _client;
        std::unordered_set<std::string> _methods;
    };

	void onClose(const BaseConnection::ptr& conn){
		remove(conn->getHost());
		if(_close_cb) _close_cb(conn);
	}

	void setCloseCallback(const CloseCallback& cb){
		std::unique_lock<std::mutex> lock(_mutex);
		_close_cb = cb;
	}

	void pt_que(std::queue<tim_cli> que){
		DLOG("打印队列：大小：%d", (int)que.size());
		while(!que.empty()){
			if(que.front().second){
				DLOG("时间：%d  主机：%s:%d", (int)std::chrono::duration_cast<std::chrono::seconds>(que.front().first.time_since_epoch()).count(), que.front().second->connection()->getHost().first.c_str(), que.front().second->connection()->getHost().second);
			}else{
				DLOG("时间：%d  keepper", (int)std::chrono::duration_cast<std::chrono::seconds>(que.front().first.time_since_epoch()).count());
			}
			que.pop();
		}
	}

	void loop(){
		while(true){
			_mutex.lock();
			if(_que.empty()){
				ELOG("队列空");
				exit(0);
			}
			auto tp = _que.front();
			if(std::chrono::system_clock::now() >= tp.first){
				DLOG("准备心跳检测");
				//DLOG("优先队列大小：%d", (int)_que.size());
				//pt_que(_que);
				_que.pop();
				// 检查是否遇到keepper
				if(tp.second == nullptr){
					tp.first = std::chrono::system_clock::now() + std::chrono::seconds(HEARTBEAT_SEC);
					_que.push(tp);
					DLOG("遇到keepper,放回队列");
				}
				// 检查连接是否断开
				else if(tp.second->connected()){
					int idle = -INF;
					_mutex.unlock();
					auto ret = detect(tp.second, idle);
					_mutex.lock();
					if(ret){
						ILOG("心跳探测，主机 %s:%d 空闲量：%d", tp.second->connection()->getHost().first.c_str(), tp.second->connection()->getHost().second, idle);
						_mutex.unlock();
						update(tp.second->connection()->getHost(), idle);
						_mutex.lock();
						tp.first = std::chrono::system_clock::now() + std::chrono::seconds(HEARTBEAT_SEC);
						_que.push(tp);
					}else{
						ELOG("心跳探测失败，删除主机 %s:%d", tp.second->connection()->getHost().first.c_str(), tp.second->connection()->getHost().second);
						_mutex.unlock();
						remove(tp.second->connection()->getHost());
						_mutex.lock();
					}
				}else{
					//连接断开
					ILOG("心跳探测：连接断开");
				}
			}
			if(_que.empty()){
				ELOG("休眠前优先队列空");
				exit(0);
			}
			// 休眠到下一次心跳检测
			auto cur = std::chrono::system_clock::now();
			if(_que.front().first > cur){
				auto d = std::chrono::duration_cast<std::chrono::milliseconds>(_que.front().first - cur);
				_mutex.unlock();
				std::this_thread::sleep_for(d);
				_mutex.lock();
			}
			if(_que.empty()){
				ELOG("休眠后优先队列空");
				exit(0);
			}
			_mutex.unlock();
		}
	}

    static int HEARTBEAT_SEC;
    bool detect(const client::Client::ptr& cli, int& idle) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto req = std::make_shared<ServiceRequest>();
        req->setOptype(ServiceOptype::SERVICE_DETECT);
		BaseMessage::ptr bas_rsp;
        cli->send(std::dynamic_pointer_cast<BaseMessage>(req), bas_rsp);
		auto rsp = std::dynamic_pointer_cast<ServiceResponse>(bas_rsp);
        if (rsp->rcode() == RCode::RCODE_OK) {
			DLOG("探测成功,idle=%d, rid=%s", rsp->idleCount(), bas_rsp->rid().c_str());
            idle = rsp->idleCount();
            return true;
        }
        ELOG("心跳检测失败，原因：%s", errReason(rsp->rcode()).c_str());
        idle = -INF;
        return false;
    }

	bool add(const std::string& method, const Address& host) {
        std::unique_lock<std::mutex> lock(_mutex);
		if(_host_info.count(host)){
			DLOG("已有 %s:%d 主机连接", host.first.c_str(), host.second);
			int idle = _host_info[host]->_idle;
			_method_hosts[method].insert(std::make_pair(idle, host));
			return true;
		}else{
			DLOG("新建 %s:%d 主机连接", host.first.c_str(), host.second);
			auto cli = createClient(host);
			int idle = -INF;
			lock.unlock();
			auto ret = detect(cli, idle);
			lock.lock();
			if(ret){
				DLOG("首次心跳检测成功");
				_method_hosts[method].insert(std::make_pair(idle, host));
				auto info = std::make_shared<HostInfo>();
				info->_idle = idle;
				info->_client = cli;
				info->_methods.insert(method);
				_host_info[host] = info;
				_que.push(std::make_pair(std::chrono::system_clock::now() + std::chrono::seconds(HEARTBEAT_SEC), cli));
				if(_service_appear_cb) std::thread(_service_appear_cb, method).detach();
				return true;
			}else{
				DLOG("首次心跳检测失败");
				return false;
			}
		}
	}

    client::Client::ptr createClient(const Address& host) {
		DLOG("主机 %s:%d 新建连接", host.first.c_str(), host.second);
		auto cli = std::make_shared<client::Client>(host.first, host.second);
		cli->setCloseCallback(std::bind(&ServiceManager::onClose, this, std::placeholders::_1));
		return cli;
	}

    client::Client::ptr getClient(const Address& host) {
        std::unique_lock<std::mutex> lock(_mutex);
		DLOG("获取主机连接 %s:%d", host.first.c_str(), host.second);
        if (_host_info.count(host)){
			DLOG("主机 %s:%d 已经连接", host.first.c_str(), host.second);
			return _host_info[host]->_client;
		}else{
			DLOG("主机 %s:%d 没有连接", host.first.c_str(), host.second);
            return createClient(host);
		}
    }

	void pt_set(const std::string& method, std::set<MethodHost> set){
		DLOG("打印方法%s的set：大小：%d", method.c_str(), (int)set.size());
		for(auto &mhost : set){
			DLOG("idle=%d, host=%s:%d", mhost.first, mhost.second.first.c_str(), mhost.second.second);
		}
	}

	bool choseMethodHost(const std::string& method, std::pair<int, Address>& mhost){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_method_hosts.count(method) == 0 || _method_hosts[method].empty() || _method_hosts[method].rbegin()->first <= 0){
			ELOG("服务 %s 没有找到可用主机", method.c_str());
			return false;
		}
		mhost = *_method_hosts[method].rbegin();
		_method_hosts[method].erase(mhost);
		mhost.first--;
		_host_info[mhost.second]->_idle--;
		lock.unlock();
		pushMethodHost(method, mhost);
		lock.lock();
		return true;
	}

    bool popMethodHost(const std::string& method, const Address& host, std::pair<int, Address>& mhost) {
        std::unique_lock<std::mutex> lock(_mutex);
		auto idle = _host_info[host]->_idle;
		auto it = _method_hosts[method].find(std::make_pair(idle, host));
		if(it == _method_hosts[method].end()){
			ELOG("popMethodHost 没有找到服务 %s:%d !!!!!!!!!!!!!!", host.first.c_str(), host.second);
			return false;
		}
		mhost = *it;
		_method_hosts[method].erase(it);
		return true;
    }

	bool pushMethodHost(const std::string& method, std::pair<int, Address>& mhost){
        std::unique_lock<std::mutex> lock(_mutex);
		_method_hosts[method].insert(mhost);
		return true;
	}

	void update(const Address& host, int idle){
		std::unique_lock<std::mutex> lock(_mutex);
		auto &info = _host_info[host];
		for(auto &method : info->_methods){
			MethodHost mhost;
			lock.unlock();
			popMethodHost(method, host, mhost);
			mhost.first = idle;
			pushMethodHost(method, mhost);
			lock.lock();
			if(idle > 0 && _service_appear_cb && _wait.count(method)){
				std::thread(_service_appear_cb, method).detach();
			}
		}
		info->_idle = idle;
	}

	void remove(const Address& host){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_host_info.count(host) == 0){
			return;
		}
		for(auto &method : _host_info[host]->_methods){
			MethodHost mhost;
			lock.unlock();
			popMethodHost(method, host, mhost);
			lock.lock();
			if(_service_lapse_cb) std::thread(_service_lapse_cb, method, host).detach();
		}
		_host_info.erase(host);
	}

	std::mutex _mutex;
    std::unordered_set<std::string> _wait;
    std::unordered_map<std::string, std::set<MethodHost>> _method_hosts;
    std::unordered_map<Address, HostInfo::ptr, AddrHash> _host_info;
    std::queue<tim_cli> _que;
	CloseCallback _close_cb;
	ServiceAppearCallback _service_appear_cb;
	ServiceLapseCallback _service_lapse_cb;
};

class DiscovererManager{
	public:
	using ptr = std::shared_ptr<DiscovererManager>;

	void gotoWait(const std::string& method, const BaseConnection::ptr& conn){
		std::unique_lock<std::mutex> lock(_mutex);
		_wait_que[method].push(conn);
		_inque[method].insert(conn->getHost());
	}
	
	void gotoUse(const std::string& method, const Address& host, const BaseConnection::ptr& conn){
		std::unique_lock<std::mutex> lock(_mutex);
		_use_que[method][host].push(conn);
		_inque[method].insert(conn->getHost());
	}

	BaseConnection::ptr outOfWait(const std::string& method){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_wait_que.count(method)){
			while(_wait_que[method].size() && !_wait_que[method].front()->connected()){
				_inque[method].erase(_wait_que[method].front()->getHost());
				_wait_que[method].pop();
			}
		}
		if(_wait_que.count(method) == 0 || _wait_que[method].empty()){
			return nullptr;
		}
		auto conn = _wait_que[method].front();
		_wait_que[method].pop();
		return conn;
	}
	
	BaseConnection::ptr outOfUse(const std::string& method, const Address& host){
		std::unique_lock<std::mutex> lock(_mutex);
		if(_use_que.count(method) && _use_que[method].count(host)){
			while(_use_que[method][host].size() && !_use_que[method][host].front()->connected()){
				_inque[method].erase(_use_que[method][host].front()->getHost());
				_use_que[method][host].pop();
			}
		}
		if(_use_que.count(method) == 0 || _use_que[method].count(host) == 0 || _use_que[method][host].empty()){
			return nullptr;
		}
		auto conn = _use_que[method][host].front();
		_use_que[method][host].pop();
		return conn;
	}

	bool wait(const std::string& method){
		std::unique_lock<std::mutex> lock(_mutex);
		return _wait_que.count(method) > 0 && !_wait_que[method].empty();
	}

	bool inque(const std::string& method, const Address& host){
		std::unique_lock<std::mutex> lock(_mutex);
		return _inque.count(method) > 0 && _inque[method].count(host) > 0;
	}


	private:
	std::mutex _mutex;
	std::unordered_map<std::string, std::unordered_set<Address, AddrHash>> _inque;
	std::unordered_map<std::string, std::queue<BaseConnection::ptr>> _wait_que;
	std::unordered_map<std::string, std::unordered_map<Address, std::queue<BaseConnection::ptr>, AddrHash>> _use_que;
};


//服务注册、注销 ok
//服务发现 ok
//服务状态变更：可用服务出现提醒(新服务上线、主机空闲)，服务失效提醒(服务注销、主机下线)(服务失效时优先为寻找其他可用主机替换)
//                             ServiceManager回调 ok           ServiceManager回调 ok

class ServiceRegistry {
   public:
    using ptr = std::shared_ptr<ServiceRegistry>;
    ServiceRegistry(int port)
        : _server(std::make_shared<Server>(port)),
		  _service_manager(std::make_shared<ServiceManager>()),
		  _discoverer_manager(std::make_shared<DiscovererManager>()) {
        auto svc_req_cb = std::bind(&ServiceRegistry::onServiceCallback, this,
                                    std::placeholders::_1, std::placeholders::_2);
        _server->registerHandler<ServiceRequest>(MType::REQ_SERVICE, svc_req_cb);

		_service_manager->setServiceAppearCallback(std::bind(&ServiceRegistry::onServiceAppear, this, std::placeholders::_1));
		_service_manager->setServiceLapseCallback(std::bind(&ServiceRegistry::onServiceLapse, this, std::placeholders::_1, std::placeholders::_2));
    }

	void start(){
		_server->start();
	}

	void setHeartbeatSec(int sec){
		_service_manager->setHeartbeatSec(sec);
	}

   private:
    void onServiceCallback(const BaseConnection::ptr& conn, ServiceRequest::ptr& msg) {
        auto optype = msg->optype();
        if (optype == ServiceOptype::SERVICE_REGISTRY) {
            DLOG("收到 服务注册 请求");
			return responseRCode(msg->rid(), conn, _service_manager->registry(msg->method(), msg->host()));
		} else if (optype == ServiceOptype::SERVICE_DEREGISTER) {
            DLOG("收到 服务注销 请求");
			return responseRCode(msg->rid(), conn, _service_manager->deregister(msg->method(), msg->host()));
		} else if (optype == ServiceOptype::SERVICE_DISCOVERY) {
            DLOG("收到 服务发现 请求");
			if(_discoverer_manager->inque(msg->method(), conn->getHost())){
				return responseRCode(msg->rid(), conn, RCode::RCODE_NOT_FOUND_SERVICE);
			}
			Address host;
			if(_service_manager->discover(msg->method(), host)){
				_discoverer_manager->gotoUse(msg->method(), host, conn);
				return responseHost(msg->rid(), conn, msg->method(), host);
			}else{
				_discoverer_manager->gotoWait(msg->method(), conn);
				_service_manager->wait(msg->method());
				return responseRCode(msg->rid(), conn, RCode::RCODE_NOT_FOUND_SERVICE);
			}
		} else {
            ELOG("收到 %d 号请求，忽略", static_cast<int>(optype));
			return responseRCode(msg->rid(), conn, RCode::RCODE_INVALID_OPTYPE);
		}
    }

	void responseRCode(std::string rid, const BaseConnection::ptr& conn, RCode code){
		auto rsp = std::make_shared<ServiceResponse>();
		rsp->setId(rid);
		rsp->setRCode(code);
		rsp->setOptype(ServiceOptype::SERVICE_RETURN);
		conn->send(std::dynamic_pointer_cast<BaseMessage>(rsp));
	}

	void responseHost(std::string rid, const BaseConnection::ptr& conn, const std::string& method, const Address& host){
		auto rsp = std::make_shared<ServiceResponse>();
		rsp->setId(rid);
		rsp->setRCode(RCode::RCODE_OK);
		rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
		rsp->setHost(host);
		rsp->setMethod(method);
		conn->send(std::dynamic_pointer_cast<BaseMessage>(rsp));
	}

	void requestUpdate(const BaseConnection::ptr& conn, const std::string& method, const Address& host, ServiceOptype optype){
		auto req = std::make_shared<ServiceRequest>();
		req->setOptype(optype);
		req->setMethod(method);
		req->setHost(host);
		conn->send(std::dynamic_pointer_cast<BaseMessage>(req));
	}

	void onServiceAppear(const std::string& method){
		DLOG("服务 %s 可用", method.c_str());
		while(true){
			auto conn = _discoverer_manager->outOfWait(method);
			if(conn == nullptr){
				_service_manager->stopWait(method);
				DLOG("等待服务 %s 的连接已经全部处理", method.c_str());
				break;
			}
			Address host;
			if(_service_manager->discover(method, host)){
				_discoverer_manager->gotoUse(method, host, conn);
				requestUpdate(conn, method, host, ServiceOptype::SERVICE_UPDATE);
				ILOG("等待服务 %s 的discoverer %s:%d 获得主机更新，新主机 %s:%d", method.c_str(), conn->getHost().first.c_str(), conn->getHost().second, host.first.c_str(), host.second);
			}else{
				_discoverer_manager->gotoWait(method, conn);
				ILOG("服务 %s 空闲量已耗尽，等待新服务上线", method.c_str());
				break;
			}
		}
	}

	void onServiceLapse(const std::string& method, const Address& host){
		bool idle = true;
		while(true){
			auto conn = _discoverer_manager->outOfUse(method, host);
			if(conn == nullptr){
				DLOG("使用服务 %s:%d 的discoverer已经全部通知", host.first.c_str(), host.second);
				break;
			}
			if(idle){
				Address new_host;
				if(_service_manager->discover(method, new_host)){
					_discoverer_manager->gotoUse(method, new_host, conn);
					requestUpdate(conn, method, new_host, ServiceOptype::SERVICE_UPDATE);
					ILOG("discoverer %s:%d 使用 %s 服务的原主机 %s:%d 失效，新主机 %s:%d", conn->getHost().first.c_str(), conn->getHost().second, method.c_str(), host.first.c_str(), host.second, new_host.first.c_str(), new_host.second);
				}else{
					idle = false;
				}
			}
			if(!idle){
				ILOG("discoverer %s:%d 使用 %s 服务的原主机 %s:%d 失效，等待新服务上线", conn->getHost().first.c_str(), conn->getHost().second, method.c_str(), host.first.c_str(), host.second);
				requestUpdate(conn, method, host, ServiceOptype::SERVICE_OFFLINE);
				_discoverer_manager->gotoWait(method, conn);
			}
		}
		if(_discoverer_manager->wait(method)){
			_service_manager->wait(method);
		}else{
			_service_manager->stopWait(method);
		}
	}

    std::shared_ptr<Server> _server;
	ServiceManager::ptr _service_manager;
	DiscovererManager::ptr _discoverer_manager;
};

int ServiceManager::HEARTBEAT_SEC = 30;

}  // namespace server
}  // namespace myrpc