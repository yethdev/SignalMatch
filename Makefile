CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

signalmatch: src/main.cpp src/ir_db.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f signalmatch

.PHONY: clean
