#include "XiaomiPlugPlugin.h"
#include "CryptoHelper.h"
#include "OptionsDlg.h"
#include <iomanip>
#include <sstream>

static HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    g_hModule = hModule;
    ConfigManager::Init(hModule);
  }
  return TRUE;
}

ITMPlugin *TMPluginGetInstance() { return &XiaomiPlugPlugin::Instance(); }

// -------------------------------------------------------------
// IPluginItem 实现
// -------------------------------------------------------------
const wchar_t *PowerPluginItem::GetItemLableText() const {
  return XiaomiPlugPlugin::Instance().GetPowerLabel();
}

const wchar_t *PowerPluginItem::GetItemValueText() const {
  return XiaomiPlugPlugin::Instance().GetPowerValueFormatted();
}

int PowerPluginItem::OnMouseEvent(MouseEventType type, int x, int y, void *hWnd,
                                  int flag) {
  if (type == MT_DBCLICKED) {
    XiaomiPlugPlugin::Instance().HandleDoubleClick((HWND)hWnd);
    return 1;
  }
  return 0;
}

const wchar_t *CurrentPluginItem::GetItemLableText() const {
  return XiaomiPlugPlugin::Instance().GetCurrentLabel();
}

const wchar_t *CurrentPluginItem::GetItemValueText() const {
  return XiaomiPlugPlugin::Instance().GetCurrentValueFormatted();
}

int CurrentPluginItem::OnMouseEvent(MouseEventType type, int x, int y,
                                    void *hWnd, int flag) {
  if (type == MT_DBCLICKED) {
    XiaomiPlugPlugin::Instance().HandleDoubleClick((HWND)hWnd);
    return 1;
  }
  return 0;
}

const wchar_t *TempPluginItem::GetItemLableText() const {
  return XiaomiPlugPlugin::Instance().GetTempLabel();
}

const wchar_t *TempPluginItem::GetItemValueText() const {
  return XiaomiPlugPlugin::Instance().GetTempValueFormatted();
}

int TempPluginItem::OnMouseEvent(MouseEventType type, int x, int y, void *hWnd,
                                 int flag) {
  if (type == MT_DBCLICKED) {
    XiaomiPlugPlugin::Instance().HandleDoubleClick((HWND)hWnd);
    return 1;
  }
  return 0;
}

// -------------------------------------------------------------
// XiaomiPlugPlugin 主插件实现
// -------------------------------------------------------------
XiaomiPlugPlugin &XiaomiPlugPlugin::Instance() {
  static XiaomiPlugPlugin s_instance;
  return s_instance;
}

XiaomiPlugPlugin::XiaomiPlugPlugin() {
  ConfigManager::LoadConfig(m_config);
  m_client.SetTarget(m_config.ip, m_config.token);
  StartWorkerThread();
}

XiaomiPlugPlugin::~XiaomiPlugPlugin() { StopWorkerThread(); }

void XiaomiPlugPlugin::OnInitialize() {
  ConfigManager::LoadConfig(m_config);
  m_client.SetTarget(m_config.ip, m_config.token);
  StartWorkerThread();
}

void XiaomiPlugPlugin::OnDisable() { StopWorkerThread(); }

IPluginItem *XiaomiPlugPlugin::GetItem(int index) {
  switch (index) {
  case 0:
    return &m_powerItem;
  case 1:
    return &m_currentItem;
  case 2:
    return &m_tempItem;
  default:
    return nullptr;
  }
}

void XiaomiPlugPlugin::DataRequired() {
  if (!m_running) {
    StartWorkerThread();
  }
}

const wchar_t *XiaomiPlugPlugin::GetInfo(PluginInfoIndex index) {
  switch (index) {
  case TMI_NAME:
    return L"MiPlugin-Traff";
  case TMI_DESCRIPTION:
    return L"实时监控局域网小米智能插座/插线板的功率、电流、温度和开关状态。";
  case TMI_AUTHOR:
    return L"Simon Chen";
  case TMI_COPYRIGHT:
    return L"Sim-OpenSource";
  case TMI_VERSION:
    return L"v1.1.2";
  case TMI_URL:
    return L"https://github.com/SimonChen-86/MiPlugin-Traff";
  default:
    return L"";
  }
}

ITMPlugin::OptionReturn XiaomiPlugPlugin::ShowOptionsDialog(void *hParent) {
  PluginConfig oldCfg = m_config;
  if (OptionsDlg::Show((HWND)hParent, m_config, m_client)) {
    ConfigManager::SaveConfig(m_config);
    m_client.SetTarget(m_config.ip, m_config.token);
    // 保存配置后立即唤醒工作线程刷新数据
    m_cv.notify_one();
    return OR_OPTION_CHANGED;
  }
  m_config = oldCfg;
  return OR_OPTION_UNCHANGED;
}

