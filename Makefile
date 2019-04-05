all: example rx-example

example: example.cpp redux.hpp rx-redux.hpp
	$(CXX) -o $@ $< -std=c++17

rx-example: example.cpp redux.hpp rx-redux.hpp
	$(CXX) -o $@ $< -std=c++14 -I../RxCpp/Rx/v2/src -DHAVE_RXCPP
