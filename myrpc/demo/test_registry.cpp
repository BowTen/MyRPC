#include "../server/service_registry.hpp"


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