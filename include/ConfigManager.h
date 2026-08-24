#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>

struct PluginConfig
{
    std::wstring device_name = L"小米智能插座";
    std::string ip = "192.168.1.100";
    std::string token = "00000000000000000000000000000000";
    int interval_sec = 2;              // 采集刷新间隔 (秒)
    int power_unit = 0;                // 0: W, 1: kW, 2: 纯数字
    int decimal_places = 1;            // 保留小数位数 (0, 1, 2)
    std::wstring power_label = L"P: "; // 功率标签
    std::wstring current_label = L"I: "; // 电流标签
    std::wstring temp_label = L"T: ";   // 温度标签
    double warn_threshold_w = 0.0;     // 报警功率阈值 (0 表示不报警)
    int double_click_action = 0;       // 0: 打开设置对话框, 1: 切换插座开关
};

class ConfigManager
{
public:
    static void Init(HMODULE hModule);
    static void LoadConfig(PluginConfig& config);
    static void SaveConfig(const PluginConfig& config);
    static const std::wstring& GetIniPath();

private:
    static std::wstring m_iniPath;
};
