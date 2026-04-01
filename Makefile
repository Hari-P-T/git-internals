CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2 -Icpp/include
LDFLAGS := -lz

SOURCES := \
	cpp/src/main.cpp \
	cpp/src/config.cpp \
	cpp/src/sha1.cpp \
	cpp/src/object_store.cpp \
	cpp/src/index.cpp \
	cpp/src/refs.cpp \
	cpp/src/repo_config.cpp \
	cpp/src/remote.cpp \
	cpp/src/working_tree.cpp \
	cpp/src/commands.cpp

OBJECTS := $(SOURCES:.cpp=.o)
TARGET := mini_git

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

cpp/src/%.o: cpp/src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	python3 -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -f $(OBJECTS) $(TARGET)
