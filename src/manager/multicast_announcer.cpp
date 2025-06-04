#include "multicast_announcer.h"
#include "ConfigManager.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fstream>

// MulticastAnnouncer 构造函数
MulticastAnnouncer::MulticastAnnouncer(int port, int interval_sec = 5)
    : port_(port), interval_sec_(interval_sec), running_(false), local_ip_("127.0.0.1"),
      multicast_addr_("239.255.0.1"), multicast_port_(50000) {}

// 析构函数，自动停止多播线程
MulticastAnnouncer::~MulticastAnnouncer() {
    stop();
}

// 启动多播线程
void MulticastAnnouncer::start() {
    if (running_) return;
    nlohmann::json config = ConfigManager::load("config.json");
    std::string ifname = ConfigManager::getString(config, "interface", "eth0");
    multicast_addr_ = ConfigManager::getString(config, "multicast_addr", "239.255.0.1");
    multicast_port_ = ConfigManager::getInt(config, "multicast_port", 50000);
    local_ip_ = getLocalIp(ifname);
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

    int counter = 0;

    while (running_) {
        // 发送 /heartbeat
        sendMulticast(sock, addr, local_ip_, "/heartbeat");

        // 每3次发送一次 /resource
        if (counter % 3 == 0) {
            sendMulticast(sock, addr, local_ip_, "/resource");
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

// 获取指定网卡的IP地址（如"en0"、"eth0"）
std::string MulticastAnnouncer::getLocalIp(const std::string& ifname) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "[MulticastAnnouncer] 创建socket失败" << std::endl;
        return "127.0.0.1";
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = 0;
    if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in* ipaddr = (struct sockaddr_in*)&ifr.ifr_addr;
        std::string ip = inet_ntoa(ipaddr->sin_addr);
        close(fd);
        return ip;
    } else {
        std::cerr << "[MulticastAnnouncer] 获取网卡" << ifname << " IP失败，使用127.0.0.1" << std::endl;
        close(fd);
        return "127.0.0.1";
    }
}