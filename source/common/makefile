all: client server
server:test_server.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

client:test_client.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

test2:2server 2client

2server:test2_server.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

2client:test2_client.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

.PHONY:clean
clean:
	rm server client

.PHONY:clean2
clean2:
	rm 2server 2client