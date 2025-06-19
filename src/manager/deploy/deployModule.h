#ifndef MODULE_DATA_ACCESS_H
#define MODULE_DATA_ACCESS_H

class ModuleDataAccess{
public:
    void updateComponentState(const std::string& instanceId, int componentIndex, const std::string& status, double cpuUsage, double memoryUsage, double gpuUsage, long long networkRx, long long networkTx);
    void nodeAbnormal(const std::string& hostIp);
private:
};

#endif //MODULE_DATA_ACCESS_H