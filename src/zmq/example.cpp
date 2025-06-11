#include "rpc_server.hpp"
#include "rpc_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// 示例服务类
class Calculator {
public:
    Json::Value add(int a, int b) {
        Json::Value result;
        result["sum"] = a + b;
        return result;
    }
    
    Json::Value multiply(int a, int b) {
        Json::Value result;
        result["product"] = a * b;
        return result;
    }
};

int main() {
    const std::string endpoint = "tcp://localhost:5555";
    
    // 创建并启动服务器
    RPCServer server(endpoint);
    Calculator calc;
    
    // 注册远程方法
    server.registerMethod("add", [&calc](int a, int b) { return calc.add(a, b); });
    server.registerMethod("multiply", [&calc](int a, int b) { return calc.multiply(a, b); });
    
    server.start();
    std::cout << "Server started on " << endpoint << std::endl;
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 创建客户端
    RPCClient client(endpoint);
    
    try {
        // 调用远程方法
        Json::Value result1 = client.call("add", 5, 3);
        std::cout << "5 + 3 = " << result1["sum"].asInt() << std::endl;
        
        Json::Value result2 = client.call("multiply", 4, 6);
        std::cout << "4 * 6 = " << result2["product"].asInt() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    // 停止服务器
    server.stop();
    return 0;
} 