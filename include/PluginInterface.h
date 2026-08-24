#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

// TrafficMonitor 插件显示项目接口
class IPluginItem
{
public:
    enum MouseEventType
    {
        MT_LCLICKED,
        MT_RCLICKED,
        MT_DBCLICKED
    };

    // 显示项目名称（如“实时功率”）
    virtual const wchar_t* GetItemName() const = 0;

    // 显示项目唯一 ID（如“XiaomiPlug_Power”）
    virtual const wchar_t* GetItemId() const = 0;

    // 显示项目标签文本（如“P: ”）
    virtual const wchar_t* GetItemLableText() const = 0;

    // 显示项目当前数值文本（如“53.4 W”）
    virtual const wchar_t* GetItemValueText() const = 0;

    // 显示项目示例数值文本（用于计算布局宽度，如“999.9 W”）
    virtual const wchar_t* GetItemValueSampleText() const = 0;

    // 是否由插件自定义绘制（返回 false 由主程序默认渲染）
    virtual bool IsCustomDraw() const = 0;

    // 自定义绘制时的项目宽度
    virtual int GetItemWidth() const { return 0; }

    // 自定义绘制时的项目宽度扩展
    virtual int GetItemWidthEx(void* hDC) const { return GetItemWidth(); }

    // 自定义绘制接口
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) {}

    // 鼠标事件响应接口
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) { return 0; }
};

// TrafficMonitor 插件主接口 (虚函数顺序与官方规范严格 1:1 对齐)
class ITMPlugin
{
public:
    enum PluginInfoIndex
    {
        TMI_NAME,         // 0: 插件名称
        TMI_DESCRIPTION,  // 1: 插件描述
        TMI_AUTHOR,       // 2: 插件作者
        TMI_COPYRIGHT,    // 3: 版权信息
        TMI_VERSION,      // 4: 插件版本
        TMI_URL           // 5: 网址/主页
    };

    enum OptionReturn
    {
        OR_OPTION_UNCHANGED,   // 配置未改变
        OR_OPTION_CHANGED,     // 配置已改变并保存
        OR_OPTION_NOT_PROVIDED // 插件未提供设置对话框
    };

    enum ExtendedInfoIndex
    {
        EI_LABEL_TEXT_COLOR,
        EI_VALUE_TEXT_COLOR,
        EI_TASKBAR_BACKGROUND_COLOR
    };

    // 虚函数 1 (0x00): 返回 API 版本
    virtual int GetAPIVersion() const { return 1; }

    // 虚函数 2 (0x08): 获取显示项目
    virtual IPluginItem* GetItem(int index) = 0;

    // 虚函数 3 (0x10): 数据刷新通知
    virtual void DataRequired() = 0;

    // 虚函数 4 (0x18): 显示插件设置选项对话框 (主程序点击【选项】按钮触发)
    virtual OptionReturn ShowOptionsDialog(void* hParent) { return OR_OPTION_NOT_PROVIDED; }

    // 虚函数 5 (0x20): 获取插件元数据信息 (TMI_NAME ~ TMI_VERSION)
    virtual const wchar_t* GetInfo(PluginInfoIndex index) = 0;

    // 虚函数 6 (0x28): 接收主程序监控数据推送
    virtual void OnMonitorInfo(const void* pMonitorInfo) {}

    // 虚函数 7 (0x30): 鼠标悬停时的详细提示信息 (Tooltip)
    virtual const wchar_t* GetTooltipInfo() { return L""; }

    // 虚函数 8 (0x38): 接收主程序扩展信息通知
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {}
};

#ifdef __cplusplus
extern "C" {
#endif

// 导出获取插件实例的函数
__declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
