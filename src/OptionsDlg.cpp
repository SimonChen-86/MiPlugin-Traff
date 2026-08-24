#include "OptionsDlg.h"
#include "CryptoHelper.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <thread>

#pragma comment(lib, "comctl32.lib")

// 控件 ID 定义
#define IDC_EDIT_NAME       1001
#define IDC_EDIT_IP         1002
#define IDC_EDIT_TOKEN      1003
#define IDC_EDIT_INTERVAL   1004
#define IDC_COMBO_UNIT      1005
#define IDC_COMBO_DECIMAL   1006
#define IDC_EDIT_WARN       1007
#define IDC_BTN_TEST        1008
#define IDC_BTN_OK          IDOK
#define IDC_BTN_CANCEL      IDCANCEL

// 自定义异步消息定义
#define WM_APP_TEST_DONE    (WM_USER + 101)

PluginConfig* OptionsDlg::s_pConfig = nullptr;
MiioClient* OptionsDlg::s_pClient = nullptr;
bool OptionsDlg::s_saved = false;
bool OptionsDlg::s_runningModal = false;
HFONT OptionsDlg::s_hFont = nullptr;

bool OptionsDlg::Show(HWND hParent, PluginConfig& config, MiioClient& client)
{
    s_pConfig = &config;
    s_pClient = &client;
    s_saved = false;
    s_runningModal = true;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const wchar_t* CLASS_NAME = L"TrafficMonitor_XiaomiPlug_OptionsDlg";

    // 仅注册一次窗口类
    WNDCLASSEXW wcExisting = { sizeof(WNDCLASSEXW) };
    if (!GetClassInfoExW(hInstance, CLASS_NAME, &wcExisting))
    {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = (WNDPROC)DialogProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = CLASS_NAME;
        RegisterClassExW(&wc);
    }

    // 计算居中坐标
    int dlgW = 460;
    int dlgH = 390;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;
    if (hParent && IsWindow(hParent))
    {
        RECT rcParent;
        GetWindowRect(hParent, &rcParent);
        posX = rcParent.left + ((rcParent.right - rcParent.left) - dlgW) / 2;
        posY = rcParent.top + ((rcParent.bottom - rcParent.top) - dlgH) / 2;
    }

    HWND hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        CLASS_NAME,
        L"小米智能插座插件 设置",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        hParent, nullptr, hInstance, nullptr
    );

    if (!hWnd) return false;

    if (hParent && IsWindow(hParent)) EnableWindow(hParent, FALSE);

    MSG msg;
    while (s_runningModal && IsWindow(hWnd))
    {
        if (GetMessageW(&msg, nullptr, 0, 0))
        {
            if (msg.message == WM_QUIT)
            {
                // 如果是整个系统的退出消息，转交后终止模态循环
                PostQuitMessage((int)msg.wParam);
                break;
            }
            if (!IsDialogMessageW(hWnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        else
        {
            break;
        }
    }

    if (hParent && IsWindow(hParent))
    {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    if (s_hFont)
    {
        DeleteObject(s_hFont);
        s_hFont = nullptr;
    }

    return s_saved;
}

void OptionsDlg::CreateControls(HWND hWnd)
{
    // 创建通用字体 (微软雅黑)
    s_hFont = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI"
    );

    auto CreateLabel = [&](const wchar_t* text, int x, int y, int w, int h) {
        HWND hCtrl = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h, hWnd, nullptr, nullptr, nullptr);
        SendMessageW(hCtrl, WM_SETFONT, (WPARAM)s_hFont, TRUE);
        return hCtrl;
    };

    auto CreateEdit = [&](int id, int x, int y, int w, int h) {
        HWND hCtrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w, h, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
        SendMessageW(hCtrl, WM_SETFONT, (WPARAM)s_hFont, TRUE);
        return hCtrl;
    };

    auto CreateButton = [&](const wchar_t* text, int id, int x, int y, int w, int h, bool isDef = false) {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
        if (isDef) style |= BS_DEFPUSHBUTTON;
        HWND hCtrl = CreateWindowW(L"BUTTON", text, style, x, y, w, h, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
        SendMessageW(hCtrl, WM_SETFONT, (WPARAM)s_hFont, TRUE);
        return hCtrl;
    };

    int startY = 18;
    int rowH = 32;
    int labelW = 100;
    int editX = 120;
    int editW = 300;

    // 1. 设备名称
    CreateLabel(L"设备名称:", 20, startY + 4, labelW, 20);
    CreateEdit(IDC_EDIT_NAME, editX, startY, editW, 24);

    // 2. IP 地址
    startY += rowH;
    CreateLabel(L"设备 IP 地址:", 20, startY + 4, labelW, 20);
    CreateEdit(IDC_EDIT_IP, editX, startY, editW - 90, 24);
    CreateButton(L"测试连接", IDC_BTN_TEST, editX + editW - 80, startY, 80, 24);

    // 3. Token
    startY += rowH;
    CreateLabel(L"设备 Token:", 20, startY + 4, labelW, 20);
    CreateEdit(IDC_EDIT_TOKEN, editX, startY, editW, 24);

    // 4. 刷新间隔
    startY += rowH;
    CreateLabel(L"刷新间隔 (秒):", 20, startY + 4, labelW, 20);
    CreateEdit(IDC_EDIT_INTERVAL, editX, startY, 80, 24);

    // 5. 功率单位
    startY += rowH;
    CreateLabel(L"功率显示单位:", 20, startY + 4, labelW, 20);
    HWND hComboUnit = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, editX, startY, 130, 150, hWnd, (HMENU)(INT_PTR)IDC_COMBO_UNIT, nullptr, nullptr);
    SendMessageW(hComboUnit, WM_SETFONT, (WPARAM)s_hFont, TRUE);
    SendMessageW(hComboUnit, CB_ADDSTRING, 0, (LPARAM)L"瓦特 (W)");
    SendMessageW(hComboUnit, CB_ADDSTRING, 0, (LPARAM)L"千瓦 (kW)");
    SendMessageW(hComboUnit, CB_ADDSTRING, 0, (LPARAM)L"纯数字 (无单位)");

    // 6. 小数位数
    startY += rowH;
    CreateLabel(L"保留小数位数:", 20, startY + 4, labelW, 20);
    HWND hComboDec = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, editX, startY, 130, 150, hWnd, (HMENU)(INT_PTR)IDC_COMBO_DECIMAL, nullptr, nullptr);
    SendMessageW(hComboDec, WM_SETFONT, (WPARAM)s_hFont, TRUE);
    SendMessageW(hComboDec, CB_ADDSTRING, 0, (LPARAM)L"0 位 (整数)");
    SendMessageW(hComboDec, CB_ADDSTRING, 0, (LPARAM)L"1 位 (如 53.4)");
    SendMessageW(hComboDec, CB_ADDSTRING, 0, (LPARAM)L"2 位 (如 53.40)");

    // 7. 高功耗告警阈值
    startY += rowH;
    CreateLabel(L"告警功率阈值(W):", 20, startY + 4, labelW + 10, 20);
    CreateEdit(IDC_EDIT_WARN, editX, startY, 80, 24);
    CreateLabel(L"(设为 0 表示不启用过载高亮)", editX + 90, startY + 4, 200, 20);

    // 底部按钮
    startY += 50;
    CreateButton(L"确定 (Save)", IDC_BTN_OK, 240, startY, 90, 28, true);
    CreateButton(L"取消 (Cancel)", IDC_BTN_CANCEL, 340, startY, 80, 28, false);
}

