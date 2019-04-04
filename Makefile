CXXFLAGS = -std=c++17

example: example.cpp redux.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<
