# Makefile for alarm_manager

CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
TARGET = alarm_manager
SRCDIR = .
SRC = $(SRCDIR)/main.cpp
OBJ = $(SRC:.cpp=.o)
DEPS = $(wildcard $(SRCDIR)/*.h)
DEPS_DIR = ../../../deps
INCLUDES = -I$(SRCDIR) -I$(DEPS_DIR)/nlohmann_json/include -I$(DEPS_DIR)/SQLiteCpp/include
LIB_DIRS = -L$(DEPS_DIR)/SQLiteCpp/build

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread $(LIB_DIRS) -lsqlite3 -lSQLiteCpp

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean