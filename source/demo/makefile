all: registry provider Add Sub discoverer discoverer_cb
registry:test_registry.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

provider:test_provider.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

Add:test_Add.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

Sub:test_Sub.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

discoverer:test_discoverer.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

discoverer_cb:test_discoverer_cb.cpp
	g++ -g $^ -o $@ -I /home/bowten/study/RPC/ -L /home/bowten/study/RPC/lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread


.PHONY:clean
clean:
	rm registry provider Add Sub discoverer discoverer_cb