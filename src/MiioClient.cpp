#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "MiioClient.h"
#include "CryptoHelper.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace
{
    // miIO 协议相关常量定义
    constexpr uint16_t MIIO_DEFAULT_PORT = 54321;
    constexpr uint32_t MIIO_STAMP_MAX_AGE_SEC = 50;
    constexpr DWORD MIIO_SOCKET_TIMEOUT_MS = 2500;
    constexpr size_t MIIO_HEADER_LEN = 32;

    // 简易 JSON 字段解析辅助
    std::string ExtractJsonString(const std::string& json, const std::string& key)
    {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        size_t endPos = json.find("\"", pos);
        if (endPos == std::string::npos) return "";
        return json.substr(pos, endPos - pos);
    }

    // 简易解析 result 数组中的项: {"result":["on",52.2,0.26,54.12,0.0],"id":2}
    std::vector<std::string> ParseResultArray(const std::string& json)
    {
        std::vector<std::string> results;
        size_t startPos = json.find("\"result\":[");
        if (startPos == std::string::npos) return results;
        startPos += 10;
        size_t endPos = json.find("]", startPos);
        if (endPos == std::string::npos) return results;

        std::string arrayContent = json.substr(startPos, endPos - startPos);
        std::stringstream ss(arrayContent);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            // 去除前后空白与引号
            item.erase(0, item.find_first_not_of(" \t\r\n\""));
            item.erase(item.find_last_not_of(" \t\r\n\"") + 1);
            results.push_back(item);
        }
        return results;
    }
}

MiioClient::MiioClient()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

MiioClient::~MiioClient()
{
    CloseSocket();
    WSACleanup();
}

void MiioClient::SetTarget(const std::string& ip, const std::string& tokenHex)
{
    m_ip = ip;
    m_tokenHex = tokenHex;
    m_token = MiioCrypto::HexToBytes(tokenHex);
    m_handshakeOk = false;
    m_deviceId = 0;
    CloseSocket();

    if (m_token.size() == 16)
    {
        // 派生 key 和 iv
        MiioCrypto::ComputeMD5(m_token.data(), 16, m_key);
        std::vector<uint8_t> keyPlusToken;
        keyPlusToken.reserve(32);
        keyPlusToken.insert(keyPlusToken.end(), m_key, m_key + 16);
        keyPlusToken.insert(keyPlusToken.end(), m_token.begin(), m_token.end());
        MiioCrypto::ComputeMD5(keyPlusToken.data(), keyPlusToken.size(), m_iv);
    }
}

bool MiioClient::InitSocket()
{
    if (m_socket != (uintptr_t)~0) return true;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;

    // 设置接收与发送超时
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&MIIO_SOCKET_TIMEOUT_MS, sizeof(MIIO_SOCKET_TIMEOUT_MS));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&MIIO_SOCKET_TIMEOUT_MS, sizeof(MIIO_SOCKET_TIMEOUT_MS));

    m_socket = (uintptr_t)s;
    return true;
}

void MiioClient::CloseSocket()
{
    if (m_socket != (uintptr_t)~0)
    {
        closesocket((SOCKET)m_socket);
        m_socket = (uintptr_t)~0;
    }
}