void OptionsDlg::LoadDataToControls(HWND hWnd)
{
    if (!s_pConfig) return;

    SetDlgItemTextW(hWnd, IDC_EDIT_NAME, s_pConfig->device_name.c_str());

    std::wstring wIp = MiioCrypto::Utf8ToWide(s_pConfig->ip);
    SetDlgItemTextW(hWnd, IDC_EDIT_IP, wIp.c_str());

    std::wstring wToken = MiioCrypto::Utf8ToWide(s_pConfig->token);
    SetDlgItemTextW(hWnd, IDC_EDIT_TOKEN, wToken.c_str());

    SetDlgItemInt(hWnd, IDC_EDIT_INTERVAL, s_pConfig->interval_sec, FALSE);

    SendDlgItemMessageW(hWnd, IDC_COMBO_UNIT, CB_SETCURSEL, s_pConfig->power_unit, 0);
    SendDlgItemMessageW(hWnd, IDC_COMBO_DECIMAL, CB_SETCURSEL, s_pConfig->decimal_places, 0);

    wchar_t wBuf[256];
    swprintf_s(wBuf, 256, L"%.1f", s_pConfig->warn_threshold_w);
    SetDlgItemTextW(hWnd, IDC_EDIT_WARN, wBuf);
}

bool OptionsDlg::SaveDataFromControls(HWND hWnd)
{
    if (!s_pConfig) return false;

    wchar_t wBuf[256];

    // 设备名
    GetDlgItemTextW(hWnd, IDC_EDIT_NAME, wBuf, 256);
    s_pConfig->device_name = wBuf;

    // IP
    GetDlgItemTextW(hWnd, IDC_EDIT_IP, wBuf, 256);
    s_pConfig->ip = MiioCrypto::WideToUtf8(wBuf);

    // Token
    GetDlgItemTextW(hWnd, IDC_EDIT_TOKEN, wBuf, 256);
    std::string tokenStr = MiioCrypto::WideToUtf8(wBuf);
    if (tokenStr.length() != 32)
    {
        MessageBoxW(hWnd, L"Token 格式错误！请输入 32 位十六进制 Token 字符串。", L"提示", MB_OK | MB_ICONWARNING);
        return false;
    }
    s_pConfig->token = tokenStr;

    // 刷新间隔
    int interval = GetDlgItemInt(hWnd, IDC_EDIT_INTERVAL, nullptr, FALSE);
    if (interval < 1) interval = 1;
    if (interval > 60) interval = 60;
    s_pConfig->interval_sec = interval;

    // 单位与小数位
    s_pConfig->power_unit = (int)SendDlgItemMessageW(hWnd, IDC_COMBO_UNIT, CB_GETCURSEL, 0, 0);
    s_pConfig->decimal_places = (int)SendDlgItemMessageW(hWnd, IDC_COMBO_DECIMAL, CB_GETCURSEL, 0, 0);

    // 阈值
    GetDlgItemTextW(hWnd, IDC_EDIT_WARN, wBuf, 256);
    s_pConfig->warn_threshold_w = _wtof(wBuf);

    return true;
}

