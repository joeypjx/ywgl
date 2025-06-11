#include "rpc_server.hpp"
#include "rpc_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <ctime>

// RPC服务类
class RPCCalculator {
public:
    RPCCalculator() {
        server_ = std::make_unique<RPCServer>("tcp://*:5555");
        registerMethods();
    }

    void start() {
        try {
            std::cout << "RPC服务器启动在 tcp://*:5555" << std::endl;
            server_->start();
        } catch (const std::exception& e) {
            std::cerr << "服务器错误: " << e.what() << std::endl;
            throw;
        }
    }

    void stop() {
        if (server_) {
            server_->stop();
        }
    }

private:
    void registerMethods() {
        // 注册RPC方法
        server_->registerMethod("add", std::function<json(int,int)>(&RPCCalculator::add, this));
        server_->registerMethod("subtract", std::function<json(int,int)>(&RPCCalculator::subtract, this));
        server_->registerMethod("multiply", std::function<json(int,int)>(&RPCCalculator::multiply, this));
        server_->registerMethod("divide", std::function<json(int,int)>(&RPCCalculator::divide, this));
        server_->registerMethod("getSystemInfo", std::function<json()>(&RPCCalculator::getSystemInfo, this));
        server_->registerMethod("getProcessList", std::function<json()>(&RPCCalculator::getProcessList, this));
    }

    // RPC方法实现
    json add(int a, int b) {
        return a + b;
    }

    json subtract(int a, int b) {
        return a - b;
    }

    json multiply(int a, int b) {
        return a * b;
    }

    json divide(int a, int b) {
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        return a / b;
    }

    json getSystemInfo() {
        json info;
        info["os"] = "Linux";
        info["version"] = "1.0.0";
        info["timestamp"] = std::time(nullptr);
        return info;
    }

    json getProcessList() {
        json processes = json::array();
        processes.push_back({
            {"pid", 1},
            {"name", "init"},
            {"status", "running"}
        });
        processes.push_back({
            {"pid", 2},
            {"name", "systemd"},
            {"status", "running"}
        });
        return processes;
    }

    std::unique_ptr<RPCServer> server_;
};

// RPC客户端测试类
class RPCClientTester {
public:
    void runTests() {
        try {
            // 等待服务器启动
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            RPCClient client("tcp://localhost:5555");
            
            // 测试基本运算
            testBasicOperations(client);
            
            // 测试系统信息
            testSystemInfo(client);
            
            // 测试进程列表
            testProcessList(client);
            
            // 测试错误处理
            testErrorHandling(client);
            
        } catch (const std::exception& e) {
            std::cerr << "客户端错误: " << e.what() << std::endl;
        }
    }

private:
    void testBasicOperations(RPCClient& client) {
        std::cout << "\n测试基本运算:" << std::endl;
        std::cout << "2 + 3 = " << client.call("add", 2, 3).get<int>() << std::endl;
        std::cout << "5 - 2 = " << client.call("subtract", 5, 2).get<int>() << std::endl;
        std::cout << "4 * 3 = " << client.call("multiply", 4, 3).get<int>() << std::endl;
        std::cout << "10 / 2 = " << client.call("divide", 10, 2).get<int>() << std::endl;
    }

    void testSystemInfo(RPCClient& client) {
        std::cout << "\n测试系统信息:" << std::endl;
        json sysInfo = client.call("getSystemInfo");
        std::cout << "操作系统: " << sysInfo["os"].get<std::string>() << std::endl;
        std::cout << "版本: " << sysInfo["version"].get<std::string>() << std::endl;
        std::cout << "时间戳: " << sysInfo["timestamp"].get<time_t>() << std::endl;
    }

    void testProcessList(RPCClient& client) {
        std::cout << "\n测试进程列表:" << std::endl;
        json processes = client.call("getProcessList");
        for (const auto& proc : processes) {
            std::cout << "PID: " << proc["pid"].get<int>() 
                      << ", 名称: " << proc["name"].get<std::string>()
                      << ", 状态: " << proc["status"].get<std::string>() << std::endl;
        }
    }

    void testErrorHandling(RPCClient& client) {
        std::cout << "\n测试错误处理:" << std::endl;
        try {
            client.call("divide", 10, 0);
        } catch (const std::exception& e) {
            std::cout << "预期的错误: " << e.what() << std::endl;
        }
    }
};

int main() {
    try {
        // 创建并启动RPC服务器
        RPCCalculator calculator;
        
        // 在单独的线程中运行服务器
        std::thread server_thread([&calculator]() {
            calculator.start();
        });
        
        // 运行客户端测试
        RPCClientTester tester;
        tester.runTests();
        
        // 等待服务器线程结束
        server_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 