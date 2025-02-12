#include "../client/rpc_client.hpp"


int main(int argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: server [port]\n";
		return 0;
	}
	int port = atoi(argv[1]);
	auto client = std::make_shared<btrpc::client::RpcClient>("127.0.0.1", port);

	while(1){
		std::string method;
		int num1, num2;
		std::cout << "请输入方法名：";
		std::cin >> method;
		std::cout << "请输入两个整数：";
		std::cin >> num1 >> num2;

		Json::Value res, params;
		params["num1"] = num1;
		params["num2"] = num2;
		client->call(method, params, res);

		std::cout << "result:\n" << btrpc::JSON::serialize(res) << '\n';
	}


	return 0;
}