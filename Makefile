CXX      ?= g++
CXXFLAGS ?= -Wall -Wextra -Wpedantic -std=c++11

.PHONY: all test example clean

all: jsonobject_example1 jsonobject_test

jsonobject_example1: jsonobject_example1.cpp jsonobject.cpp jsonobject.h
	$(CXX) $(CXXFLAGS) -o jsonobject_example1 jsonobject_example1.cpp jsonobject.cpp

jsonobject_test: jsonobject_test.cpp jsonobject.cpp jsonobject.h
	$(CXX) $(CXXFLAGS) -o jsonobject_test jsonobject_test.cpp jsonobject.cpp

test: jsonobject_test
	./jsonobject_test

example: jsonobject_example1
	./jsonobject_example1

clean:
	rm -f jsonobject_example1 jsonobject_test
