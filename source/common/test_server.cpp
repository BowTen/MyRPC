#include "net.hpp"
#include "dispatcher.hpp"
#include "../server/rpc_router.hpp"

void rpc_req(const btrpc::BaseConnection::ptr& conn, btrpc::RpcRequest::ptr& msg){
	std::cout << "收到RPC请求\n";
	auto jsonstr = msg->serialize();
	std::cout << "request body:\n" << jsonstr << '\n';
	Json::Value p = msg->params();
	btrpc::JSON::serialize(p, jsonstr);
	std::cout << "params:\n" << jsonstr << '\n';

	auto rsp = btrpc::MessageFactory::create<btrpc::RpcResponse>();
	rsp->setId("666");
	rsp->setMType(btrpc::MType::RSP_RPC);
	rsp->setRCode(btrpc::RCode::RCODE_OK);
	Json::Value res;
	res["res"] = "输出成功";
	rsp->setResult(res);
	conn->send(rsp);
}

void tpc_req(const btrpc::BaseConnection::ptr& conn, btrpc::TopicRequest::ptr& msg){
	std::cout << "收到topic请求\n";
	auto jsonstr = msg->serialize();
	std::cout << "request body:\n" << jsonstr << '\n';
	std::string tmsg = msg->topicMsg();
	std::cout << "topicMsg:\n" << tmsg << '\n';


	auto rsp = btrpc::MessageFactory::create<btrpc::RpcResponse>();
	rsp->setId("666");
	rsp->setMType(btrpc::MType::RSP_TOPIC);
	rsp->setRCode(btrpc::RCode::RCODE_OK);
	Json::Value res;
	res["res"] = "tpc成功";
	rsp->setResult(res);
	conn->send(rsp);
}

void Add(const Json::Value &params, Json::Value &res){
	int a = params["num1"].asInt();
	int b = params["num2"].asInt();
	res = a + b;
}

int main(int argc, char* argv[]){

	if(argc != 2){
		std::cout << "Usage: server [port]\n";
		return 0;
	}
	int port = atoi(argv[1]);

	auto rpc_router = std::make_shared<btrpc::server::RpcRouter>();
	btrpc::server::SDescribeFactory sd_fac;
	sd_fac.setMethodName("Add");
	sd_fac.setParamsDesc("num1", btrpc::server::VType::INTEGRAL);
	sd_fac.setParamsDesc("num2", btrpc::server::VType::INTEGRAL);
	sd_fac.setReturnType(btrpc::server::VType::INTEGRAL);
	sd_fac.setCallback(Add);
	rpc_router->registerMethod(sd_fac.build());
	auto rpc_router_cb = std::bind(&btrpc::server::RpcRouter::onRpcRequest, rpc_router.get(),
								   std::placeholders::_1, std::placeholders::_2);


	auto dsp = std::make_shared<btrpc::Dispatcher>();
	dsp->registerHandler<btrpc::RpcRequest>(btrpc::MType::REQ_RPC, rpc_router_cb);
	dsp->registerHandler<btrpc::TopicRequest>(btrpc::MType::REQ_TOPIC, tpc_req);

	auto server = btrpc::ServerFactory::create(port);
	server->setMessageCallback(std::bind(&btrpc::Dispatcher::onMessage, dsp.get(),
							   std::placeholders::_1, std::placeholders::_2));
	server->start();

	return 0;
}