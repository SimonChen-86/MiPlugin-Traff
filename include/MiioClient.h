#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

// 插座数据结构体
struct PlugData
{
    bool online = false;            // 是否在线通信正常
    bool is_on = false;             // 开关是否打开
    double power_w = 0.0;           // 实时功率 (W)
    double current_a = 0.0;         // 实时电流 (A)
    double temperature_c = 0.0;     // 内部温度 (℃)
    std::string model;              // 设备型号
    std::string fw_ver;             // 固件版本
    std::string error_msg;          // 错误信息
    std::string update_time_str;    // 最后更新时间文本
};

class MiioClient
{
public:
    MiioClient();
    ~MiioClient();

    // 设置设备 IP 与 Token
    void SetTarget(const std::string& ip, const std::string& tokenHex);

    // 测试连接设备并返回是否成功及提示信息
    bool TestConnection(std::string& outMsg, double& outPower);

    // 查询当前插座的完整电力与运行数据
    bool FetchData(PlugData& data);

    // 控制插座电源开关 (true: 开, false: 关)
    bool SetPower(bool turnOn);

private:
    std::string m_ip;
    std::string m_tokenHex;
    std::vector<uint8_t> m_token;
    uint8_t m_key[16]{};
    uint8_t m_iv[16]{};

    uint32_t m_deviceId = 0;
    uint32_t m_serverStamp = 0;
    std::chrono::steady_clock::time_point m_localStampTime;
    uint32_t m_msgId = 1;
    bool m_handshakeOk = false;

    bool InitSocket();
    void CloseSocket();
    bool DoHandshake(int retries = 3);
    bool SendRawCommand(const std::string& method, const std::string& paramsJson, std::string& responseJson, int retries = 3);

    uintptr_t m_socket = ~0; // SOCKET 句柄
};
