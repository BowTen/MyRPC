#include "net.hpp"
#include "dispatcher.hpp"
#include "../client/requestor.hpp"
#include "../client/rpc_caller.hpp"


void rpc_rsp(const btrpc::BaseConnection::ptr& conn, btrpc::RpcResponse::ptr& msg){
	std::cout << "收到rpc响应\n";
	auto jsonstr = msg->serialize();
	std::cout << "response body:\n" << jsonstr << '\n';
	
	auto res = msg->result();
	btrpc::JSON::serialize(res, jsonstr);
	std::cout << "result:\n" << jsonstr << '\n';
}
void tpc_rsp(const btrpc::BaseConnection::ptr& conn, btrpc::TopicResponse::ptr& msg){
	std::cout << "收到topic响应\n";
	auto jsonstr = msg->serialize();
	std::cout << "response body:\n" << jsonstr << '\n';

	auto rc = msg->rcode();
	std::cout << "rcode:\n" << static_cast<int>(rc) << '\n';
}

int main(int  argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: server [port]\n";
		return 0;
	}
	int port = atoi(argv[1]);

	auto requestor = std::make_shared<btrpc::client::Requestor>();
	auto caller = std::make_shared<btrpc::client::RpcCaller>(requestor);

	auto requestor_cb = std::bind(&btrpc::client::Requestor::onResponse, requestor.get(),
								  std::placeholders::_1, std::placeholders::_2);

	auto dsp = std::make_shared<btrpc::Dispatcher>();
	dsp->registerHandler<btrpc::BaseMessage>(btrpc::MType::RSP_RPC, requestor_cb);
	dsp->registerHandler<btrpc::TopicResponse>(btrpc::MType::RSP_TOPIC, tpc_rsp);

	auto client = btrpc::ClientFactory::create("127.0.0.1", port);
	client->setMessageCallback(std::bind(&btrpc::Dispatcher::onMessage, dsp.get(),
							   std::placeholders::_1, std::placeholders::_2));
	client->connect();

	auto conn = client->connection();

	while(1){
		std::string str;
		std::cout << "rpc/tpc/exit ? >>";
		std::getline(std::cin, str);
		
		if(str == "exit") break;
		else if(str == "rpc"){
			auto req = btrpc::MessageFactory::create<btrpc::RpcRequest>();
			std::cout << "请输入方法名：";
			std::cin >> str;
			std::cout << "请输入整数分割的两个数：";
			int num1, num2;
			std::cin >> num1 >> num2;
			Json::Value params, result;
			params["num1"] = num1;
			params["num2"] = num2;

			if(!caller->call(conn, str, params, result)){
				DLOG("请求失败");
			}else{
				std::string json;
				btrpc::JSON::serialize(result, json);
				std::cout << "result:\n" << json << '\n';
			}
			
		}
		else if(str == "tpc"){
			std::getline(std::cin, str);
			auto req = btrpc::MessageFactory::create<btrpc::TopicRequest>();
			req->setId("888");
			req->setMType(btrpc::MType::REQ_TOPIC);
			req->setTopicKey(str);
			req->setTopicMsg("基尼太美");
			client->send(req);
		}else{
		}
	}

	client->shutdown();
	return 0;
}