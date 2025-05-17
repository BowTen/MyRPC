CXX = g++
CXXFLAGS = -g -I ../../
LDFLAGS = -L ../../lib -ljsoncpp -lmuduo_net -lmuduo_base -lpthread

all: registry provider Add Sub discoverer discoverer_cb

%: test_%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean all

clean:
	rm -f registry provider Add Sub discoverer discoverer_cb
