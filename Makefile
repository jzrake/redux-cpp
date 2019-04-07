all: example rx-example undoable-example

example: example.cpp redux.hpp rx-redux.hpp
	$(CXX) -o $@ $< -std=c++17

rx-example: example.cpp redux.hpp rx-redux.hpp
	$(CXX) -o $@ $< -std=c++14 -I../RxCpp/Rx/v2/src -DHAVE_RXCPP

undoable-example: undoable-example.cpp
	$(CXX) -o $@ $< -std=c++14 -I../immer

clean:
	$(RM) *.o example rx-example undoable-example
