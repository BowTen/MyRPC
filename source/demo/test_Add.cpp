#include "../server/rpc_server.hpp"

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


//#include "../client/rpc_client.hpp"


//int main(int argc, char* argv[]){

//	if(argc != 2){
//		std::cout << "Usage: client [ip] [port]\n";
//		return 0;
//	}
//	std::string ip(argv[1]);
//	int port = atoi(argv[2]);
//	auto client = std::make_shared<btrpc::client::RpcClient>(ip, port);

//	std::string method;
//	int num1, num2;
//	method = "Add";
//	std::cout << "请输入两个整数：";
//	std::cin >> num1 >> num2;

//	Json::Value res, params;
//	params["num1"] = num1;
//	params["num2"] = num2;
//	client->call(method, params, res);
	
//	std::cout << "result:\n" << btrpc::JSON::serialize(res) << '\n';

//	return 0;
//}