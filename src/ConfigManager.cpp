#include "ConfigManager.h"
#include "CryptoHelper.h"
#include <vector>

std::wstring ConfigManager::m_iniPath;

void ConfigManager::Init(HMODULE hModule)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring fullPath(path);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
    {
        m_iniPath = fullPath.substr(0, lastSlash + 1) + L"XiaomiPlugPlugin.ini";
    }
    else
    {
        m_iniPath = L".\\XiaomiPlugPlugin.ini";
    }
}

const std::wstring& ConfigManager::GetIniPath()
{
    return m_iniPath;
}

void ConfigManager::LoadConfig(PluginConfig& config)
{
    wchar_t buf[256];

    // 设备名称
    GetPrivateProfileStringW(L"Device", L"DeviceName", L"小米智能插座", buf, 256, m_iniPath.c_str());
    config.device_name = buf;

    // IP
    GetPrivateProfileStringW(L"Device", L"IP", L"192.168.1.100", buf, 256, m_iniPath.c_str());
    config.ip = MiioCrypto::WideToUtf8(buf);

    // Token
    GetPrivateProfileStringW(L"Device", L"Token", L"00000000000000000000000000000000", buf, 256, m_iniPath.c_str());
    config.token = MiioCrypto::WideToUtf8(buf);

    // 刷新频率 (秒)
    config.interval_sec = GetPrivateProfileIntW(L"Settings", L"IntervalSec", 2, m_iniPath.c_str());
    if (config.interval_sec < 1) config.interval_sec = 1;
    if (config.interval_sec > 60) config.interval_sec = 60;

    // 单位与小数位
    config.power_unit = GetPrivateProfileIntW(L"Display", L"PowerUnit", 0, m_iniPath.c_str());
    config.decimal_places = GetPrivateProfileIntW(L"Display", L"DecimalPlaces", 1, m_iniPath.c_str());

    // 标签
    GetPrivateProfileStringW(L"Display", L"PowerLabel", L"P: ", buf, 256, m_iniPath.c_str());
    config.power_label = buf;
    GetPrivateProfileStringW(L"Display", L"CurrentLabel", L"I: ", buf, 256, m_iniPath.c_str());
    config.current_label = buf;
    GetPrivateProfileStringW(L"Display", L"TempLabel", L"T: ", buf, 256, m_iniPath.c_str());
    config.temp_label = buf;

    // 阈值与事件
    GetPrivateProfileStringW(L"Advanced", L"WarnThreshold", L"0.0", buf, 256, m_iniPath.c_str());
    config.warn_threshold_w = _wtof(buf);
    config.double_click_action = GetPrivateProfileIntW(L"Advanced", L"DoubleClickAction", 0, m_iniPath.c_str());
}

void ConfigManager::SaveConfig(const PluginConfig& config)
{
    // 设备名称
    WritePrivateProfileStringW(L"Device", L"DeviceName", config.device_name.c_str(), m_iniPath.c_str());

    // IP 与 Token
    std::wstring wIp = MiioCrypto::Utf8ToWide(config.ip);
    WritePrivateProfileStringW(L"Device", L"IP", wIp.c_str(), m_iniPath.c_str());

    std::wstring wToken = MiioCrypto::Utf8ToWide(config.token);
    WritePrivateProfileStringW(L"Device", L"Token", wToken.c_str(), m_iniPath.c_str());

    // 刷新频率
    WritePrivateProfileStringW(L"Settings", L"IntervalSec", std::to_wstring(config.interval_sec).c_str(), m_iniPath.c_str());

    // 显示设置
    WritePrivateProfileStringW(L"Display", L"PowerUnit", std::to_wstring(config.power_unit).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Display", L"DecimalPlaces", std::to_wstring(config.decimal_places).c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Display", L"PowerLabel", config.power_label.c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Display", L"CurrentLabel", config.current_label.c_str(), m_iniPath.c_str());
    WritePrivateProfileStringW(L"Display", L"TempLabel", config.temp_label.c_str(), m_iniPath.c_str());

    // 高级设置
    wchar_t wBuf[256];
    swprintf_s(wBuf, 256, L"%.1f", config.warn_threshold_w);
    WritePrivateProfileStringW(L"Advanced", L"WarnThreshold", wBuf, m_iniPath.c_str());
    WritePrivateProfileStringW(L"Advanced", L"DoubleClickAction", std::to_wstring(config.double_click_action).c_str(), m_iniPath.c_str());
}
