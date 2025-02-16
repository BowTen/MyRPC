all: registry provider Add discoverer
registry:test_registry.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

provider:test_provider.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

Add:test_Add.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

discoverer:test_discoverer.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread


.PHONY:clean
clean:
	rm registry provider Add discoverer




test2:2server 2client

2server:test2_server.cpp
	g++ -g -m64 $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

2client:test2_client.cpp
	g++ -g -m64 $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread



.PHONY:clean2
clean2:
	rm 2server 2client