#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "ConfigManager.h"
#include "MiioClient.h"

class OptionsDlg
{
public:
    static bool Show(HWND hParent, PluginConfig& config, MiioClient& client);

private:
    static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void CreateControls(HWND hWnd);
    static void LoadDataToControls(HWND hWnd);
    static bool SaveDataFromControls(HWND hWnd);
    static void DoTestConnection(HWND hWnd);

    static PluginConfig* s_pConfig;
    static MiioClient* s_pClient;
    static bool s_saved;
    static bool s_runningModal;
    static HFONT s_hFont;
};
