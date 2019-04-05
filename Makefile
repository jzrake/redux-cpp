CXXFLAGS = -std=c++17

all: example rx-example

example: example.cpp redux.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

rx-example: rx-example.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -I../RxCpp/Rx/v2/src
