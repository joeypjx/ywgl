# 资源监控系统 Makefile

# 编译器和标准
CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra

# 项目目录
SRC_DIR = src
MANAGER_DIR = $(SRC_DIR)/manager
ALARM_DIR = $(MANAGER_DIR)/alarm
BUILD_DIR = build

# 依赖库目录
DEPS_DIR = deps
JSON_DIR = $(DEPS_DIR)/nlohmann_json
HTTPLIB_DIR = $(DEPS_DIR)/cpp-httplib
SQLITECPP_DIR = $(DEPS_DIR)/SQLiteCpp

# 包含目录
INCLUDES = -I$(SRC_DIR) \
          -I$(MANAGER_DIR) \
          -I$(ALARM_DIR) \
          -I$(JSON_DIR)/include \
          -I$(HTTPLIB_DIR) \
          -I$(SQLITECPP_DIR)/include \
          -I/usr/local/include

# 库目录
LIB_DIRS = -L/usr/local/lib \
           -L/usr/lib \
		   -L/usr/lib64 \
           -L$(SQLITECPP_DIR)/build \
		   -L/usr/local/taos/driver

# Manager源文件
MANAGER_BASE_SOURCES = $(wildcard $(MANAGER_DIR)/*.cpp)
ALARM_SOURCES = $(wildcard $(ALARM_DIR)/*.cpp)
MANAGER_SOURCES = $(MANAGER_BASE_SOURCES) \
				  $(ALARM_SOURCES) \
                  $(SRC_DIR)/manager_main.cpp

# 目标文件
MANAGER_OBJECTS = $(MANAGER_SOURCES:%.cpp=$(BUILD_DIR)/%.o)

# 依赖库
MANAGER_LIBS = -lsqlite3 -lpthread -lSQLiteCpp -ltaos -l:libtaosws.so.3.3.6.9

# 目标可执行文件
MANAGER_TARGET = $(BUILD_DIR)/manager

# 默认目标
all: prepare $(MANAGER_TARGET)

# 准备构建目录
prepare:
	mkdir -p $(BUILD_DIR)/$(SRC_DIR)
	mkdir -p $(BUILD_DIR)/$(MANAGER_DIR)
	mkdir -p $(BUILD_DIR)/$(ALARM_DIR)

# 编译Manager
$(MANAGER_TARGET): $(MANAGER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_DIRS) $(MANAGER_LIBS)

# 编译规则
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# 清理
clean:
	rm -rf $(BUILD_DIR)

# 安装
install: all
	mkdir -p /usr/local/bin
	cp $(MANAGER_TARGET) /usr/local/bin/

# 依赖检查
deps:
	@echo "检查依赖..."
	@which $(CXX) > /dev/null || (echo "错误: 需要安装 g++" && exit 1)
	@ldconfig -p | grep libcurl > /dev/null || (echo "错误: 需要安装 libcurl" && exit 1)
	@ldconfig -p | grep libuuid > /dev/null || (echo "错误: 需要安装 libuuid" && exit 1)
	@ldconfig -p | grep libsqlite3 > /dev/null || (echo "错误: 需要安装 libsqlite3" && exit 1)
	@echo "所有依赖已满足"

# 帮助
help:
	@echo "资源监控系统 Makefile"
	@echo "使用方法:"
	@echo "  make        - 构建agent和manager"
	@echo "  make agent  - 仅构建agent"
	@echo "  make manager - 仅构建manager"
	@echo "  make clean   - 清理构建文件"
	@echo "  make install - 安装到系统"
	@echo "  make deps    - 检查依赖"
	@echo "  make help    - 显示此帮助信息"

# 单独构建目标
manager: prepare $(MANAGER_TARGET)

.PHONY: all prepare clean install deps help manager