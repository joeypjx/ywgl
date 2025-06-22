#ifndef TDEINGINE_MANAGER_H
#define TDEINGINE_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

// 包含 TDengine WebSocket 头文件
#include "/usr/local/taos/include/taosws.h" 

// 代表先前在 'node' 表中的数据
struct NodeInfo {
    int id;
    int box_id;
    int slot_id;
    int cpu_id;
    int srio_id;
    std::string host_ip;
    std::string hostname;
    int service_port;
    std::string box_type;
    std::string board_type;
    std::string cpu_type;
    std::string os_type;
    std::string resource_type;
    std::string cpu_arch;
    nlohmann::json gpu; // 以解析后的 JSON 格式存储
    std::string status;
    long long created_at;
    long long updated_at;
};

// 节点缓存 map 的键
struct NodeKey {
    int box_id;
    int slot_id;
    int cpu_id;

    bool operator<(const NodeKey& other) const {
        if (box_id != other.box_id) return box_id < other.box_id;
        if (slot_id != other.slot_id) return slot_id < other.slot_id;
        return cpu_id < other.cpu_id;
    }
};

class TDengineManager {
public:
    /**
     * @brief 构造函数，用于连接 TDengine
     * @param host 服务器地址
     * @param user 用户名
     * @param password 密码
     * @param db 数据库名
     * @param port 端口号
     */
    TDengineManager(const std::string& host, const std::string& user, const std::string& password, const std::string& db, int port = 6041);

    /**
     * @brief 析构函数，断开与 TDengine 的连接
     */
    ~TDengineManager();

    // 禁止拷贝和赋值
    TDengineManager(const TDengineManager&) = delete;
    TDengineManager& operator=(const TDengineManager&) = delete;

    /**
     * @brief 检查数据库连接状态
     * @return 如果连接成功则返回 true，否则返回 false
     */
    bool isConnected() const;

    /**
     * @brief 在 TDengine 中创建本应用所需的超级表
     */
    void createTables();

    /**
     * @brief 将格式化的资源使用数据保存到 TDengine
     * @param resource_usage 包含所有指标的 JSON 对象
     * @return 如果操作成功，返回 true
     */
    bool saveMetrics(const nlohmann::json& resource_usage);

    /**
     * @brief 更新或插入节点的元数据信息
     * @param node_info 包含节点信息的 JSON 对象
     * @return 如果操作成功，返回 true
     */
    bool updateNodeInfo(const nlohmann::json& node_info);

    /**
     * @brief 根据 box_id, slot_id, 和 cpu_id 获取节点信息
     * @return 包含节点信息的 JSON 对象，如果未找到则为空
     */
    nlohmann::json getNodeInfo(int box_id, int slot_id, int cpu_id);

    /**
     * @brief 根据 host_ip 获取节点信息
     * @return 包含节点信息的 JSON 对象，如果未找到则为空
     */
    nlohmann::json getNodeInfoByHostIp(const std::string& host_ip);

    /**
     * @brief 获取所有节点的元数据信息
     * @return 包含所有节点信息的 JSON 数组
     */
    nlohmann::json getAllNodesInfo();

    /**
     * @brief 获取所有节点的信息及其最新的指标数据
     * @return 包含节点及其最新指标的 JSON 数组
     */
    nlohmann::json getNodesWithLatestMetrics();

    /**
     * @brief 启动一个后台线程来监控节点状态
     */
    void startNodeStatusMonitor();

    /**
     * @brief 停止节点状态监控线程
     */
    void stopNodeStatusMonitor();

private:
    /**
     * @brief 执行一个 SQL 查询
     * @param sql 要执行的 SQL 语句
     */
    void executeQuery(const std::string& sql);

    /**
     * @brief 执行一个 SQL 查询并以 JSON 格式返回结果
     * @param sql 要执行的 SQL 语句
     * @return 查询结果的 JSON 数组
     */
    nlohmann::json queryTDengine(const std::string& sql);

    /**
     * @brief 清理字符串，使其可用作 TDengine 表名
     * @param input 原始字符串
     * @return 清理后的字符串
     */
    std::string sanitizeForTableName(const std::string& input);

    void nodeStatusMonitorLoop();

    WS_TAOS* taos_ = nullptr;
    bool connected_ = false;

    // 节点元数据的内存缓存
    std::map<NodeKey, NodeInfo> node_info_cache_;
    std::map<std::string, NodeKey> host_ip_to_key_map_;
    std::mutex node_cache_mutex_;

    // 节点状态监控
    std::unique_ptr<std::thread> node_status_monitor_thread_;
    std::atomic<bool> node_status_monitor_running_{false};
};

#endif // TDEINGINE_MANAGER_H 