void OptionsDlg::DoTestConnection(HWND hWnd)
{
    wchar_t wIp[128], wToken[128];
    GetDlgItemTextW(hWnd, IDC_EDIT_IP, wIp, 128);
    GetDlgItemTextW(hWnd, IDC_EDIT_TOKEN, wToken, 128);

    std::string ipA = MiioCrypto::WideToUtf8(wIp);
    std::string tokenA = MiioCrypto::WideToUtf8(wToken);

    // 禁用测试按钮，防止重复点击
    HWND hBtnTest = GetDlgItem(hWnd, IDC_BTN_TEST);
    EnableWindow(hBtnTest, FALSE);
    SetWindowTextW(hBtnTest, L"测试中...");

    // 启动独立工作线程执行测试，彻底消除主 UI 假死
    std::thread([hWnd, ipA, tokenA]() {
        MiioClient tester;
        tester.SetTarget(ipA, tokenA);

        std::string msg;
        double power = 0.0;
        bool ok = tester.TestConnection(msg, power);

        std::wstring* pMsg = new std::wstring(MiioCrypto::Utf8ToWide(msg));
        PostMessageW(hWnd, WM_APP_TEST_DONE, ok ? 1 : 0, (LPARAM)pMsg);
    }).detach();
}

INT_PTR CALLBACK OptionsDlg::DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        CreateControls(hWnd);
        LoadDataToControls(hWnd);
        return 0;

    case WM_APP_TEST_DONE:
    {
        // 恢复测试连接按钮状态
        HWND hBtnTest = GetDlgItem(hWnd, IDC_BTN_TEST);
        SetWindowTextW(hBtnTest, L"测试连接");
        EnableWindow(hBtnTest, TRUE);

        bool ok = (wParam != 0);
        std::wstring* pMsg = (std::wstring*)lParam;
        if (pMsg)
        {
            if (ok)
            {
                MessageBoxW(hWnd, pMsg->c_str(), L"测试连接成功", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(hWnd, pMsg->c_str(), L"测试连接失败", MB_OK | MB_ICONERROR);
            }
            delete pMsg;
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_TEST:
            DoTestConnection(hWnd);
            return TRUE;

        case IDC_BTN_OK:
            if (SaveDataFromControls(hWnd))
            {
                s_saved = true;
                s_runningModal = false;
                DestroyWindow(hWnd);
            }
            return TRUE;

        case IDC_BTN_CANCEL:
            s_saved = false;
            s_runningModal = false;
            DestroyWindow(hWnd);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        s_saved = false;
        s_runningModal = false;
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        // 模态对话框销毁，绝不调用 PostQuitMessage
        s_runningModal = false;
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