const wchar_t *XiaomiPlugPlugin::GetTooltipInfo() {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  std::wstringstream ss;
  ss << L"【" << m_config.device_name << L"】\n";

  if (!m_cachedData.online) {
    ss << L"状态: 🔴 离线 / 正在连接...\n";
    if (!m_cachedData.error_msg.empty()) {
      ss << L"提示: " << MiioCrypto::Utf8ToWide(m_cachedData.error_msg)
         << L"\n";
    }
    ss << L"IP: " << MiioCrypto::Utf8ToWide(m_config.ip);
  } else {
    ss << L"开关状态: "
       << (m_cachedData.is_on ? L"🟢 开启 (ON)" : L"🔴 关闭 (OFF)") << L"\n";
    ss << L"实时功率: " << std::fixed << std::setprecision(2)
       << m_cachedData.power_w << L" W\n";
    ss << L"工作电流: " << std::fixed << std::setprecision(2)
       << m_cachedData.current_a << L" A\n";
    ss << L"内部温度: " << std::fixed << std::setprecision(1)
       << m_cachedData.temperature_c << L" ℃\n";
    if (m_config.warn_threshold_w > 0.0 &&
        m_cachedData.power_w >= m_config.warn_threshold_w) {
      ss << L"⚠️ 告警: 当前功率已超过设定阈值 (" << m_config.warn_threshold_w
         << L" W)！\n";
    }
    ss << L"更新时间: " << MiioCrypto::Utf8ToWide(m_cachedData.update_time_str);
  }

  m_tooltipStr = ss.str();
  return m_tooltipStr.c_str();
}

const wchar_t *XiaomiPlugPlugin::GetPowerValueFormatted() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (!m_cachedData.online) {
    m_powerValStr = L"--";
    return m_powerValStr.c_str();
  }

  double power = m_cachedData.power_w;
  std::wstringstream ss;

  // 检查告警高亮
  if (m_config.warn_threshold_w > 0.0 && power >= m_config.warn_threshold_w) {
    ss << L"!";
  }

  if (m_config.power_unit == 1) // kW
  {
    double kw = power / 1000.0;
    ss << std::fixed << std::setprecision(m_config.decimal_places + 1) << kw
       << L" kW";
  } else if (m_config.power_unit == 2) // 纯数值
  {
    ss << std::fixed << std::setprecision(m_config.decimal_places) << power;
  } else // W (默认)
  {
    ss << std::fixed << std::setprecision(m_config.decimal_places) << power
       << L" W";
  }

  m_powerValStr = ss.str();
  return m_powerValStr.c_str();
}

const wchar_t *XiaomiPlugPlugin::GetCurrentValueFormatted() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (!m_cachedData.online) {
    m_currentValStr = L"--";
    return m_currentValStr.c_str();
  }

  std::wstringstream ss;
  ss << std::fixed << std::setprecision(2) << m_cachedData.current_a << L" A";
  m_currentValStr = ss.str();
  return m_currentValStr.c_str();
}

const wchar_t *XiaomiPlugPlugin::GetTempValueFormatted() const {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  if (!m_cachedData.online) {
    m_tempValStr = L"--";
    return m_tempValStr.c_str();
  }

  std::wstringstream ss;
  ss << std::fixed << std::setprecision(1) << m_cachedData.temperature_c
     << L" ℃";
  m_tempValStr = ss.str();
  return m_tempValStr.c_str();
}

void XiaomiPlugPlugin::TogglePowerAsync() {
  // 在独立后台线程中异步执行开关控制，彻底消除主 UI 假死卡死
  std::thread([this]() {
    bool targetState = false;
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      targetState = !m_cachedData.is_on;
    }

    if (m_client.SetPower(targetState)) {
      {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_cachedData.is_on = targetState;
      }
      // 操作成功后立即唤醒工作线程更新最新功率和状态
      m_cv.notify_one();
    }
  }).detach();
}

void XiaomiPlugPlugin::HandleDoubleClick(HWND hWnd) {
  if (m_config.double_click_action == 1) {
    // 异步切换插座电源开关 (非阻塞)
    TogglePowerAsync();
  } else {
    // 打开设置对话框
    ShowOptionsDialog(hWnd);
  }
}

void XiaomiPlugPlugin::StartWorkerThread() {
  StopWorkerThread();
  m_running = true;
  m_workerThread = std::thread(&XiaomiPlugPlugin::WorkerLoop, this);
}

void XiaomiPlugPlugin::StopWorkerThread() {
  m_running = false;
  m_cv.notify_all();
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
}

void XiaomiPlugPlugin::WorkerLoop() {
  while (m_running) {
    PlugData freshData;
    m_client.FetchData(freshData);

    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_cachedData = freshData;
    }

    // 使用 condition_variable 进行可即时唤醒的休眠
    std::unique_lock<std::mutex> cvLock(m_cvMutex);
    m_cv.wait_for(cvLock, std::chrono::seconds(m_config.interval_sec),
                  [this] { return !m_running; });
  }
}
