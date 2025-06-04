#ifndef MULTICAST_ANNOUNCER_H
#define MULTICAST_ANNOUNCER_H

#include <string>
#include <thread>
#include <atomic>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <memory>

class MulticastAnnouncer {
public:
    MulticastAnnouncer(int port, int interval_sec = 5);
    ~MulticastAnnouncer();
    void start();
    void stop();
    void sendMulticast(int sock, const sockaddr_in& addr, const std::string& local_ip, const std::string& url);

private:
    void run();
    std::string getLocalIp(const std::string& interface_name);
    int port_;
    std::string multicast_addr_;
    int multicast_port_;
    int interval_sec_;
    std::thread thread_;
    std::atomic<bool> running_;
    std::string local_ip_;
    std::shared_ptr<ConfigManager> config_;
};

#endif // MULTICAST_ANNOUNCER_H 