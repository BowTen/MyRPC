#include "../client/registry_discover.hpp"
#include "../client/rpc_client.hpp"


int main(int  argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: discoverer [rport]\n";
		return 0;
	}
	int rport = atoi(argv[1]);
	
	auto discoverer = std::make_shared<myrpc::client::Discoverer>("127.0.0.1", rport);

	myrpc::client::RpcClient::ptr rpc = nullptr;
	while(true){
		std::string method;
		method = "Add";
		int num1, num2;
		std::cout << "请输入两个整数：";
		std::cin >> num1 >> num2;

		Json::Value res, params;
		params["num1"] = num1;
		params["num2"] = num2;
		while(true){
			myrpc::Address dis_host;
			int ret = discoverer->discover(method, dis_host);
			if(ret){
				if(rpc == nullptr || rpc->getHost() != dis_host || !rpc->connected()){
					rpc = std::make_shared<myrpc::client::RpcClient>(dis_host.first, dis_host.second);
					ILOG("服务主机发生变化，新建连接 %s:%d", dis_host.first.c_str(), dis_host.second);
				}else{
					ILOG("继续使用 %s:%d", dis_host.first.c_str(), dis_host.second);
				}
				break;
			}else{
				ILOG("服务发现失败，等待重新发现");
				sleep(5);
			}
		}
		if(rpc == nullptr){
			ELOG("rpc client 为空");
			exit(0);
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