bool MiioClient::DoHandshake(int retries)
{
    if (m_ip.empty() || m_token.size() != 16) return false;
    if (!InitSocket()) return false;

    sockaddr_in targetAddr{};
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(MIIO_DEFAULT_PORT);
    inet_pton(AF_INET, m_ip.c_str(), &targetAddr.sin_addr);

    // 32 字节标准握手包
    std::string helloHex = "21310020ffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    std::vector<uint8_t> helloPacket = MiioCrypto::HexToBytes(helloHex);

    for (int i = 0; i < retries; ++i)
    {
        int sent = sendto((SOCKET)m_socket, (const char*)helloPacket.data(), static_cast<int>(helloPacket.size()), 0,
                          (sockaddr*)&targetAddr, sizeof(targetAddr));
        if (sent <= 0)
        {
            CloseSocket();
            InitSocket();
            Sleep(150);
            continue;
        }

        uint8_t buffer[1024];
        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);
        int received = recvfrom((SOCKET)m_socket, (char*)buffer, sizeof(buffer), 0, (sockaddr*)&fromAddr, &fromLen);

        if (received >= static_cast<int>(MIIO_HEADER_LEN) && buffer[0] == 0x21 && buffer[1] == 0x31)
        {
            // 解析 Device ID 和时间戳 Stamp（网络大端序）
            m_deviceId = (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11];
            m_serverStamp = (buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15];
            m_localStampTime = std::chrono::steady_clock::now();
            m_handshakeOk = true;
            return true;
        }
        Sleep(150);
    }
    m_handshakeOk = false;
    return false;
}

bool MiioClient::SendRawCommand(const std::string& method, const std::string& paramsJson, std::string& responseJson, int retries)
{
    if (m_ip.empty() || m_token.size() != 16) return false;
    if (!InitSocket()) return false;

    sockaddr_in targetAddr{};
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(MIIO_DEFAULT_PORT);
    inet_pton(AF_INET, m_ip.c_str(), &targetAddr.sin_addr);

    for (int attempt = 0; attempt < retries; ++attempt)
    {
        // 检查是否需要重新握手同步时间戳
        auto now = std::chrono::steady_clock::now();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - m_localStampTime).count();
        if (!m_handshakeOk || elapsedSec > MIIO_STAMP_MAX_AGE_SEC)
        {
            if (!DoHandshake(2))
            {
                Sleep(150);
                continue;
            }
        }

        uint32_t currentStamp = m_serverStamp + static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_localStampTime).count());
        m_msgId++;

        // 构造 JSON-RPC 载荷
        std::stringstream payloadSs;
        payloadSs << "{\"id\":" << m_msgId << ",\"method\":\"" << method << "\",\"params\":" << paramsJson << "}";
        std::string payloadStr = payloadSs.str();
        std::vector<uint8_t> plaintext(payloadStr.begin(), payloadStr.end());

        // 加密
        std::vector<uint8_t> encrypted;
        if (!MiioCrypto::Aes128CbcEncrypt(plaintext, m_key, m_iv, encrypted))
        {
            return false;
        }

        uint16_t length = static_cast<uint16_t>(MIIO_HEADER_LEN + encrypted.size());
        uint8_t header[16]{};
        header[0] = 0x21;
        header[1] = 0x31;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        header[4] = 0; header[5] = 0; header[6] = 0; header[7] = 0;
        header[8] = (m_deviceId >> 24) & 0xFF;
        header[9] = (m_deviceId >> 16) & 0xFF;
        header[10] = (m_deviceId >> 8) & 0xFF;
        header[11] = m_deviceId & 0xFF;
        header[12] = (currentStamp >> 24) & 0xFF;
        header[13] = (currentStamp >> 16) & 0xFF;
        header[14] = (currentStamp >> 8) & 0xFF;
        header[15] = currentStamp & 0xFF;

        // 计算 Checksum: MD5(Header + Token + Encrypted)
        std::vector<uint8_t> checkSource;
        checkSource.reserve(16 + m_token.size() + encrypted.size());
        checkSource.insert(checkSource.end(), header, header + 16);
        checkSource.insert(checkSource.end(), m_token.begin(), m_token.end());
        checkSource.insert(checkSource.end(), encrypted.begin(), encrypted.end());

        uint8_t checksum[16]{};
        MiioCrypto::ComputeMD5(checkSource.data(), checkSource.size(), checksum);

        // 拼接最终数据包
        std::vector<uint8_t> packet;
        packet.reserve(16 + 16 + encrypted.size());
        packet.insert(packet.end(), header, header + 16);
        packet.insert(packet.end(), checksum, checksum + 16);
        packet.insert(packet.end(), encrypted.begin(), encrypted.end());

        int sent = sendto((SOCKET)m_socket, (const char*)packet.data(), static_cast<int>(packet.size()), 0,
                          (sockaddr*)&targetAddr, sizeof(targetAddr));
        if (sent <= 0)
        {
            m_handshakeOk = false;
            Sleep(150);
            continue;
        }

        uint8_t respBuffer[4096];
        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);
        int received = recvfrom((SOCKET)m_socket, (char*)respBuffer, sizeof(respBuffer), 0, (sockaddr*)&fromAddr, &fromLen);

        if (received > static_cast<int>(MIIO_HEADER_LEN) && respBuffer[0] == 0x21 && respBuffer[1] == 0x31)
        {
            std::vector<uint8_t> respEncrypted(respBuffer + MIIO_HEADER_LEN, respBuffer + received);
            std::vector<uint8_t> respPlain;
            if (MiioCrypto::Aes128CbcDecrypt(respEncrypted, m_key, m_iv, respPlain))
            {
                responseJson.assign(respPlain.begin(), respPlain.end());
                return true;
            }
        }

        m_handshakeOk = false;
        Sleep(150);
    }
    return false;
}

