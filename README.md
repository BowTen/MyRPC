# MyRPC ✨

一个基于 **Muduo 库**的高性能 RPC 框架，支持高并发、多路复用、动态服务注册/发现，并内置智能负载均衡算法。让分布式服务调用更简单！🙌

------

## 🌟 核心特性

- ✅ **高并发通信**：基于 Muduo 库封装 LV 协议，提供 `myrpc::server::Server` 和 `myrpc::client::Client` 类。
- ✅ **RPC 核心模块**：实现服务端 `myrpc::server::RpcServer` 与客户端 `myrpc::client::RpcClient`。
- ✅ **服务治理**：支持动态注册/注销、服务发现、负载均衡，提供 `ServiceRegistry`、`Provider`、`Discoverer` 类。
- ✅ **智能负载均衡**：基于红黑树维护最大空闲主机，心跳检测保活，队列化请求调度。

------

## ⚙️ 负载均衡算法详解

### 📊 最大空闲主机分配

1. **心跳检测 + 红黑树**
   - 服务端注册时声明 `max_connections`（最大连接数），注册中心通过心跳检测获取实时空闲值 `idle`。
   - 使用红黑树按 `idle` 排序主机，服务发现时优先分配 `idle` 最大的主机，并动态更新 `idle`。
   - 心跳间隔：`HEARTBEAT_SEC` 秒，首次注册立即检测。
2. **队列化请求调度**
   - 发现者按可用性进入 `_use_que`（使用队列）或 `_wait_que`（等待队列）。
   - 新主机上线时，优先分配等待队列中的请求；主机下线时，动态通知请求者切换或等待。

![负载均衡架构图](https://gitee.com/BowTen/img-bed/raw/master/images/202502180102146.png)

------

## 🚀 快速开始

### 📡 服务实现端（示例：实现加法服务）

```cpp
#include "server/rpc_server.hpp"

void Add(const Json::Value& params, Json::Value &res){
	int num1 = params["num1"].asInt64();
	int num2 = params["num2"].asInt64();
	res = num1 + num2;
}

int main(int argc, char* argv[]){

	if(argc != 3){
		std::cout << "Usage: Add [port] [max_connections]\n";
		return 0;
	}
	int port = atoi(argv[1]);
	int max_connections = atoi(argv[2]);
	auto server = std::make_shared<myrpc::server::RpcServer>(port, max_connections, 0);

	//构造方法描述
	auto sdf = myrpc::server::SDescribeFactory();
	sdf.setMethodName("Add");
	sdf.setParamsDesc("num1", myrpc::server::VType::INTEGRAL);
	sdf.setParamsDesc("num2", myrpc::server::VType::INTEGRAL);
	sdf.setReturnType(myrpc::server::VType::INTEGRAL);
	sdf.setCallback(Add);
	auto sd = sdf.build();

	//注册方法
	server->registerMethod(sd);
	server->start();

	return 0;
}
```

### 📡 服务调用端

```cpp
#include "client/rpc_client.hpp"

int main(int argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: client [ip] [port]\n";
		return 0;
	}
	std::string ip(argv[1]);
	int port = atoi(argv[2]);
	auto client = std::make_shared<btrpc::client::RpcClient>(ip, port);

	std::string method;
	int num1, num2;
	method = "Add";
	std::cout << "请输入两个整数：";
	std::cin >> num1 >> num2;

	Json::Value res, params;
	params["num1"] = num1;
	params["num2"] = num2;
	client->call(method, params, res);
	
	std::cout << "result:\n" << btrpc::JSON::serialize(res) << '\n';

	return 0;
}
```

------

### 服务注册中心

```cpp
#include "server/service_registry.hpp"

int main(int argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: registry [port]\n";
		return 0;
	}
	int port = atoi(argv[1]);

	auto registry = std::make_shared<myrpc::server::ServiceRegistry>(port);
	registry->setHeartbeatSec(30);
	registry->start();

	return 0;
}
```



### 服务提供者（注册者）

```cpp
#include "client/registry_discover.hpp"

int main(int argc, char* argv[]){

	if(argc != 7){
		std::cout << "Usage: provider [registry/deregister] [method] [sip] [sport] [rip] [rport]\n";
		return 0;
	}
	std::string op(argv[1]);
	std::string method(argv[2]);
	std::string sip(argv[3]);
	int sport = atoi(argv[4]);
	std::string rip(argv[5]);
	int rport = atoi(argv[6]);

	auto pr = std::make_shared<myrpc::client::Provider>(rip, rport);
	auto host = std::make_pair(sip, sport);
	if(op == "registry") pr->registryMethod(method, host);
	else if(op == "deregister") pr->deregisterMethod(method, host);
	else{
		std::cout << "Usage: provider [registry/deregister] [method] [sport] [rport]\n";
		return 0;
	}

	sleep(1);

	return 0;
}
```



### 🔍 服务发现+服务调用

```cpp
#include "client/registry_discover.hpp"
#include "client/rpc_client.hpp"
#include<bits/stdc++.h>

std::unordered_map<std::string, myrpc::client::RpcClient::ptr>clis;

void onServiceUpdate(const std::string& method, const myrpc::Address& host){
	clis[method] = std::make_shared<myrpc::client::RpcClient>(host.first, host.second);
}

void onServiceLapse(const std::string& method){
	clis.erase(method);
}


int main(int  argc, char* argv[]){

	if(argc != 3){
		std::cout << "Usage: discoverer_cb [rip] [rport]\n";
		return 0;
	}
	std::string rip(argv[1]);
	int rport = atoi(argv[2]);
	
	auto discoverer = std::make_shared<myrpc::client::Discoverer>(rip, rport);
	discoverer->setOnServiceFirstDiscover(onServiceUpdate);
	discoverer->setOnServiceUpdate(onServiceUpdate);
	discoverer->setOnServiceLapse(onServiceLapse);

	myrpc::Address host;
	//调用 Add 或 Sub
	discoverer->discover(std::string("Add"), host);
	discoverer->discover(std::string("Sub"), host);

	while(true){
		std::string method;
		std::cout << "请输入方法名：";
		std::cin >> method;
		auto &rpc = clis[method];
		int num1, num2;
		std::cout << "请输入两个整数：";
		std::cin >> num1 >> num2;

		Json::Value res, params;
		params["num1"] = num1;
		params["num2"] = num2;
		while(true){
			if(rpc != nullptr && rpc->connected()){
				break;
			}
			ILOG("暂无可用主机，等待服务发现...");
			sleep(5);
		}
		auto ret = rpc->call(method, params, res);
		if(ret){
			ILOG("调用成功，result:\n%s", myrpc::JSON::serialize(res).c_str());
		}else{	
			ELOG("调用失败");
		}
	}
	
	return 0;
}
```

------

## 📌 依赖项

- Muduo 网络库
- JsonCpp
