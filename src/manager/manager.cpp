#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "multicast_announcer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <dirent.h>
#include <fstream>
#include <sstream>

Manager::Manager(int port, const std::string& db_path)
    : port_(port), db_path_(db_path), running_(false) {}

Manager::~Manager() {
    if (running_) {
        stop();
    }
}

bool Manager::initialize() {
    std::cout << "[Manager] 初始化..." << std::endl;

    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    if (!db_manager_ || !db_manager_->initialize()) {
        std::cerr << "[Manager] 数据库管理器初始化失败" << std::endl;
        return false;
    }

    http_server_ = std::make_unique<HTTPServer>(db_manager_, port_);
    multicast_announcer_ = std::make_unique<MulticastAnnouncer>(port_);

    std::cout << "[Manager] 初始化成功" << std::endl;
    return true;
}

bool Manager::start() {
    if (running_) {
        std::cerr << "[Manager] 已经在运行" << std::endl;
        return false;
    }

    std::cout << "[Manager] 启动..." << std::endl;

    if (http_server_) {
        std::thread server_thread([this]() {
            if (!http_server_->start()) {
                std::cerr << "[Manager] HTTP服务器启动失败" << std::endl;
                running_ = false;
            }
        });
        server_thread.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (multicast_announcer_) {
        multicast_announcer_->start();
    }

    running_ = true;
    std::cout << "[Manager] 启动成功" << std::endl;
    return true;
}

void Manager::stop() {
    if (!running_) {
        std::cerr << "[Manager] 未在运行" << std::endl;
        return;
    }

    std::cout << "[Manager] 停止..." << std::endl;

    if (http_server_) {
        http_server_->stop();
    }
    if (multicast_announcer_) {
        multicast_announcer_->stop();
    }

    running_ = false;
    std::cout << "[Manager] 已停止" << std::endl;
}

json Manager::handleGetSystemInfo() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        throw std::runtime_error("Failed to get system info");
    }

    json result;
    result["uptime"] = si.uptime;
    result["total_ram"] = si.totalram;
    result["free_ram"] = si.freeram;
    result["shared_ram"] = si.sharedram;
    result["buffer_ram"] = si.bufferram;
    result["total_swap"] = si.totalswap;
    result["free_swap"] = si.freeswap;
    result["procs"] = si.procs;
    result["loads"][0] = si.loads[0] / 65536.0;
    result["loads"][1] = si.loads[1] / 65536.0;
    result["loads"][2] = si.loads[2] / 65536.0;

    return result;
}

json Manager::handleGetResourceUsage() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        throw std::runtime_error("Failed to get resource usage");
    }

    json result;
    result["user_time"] = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
    result["system_time"] = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
    result["max_rss"] = usage.ru_maxrss;
    result["page_faults"] = usage.ru_majflt;
    result["block_input"] = usage.ru_inblock;
    result["block_output"] = usage.ru_oublock;
    result["voluntary_context_switches"] = usage.ru_nvcsw;
    result["involuntary_context_switches"] = usage.ru_nivcsw;

    return result;
}

json Manager::handleGetProcessList() {
    json result = json::array();
    DIR* dir = opendir("/proc");
    if (!dir) {
        throw std::runtime_error("Failed to open /proc directory");
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string name = entry->d_name;
            if (std::all_of(name.begin(), name.end(), ::isdigit)) {
                int pid = std::stoi(name);
                try {
                    json process_info = handleGetProcessInfo(pid);
                    result.push_back(process_info);
                } catch (...) {
                    // 忽略无法读取的进程信息
                    continue;
                }
            }
        }
    }
    closedir(dir);
    return result;
}

json Manager::handleGetProcessInfo(int pid) {
    json result;
    result["pid"] = pid;

    // 读取进程状态
    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(stat_path);
    if (stat_file.is_open()) {
        std::string line;
        std::getline(stat_file, line);
        std::istringstream iss(line);
        std::string token;
        
        // 跳过前13个字段
        for (int i = 0; i < 13; ++i) {
            iss >> token;
        }
        
        // 读取用户态和系统态CPU时间
        unsigned long utime, stime;
        iss >> utime >> stime;
        result["user_time"] = utime / sysconf(_SC_CLK_TCK);
        result["system_time"] = stime / sysconf(_SC_CLK_TCK);
    }

    // 读取进程内存信息
    std::string status_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_file(status_path);
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string key, value;
                iss >> key >> value;
                result["memory_usage"] = std::stoi(value);
            }
        }
    }

    return result;
}
