# MiPlugin-Traff - 生产发布包说明 (v1.1.2)

## 📌 版本信息
* **插件名称**：MiPlugin-Traff
* **作　　者**：Simon Chen
* **版　　本**：v1.1.2 (Release)
* **版　　权**：Sim-OpenSource
* **构建特性**：MSVC `/O2` `/GL` `/LTCG` `/OPT:REF` `/MT` 纯静态单文件

---

## 📂 文件清单
* `x64\XiaomiPlugPlugin.dll`：64 位正式发布版（推荐 64 位 TrafficMonitor 使用）
* `x86\XiaomiPlugPlugin.dll`：32 位正式发布版（用于 32 位 TrafficMonitor）

---

## 🚀 安装步骤
1. 将 `x64\XiaomiPlugPlugin.dll` 复制到 TrafficMonitor 安装目录下的 `plugins` 文件夹。
2. 重启 TrafficMonitor。
3. 右键任务栏 -> **【插件管理】** -> 勾选 **【MiPlugin-Traff】**。
4. 右键任务栏 -> **【显示设置】** -> **【项目显示】** -> 勾选 **【小米插座实时功率】**。
5. 右键项目 -> **【选项】** 打开配置窗口，配置设备名称、IP 地址与 Token。
