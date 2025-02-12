#include "../server/rpc_server.hpp"

void Add(const Json::Value& params, Json::Value &res){
	int num1 = params["num1"].asInt64();
	int num2 = params["num2"].asInt64();
	res = num1 + num2;
}

int main(int argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: server [port]\n";
		return 0;
	}
	int port = atoi(argv[1]);
	auto server = std::make_shared<btrpc::server::RpcServer>(port, 2, 0);

	auto sdf = btrpc::server::SDescribeFactory();
	sdf.setMethodName("Add");
	sdf.setParamsDesc("num1", btrpc::server::VType::INTEGRAL);
	sdf.setParamsDesc("num2", btrpc::server::VType::INTEGRAL);
	sdf.setReturnType(btrpc::server::VType::INTEGRAL);
	sdf.setCallback(Add);
	auto sd = sdf.build();

	server->registerMethod(sd);
	server->start();

	return 0;
}