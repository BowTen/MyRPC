#include "net.hpp"
#include "dispatcher.hpp"


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

	auto dsp = std::make_shared<btrpc::Dispatcher>();
	dsp->registerHandler<btrpc::RpcResponse>(btrpc::MType::RSP_RPC, rpc_rsp);
	dsp->registerHandler<btrpc::TopicResponse>(btrpc::MType::RSP_TOPIC, tpc_rsp);

	auto client = btrpc::ClientFactory::create("127.0.0.1", port);
	client->setMessageCallback(std::bind(&btrpc::Dispatcher::onMessage, dsp.get(),
							   std::placeholders::_1, std::placeholders::_2));
	client->connect();

	while(1){
		std::string str;
		std::cout << "rpc/tpc/exit ? >>";
		std::getline(std::cin, str);
		
		if(str == "exit") break;
		else if(str == "rpc"){
			auto req = btrpc::MessageFactory::create<btrpc::RpcRequest>();
			req->setId("888");
			req->setMType(btrpc::MType::REQ_RPC);
			req->setMethod("Add");
			int num1, num2;
			std::cout << "输入空格分割的两个数：\n";
			std::cin >> num1 >> num2;
			Json::Value params;
			params["num1"] = num1;
			params["num2"] = num2;
			req->setParams(params);
			client->send(req);
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


	return 0;
}