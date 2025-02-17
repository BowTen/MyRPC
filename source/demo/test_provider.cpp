#include "../client/registry_discover.hpp"


int main(int argc, char* argv[]){

	if(argc != 7){
		std::cout << "Usage: provider [registry/deregister] [method] [sip] [sport] [rip] [rport]\n";
		return 0;
	}
	std::string op(argv[1]);
	std::string method(argv[2]);
	std::string sip(argv[3]);
	int sport = atoi(argv[4]);
	std::string rip(argv[5]);
	int rport = atoi(argv[6]);

	auto pr = std::make_shared<myrpc::client::Provider>(rip, rport);
	auto host = std::make_pair(sip, sport);
	if(op == "registry") pr->registryMethod(method, host);
	else if(op == "deregister") pr->deregisterMethod(method, host);
	else{
		std::cout << "Usage: provider [registry/deregister] [method] [sport] [rport]\n";
		return 0;
	}

	sleep(1);

	return 0;
}
