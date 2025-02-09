#include <iostream>
#include <functional>


template<typename T>
void fun(std::function<void(T)> f, float a){
	T b = (T)a;
	f(b);
}

void pt(int a){
	std::cout << a << '\n';
}

int main() {

	//std::function<void(int)> a;

	auto bf = std::bind(fun<int>, pt, std::placeholders::_1);
	bf(3);

    return 0;
}