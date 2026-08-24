#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "PluginInterface.h"
#include "ConfigManager.h"
#include "MiioClient.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>

// 实时功率监控项
class PowerPluginItem : public IPluginItem
{
public:
    virtual const wchar_t* GetItemName() const override { return L"小米插座实时功率"; }
    virtual const wchar_t* GetItemId() const override { return L"XiaomiPlug_Power"; }
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override { return L"999.9 W"; }
    virtual bool IsCustomDraw() const override { return false; }
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
};

// 实时电流监控项
class CurrentPluginItem : public IPluginItem
{
public:
    virtual const wchar_t* GetItemName() const override { return L"小米插座工作电流"; }
    virtual const wchar_t* GetItemId() const override { return L"XiaomiPlug_Current"; }
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override { return L"99.99 A"; }
    virtual bool IsCustomDraw() const override { return false; }
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
};

// 内部温度监控项
class TempPluginItem : public IPluginItem
{
public:
    virtual const wchar_t* GetItemName() const override { return L"小米插座内部温度"; }
    virtual const wchar_t* GetItemId() const override { return L"XiaomiPlug_Temp"; }
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override { return L"99.9 ℃"; }
    virtual bool IsCustomDraw() const override { return false; }
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
};

// TrafficMonitor 小米插座主插件类
class XiaomiPlugPlugin : public ITMPlugin
{
public:
    static XiaomiPlugPlugin& Instance();

    virtual int GetAPIVersion() const override { return 1; }
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;

    void OnInitialize();
    void OnDisable();

    // 格式化文本获取
    const wchar_t* GetPowerValueFormatted() const;
    const wchar_t* GetCurrentValueFormatted() const;
    const wchar_t* GetTempValueFormatted() const;
    const wchar_t* GetPowerLabel() const { return m_config.power_label.c_str(); }
    const wchar_t* GetCurrentLabel() const { return m_config.current_label.c_str(); }
    const wchar_t* GetTempLabel() const { return m_config.temp_label.c_str(); }

    void HandleDoubleClick(HWND hWnd);
    void TogglePowerAsync();

private:
    XiaomiPlugPlugin();
    ~XiaomiPlugPlugin();

    void StartWorkerThread();
    void StopWorkerThread();
    void WorkerLoop();

    PluginConfig m_config;
    MiioClient m_client;
    PlugData m_cachedData;
    mutable std::mutex m_dataMutex;

    std::thread m_workerThread;
    std::atomic<bool> m_running{ false };
    std::condition_variable m_cv;
    std::mutex m_cvMutex;

    PowerPluginItem m_powerItem;
    CurrentPluginItem m_currentItem;
    TempPluginItem m_tempItem;

    mutable std::wstring m_powerValStr;
    mutable std::wstring m_currentValStr;
    mutable std::wstring m_tempValStr;
    mutable std::wstring m_tooltipStr;
};
