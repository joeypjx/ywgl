#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>

#include "manager/manager.h"

// 全局Manager实例，用于信号处理
std::unique_ptr<Manager> g_manager;

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "Received signal " << signum << std::endl;
    if (g_manager) {
        g_manager->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // 默认参数
    int port = 8080;
    std::string db_path = "resource_monitor.db";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--db-path" && i + 1 < argc) {
            db_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: manager [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port <port>       HTTP server port (default: 8080)" << std::endl;
            std::cout << "  --db-path <path>    Database file path (default: resource_monitor.db)" << std::endl;
            std::cout << "  --help              Show this help message" << std::endl;
            return 0;
        }
    }
    
    // 注册信号处理
    // signal(SIGINT, signalHandler);
    // signal(SIGTERM, signalHandler);
    
    // 创建Manager实例
    g_manager = std::make_unique<Manager>(port, db_path);
    
    if (!g_manager->initialize()) {
        std::cerr << "Failed to initialize manager" << std::endl;
        return 1;
    }
    
    // 启动Manager
    if (!g_manager->start()) {
        std::cerr << "Failed to start manager" << std::endl;
        return 1;
    }
    
    std::cout << "Manager started on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // 等待信号
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
