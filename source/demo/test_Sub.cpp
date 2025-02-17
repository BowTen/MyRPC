#include "../server/rpc_server.hpp"

void Sub(const Json::Value& params, Json::Value &res){
	int num1 = params["num1"].asInt64();
	int num2 = params["num2"].asInt64();
	res = num1 - num2;
}

int main(int argc, char* argv[]){

	if(argc != 3){
		std::cout << "Usage: Sub [port] [max_connections]\n";
		return 0;
	}
	int port = atoi(argv[1]);
	int max_connections = atoi(argv[2]);
	auto server = std::make_shared<myrpc::server::RpcServer>(port, max_connections, 0);

	auto sdf = myrpc::server::SDescribeFactory();
	sdf.setMethodName("Sub");
	sdf.setParamsDesc("num1", myrpc::server::VType::INTEGRAL);
	sdf.setParamsDesc("num2", myrpc::server::VType::INTEGRAL);
	sdf.setReturnType(myrpc::server::VType::INTEGRAL);
	sdf.setCallback(Sub);
	auto sd = sdf.build();

	server->registerMethod(sd);
	server->start();

	return 0;
}