all: client server
server:test_server.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

client:test_client.cpp
	g++ $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

.PHONY:clean
clean:
	rm server client