CXXFLAGS=-std=c++14
LDFLAGS=-lboost_program_options

process_motion: main.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ 
