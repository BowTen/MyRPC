#include "../client/registry_discover.hpp"
#include "../client/rpc_client.hpp"
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