bool MiioClient::TestConnection(std::string& outMsg, double& outPower)
{
    if (m_ip.empty())
    {
        outMsg = "错误: 设备 IP 地址为空";
        return false;
    }
    if (m_tokenHex.length() != 32)
    {
        outMsg = "错误: Token 长度必须为 32 位十六进制字符";
        return false;
    }

    if (!DoHandshake(2))
    {
        outMsg = "连接失败: 无法与插座握手，请检查局域网连接或 IP 地址是否正确";
        return false;
    }

    std::string resp;
    if (!SendRawCommand("get_prop", "[\"power\",\"power_consume_rate\",\"current\",\"temperature\"]", resp))
    {
        outMsg = "连接失败: 握手成功但发送指令超时，请核对 Token 是否正确";
        return false;
    }

    auto items = ParseResultArray(resp);
    if (!items.empty() && items[0] != "null")
    {
        double p = 0.0;
        if (items.size() > 1 && items[1] != "null")
        {
            p = std::strtod(items[1].c_str(), nullptr);
        }
        outPower = p;
        std::stringstream ss;
        ss << "连接成功! 当前插座状态: " << items[0] << " | 实时功率: " << p << " W";
        outMsg = ss.str();
        return true;
    }

    outMsg = "获取数据失败，返回格式异常: " + resp;
    return false;
}

bool MiioClient::FetchData(PlugData& data)
{
    std::string resp;
    bool success = SendRawCommand("get_prop", "[\"power\",\"power_consume_rate\",\"current\",\"temperature\",\"wifi_led\"]", resp, 2);
    
    // 获取当前时间字符串
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm tmNow;
    localtime_s(&tmNow, &now_c);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmNow);
    data.update_time_str = timeBuf;

    if (!success)
    {
        data.online = false;
        data.error_msg = "通信超时或网络中断";
        return false;
    }

    auto items = ParseResultArray(resp);
    if (items.size() >= 4)
    {
        data.online = true;
        data.is_on = (items[0] == "on");
        data.power_w = (items[1] != "null" && !items[1].empty()) ? std::strtod(items[1].c_str(), nullptr) : 0.0;
        data.current_a = (items[2] != "null" && !items[2].empty()) ? std::strtod(items[2].c_str(), nullptr) : 0.0;
        data.temperature_c = (items[3] != "null" && !items[3].empty()) ? std::strtod(items[3].c_str(), nullptr) : 0.0;
        data.error_msg = "";
        return true;
    }

    data.online = false;
    data.error_msg = "返回数据解析失败";
    return false;
}

bool MiioClient::SetPower(bool turnOn)
{
    std::string resp;
    std::string params = turnOn ? "[\"on\"]" : "[\"off\"]";
    return SendRawCommand("set_power", params, resp, 2);
}
