#include "multicast_announcer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <nlohmann/json.hpp>
// 构造函数，初始化多播参数
MulticastAnnouncer::MulticastAnnouncer(int port, const std::string& multicast_addr, int multicast_port, int interval_sec)
    : port_(port), multicast_addr_(multicast_addr), multicast_port_(multicast_port), interval_sec_(interval_sec), running_(false) {}

// 析构函数，自动停止多播线程
MulticastAnnouncer::~MulticastAnnouncer() {
    stop();
}

// 启动多播线程
void MulticastAnnouncer::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&MulticastAnnouncer::run, this);
}

// 停止多播线程
void MulticastAnnouncer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

// 多播主循环，定期广播本机IP和端口
void MulticastAnnouncer::run() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "[MulticastAnnouncer] 创建socket失败" << std::endl;
        return;
    }

    int ttl = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        std::cerr << "[MulticastAnnouncer] 设置TTL失败" << std::endl;
        close(sock);
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast_addr_.c_str());
    addr.sin_port = htons(multicast_port_);

    std::string local_ip = getLocalIp();
    int counter = 0;

    while (running_) {
        // 发送 /heartbeat
        sendMulticast(sock, addr, local_ip, "/heartbeat");

        // 每3次发送一次 /resource
        if (counter % 3 == 0) {
            sendMulticast(sock, addr, local_ip, "/resource");
        }

        counter++;
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
    }
    close(sock);
}

void MulticastAnnouncer::sendMulticast(int sock, const sockaddr_in& addr, const std::string& local_ip, const std::string& url) {
    std::cout << "[MulticastAnnouncer] 发送 " << url << " 多播" << std::endl;
    nlohmann::json data_json = {
        {"manager_ip", local_ip},
        {"manager_port", port_},
        {"url", url}
    };
    nlohmann::json msg_json = {
        {"api_version", 1},
        {"data", data_json}
    };
    std::string msg = msg_json.dump();
    int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        std::cerr << "[MulticastAnnouncer] 发送 " << url << " 多播失败: " << strerror(errno) << std::endl;
    }
}

// 获取本机局域网IP地址
std::string MulticastAnnouncer::getLocalIp() {
    char host[256] = {0};
    if (gethostname(host, sizeof(host)) == 0) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res) {
            sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);
            freeaddrinfo(res);
            return ip;
        }
    }
    std::cerr << "[MulticastAnnouncer] 获取本机IP失败，使用127.0.0.1" << std::endl;
    return "127.0.0.1";
}