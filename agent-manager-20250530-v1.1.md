# agent与manager接口协议
- 日期：2025-05-30
- 版本：v1.1
- 接口协议包括状态协议，命令协议
## 状态协议
1. 状态包含代理心跳、资源信息
2. 状态由manager查询，agent上报状态信息，遵循一查一报原则
3. agent通过http协议上报，要求回复的body为空，状态码为200
### manager查询
udp组播：239.192.168.80:3980
```json
{
    "api_version" : 1,
    "data": {
        "manager_ip": "192.168.10.254，string（点分十进制），上报的ip地址",
        "manager_port": "3926, int 上报的端口",
        "url": "/heartbeat,/resource 每次只能请求一个url"
    }
}
```

### agent心跳信息上报
POST /heartbeat
```json
{
    "api_version" : 1,
    "data": {
        "box_id": "1，int，机箱号",
        "slot_id": "1，int, 槽位号",
        "cpu_id": "1，int，cpu号",
        "srio_id": "0，int，srio号",
        "host_ip": "192.168.10.29，string，点分十进制，主机ip",
        "hostname": "localhost.localdomain，string，主机名",
        "service_port": "23980, uint16_t，命令响应服务端口",
        "box_type": "计算I型，string，机箱类型",
        "board_type": "GPU，string，板卡类型",
        "cpu_type": "Phytium,D2000/8，string，cpu类型",
        "os_type": "Kylin Linux Advanced Server V10，string，操作系统类型",
        "resource_type": "GPU I，string，GPU计算模块I型",
        "cpu_arch": "aarch64，string，cpu架构",
        "gpu": [
            {
                "index": "int，gpu设备序号",
                "name": "string，gpu设备名称",
            }
        ]
    }
}
```


### agent资源信息上报
POST /resource
```json
{
    "api_version" : 1,
    "data":{
        "host_ip": "192.168.10.29，string，点分十进制，主机ip",
        "resource": {
            "cpu" : {
                "usage_percent": "double，cpu使用率百分比",
                "load_avg_1m": "double，cpu1分钟负载",
                "load_avg_5m": "double，cpu5分钟负载",
                "load_avg_15m": "double，cpu15分钟负载" ,
                "core_count": "int，cpu核数",
                "core_allocated": "int, cpu已分配核数",
                "temperature": "double, 温度, 摄氏度",
                "voltage": "double，电压，伏特",
                "current": "double，电流，安培",
                "power": "double 功耗，瓦特"
            },
            "memory": {
                "total" : "uint64_t，内存总大小，字节", 
                "used" : "uint64_t，内存已使用大小，字节",
                "free" : "uint64_t，内存剩余大小，字节",
                "usage_percent" : "double，内存使用率，百分比"
            },
            "network": [
                {
                    "interface": "string，网卡名",
                    "rx_bytes": "uint64_t，接收字节",
                    "tx_bytes": "uint64_t，发送字节",
                    "rx_packets": "uint64_t，接收报文",
                    "tx_packets": "uint64_t，发送报文",
                    "rx_errors": "int，接收错误",
                    "tx_errors": "int，发送错误"
                }
            ],
            "disk":[
                {
                    "device": "string，设备名称",
                    "mount_point": "string，挂载路径",
                    "total": "uint64_t，总大小，字节",
                    "used": "uint64_t，已使用大小，字节",
                    "free": "uint64_t， 剩余大小，字节",
                    "usage_percent": "double，使用率，百分比"
                }
            ],
            "gpu":[
                {
                    "index": "int，gpu设备序号",
                    "name": "string，gpu设备名称",
                    "compute_usage": "double，gpu计算使用率",
                    "mem_usage": "double， gpu内存使用率",
                    "mem_used": "uint64_t,显存已使用大小，字节",
                    "mem_total": "uint64_t，显存总大小，字节",
                    "temperature": "double, 温度, 摄氏度",
                    "voltage": "double，电压，伏特",
                    "current": "double，电流，安培",
                    "power": "double 功耗，瓦特"
                }
            ]
        },
        "component": [
            {
                "instance_id": "string, 容器所属的业务示例id",
                "uuid": "string，容器所属的组件实例uuid",
                "index": "int",
                "config": {
                    "name": "string，容器name",
                    "id": "string，容器ID", // docker id
                },
                "state": "PENDING/RUNNING/FAILED/STOPPED/SLEEPING，string，容器运行状态",
                "resource":{ // 容器资源使用情况
                    "cpu" : {
                        "load": "double,负载率，百分比"
                    },
                    "memory": {
                        "mem_used" : "uint64_t，内存占用，字节",
                        "mem_limit" : "uint64_t，内存限制，字节",
                    },
                    "network": {
                        "tx": "uint64_t，发送字节，字节",
                        "rx": "uint64_t，接收字节，字节"
                    }
                }
            }
        ]
    }
}
```

## 命令协议
1. 命令协议包括加载命令
2. 命令协议由manager发起请求，agent回复结果
### 加载命令
POST /deploy
```json
//请求
{
    "api_version" : 1,
    "data":{
        "mode" : "start/stop，string，部署方式，启动和停止，必要字段",
        "type" : "docker/bar，string，部署类型，支持docker和bar，必要字段",
        "uuid" : "string，组件实例uuid，必要字段",
        "id": "string，容器ID/int bar进程pid，停止时必要", // 停止时传入
        "url" : "string，软件仓库地址，启动时必要",
        "file" : "软件名称，启动时必要",
        "require" :{
            "cpu" : "1, int, 需要的cpu核数， 启动时必要",
            "memory": "uint64_t，需要的内存大小，字节，启动时必要",
            "gpu": "1，int，需要的gpu个数，启动时必要"
        }
    }
}
//回复
{
    "api_version" : 1,
    "data":{
        "mode" : "start/stop，string，部署方式，启动和停止",
        "type" : "docker/bar，string，部署类型，支持docker和bar",
        "uuid" : "string，组件实例uuid",
        "id": "string，容器ID/int bar进程pid", // docker id
        "url" : "string，软件仓库地址",
        "file" : "软件名称",
        "require" :{
            "cpu" : "1, int, 需要的cpu核数",
            "memory": "uint64_t，需要的内存大小，字节",
            "gpu": "1，int，需要的gpu个数"
        },
        "result": "int 结果代码，0代表成功",
        "message": "string，结果具体信息"
    }
}

```
