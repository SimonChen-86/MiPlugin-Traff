import argparse
import json
import sys
import io

# 确保在 Windows 控制台下支持 UTF-8 输出
if sys.platform.startswith('win'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

from miio_device import XiaomiPlug

# 默认设备配置
DEFAULT_IP = "192.168.1.100"
DEFAULT_TOKEN = "00000000000000000000000000000000"


def parse_args():
    parser = argparse.ArgumentParser(description="获取小米智能插座实时功率及状态")
    parser.add_argument("--ip", default=DEFAULT_IP, help="设备局域网 IP 地址 (默认: %(default)s)")
    parser.add_argument("--token", default=DEFAULT_TOKEN, help="设备 Token (默认: %(default)s)")
    parser.add_argument("--json", action="store_true", help="以 JSON 格式输出结果")
    parser.add_argument("--info", action="store_true", help="获取插座详细设备与网络信息")
    return parser.parse_args()


def main():
    args = parse_args()
    plug = XiaomiPlug(args.ip, args.token)

    try:
        if args.info:
            info = plug.get_info()
            if args.json:
                print(json.dumps(info, indent=2, ensure_ascii=False))
            else:
                print("=" * 45)
                print("      小米智能插座/插线板 详细信息")
                print("=" * 45)
                print(f"设备型号 (Model)   : {info.get('model', '未知')}")
                print(f"硬件版本 (HW Ver)  : {info.get('hw_ver', '未知')}")
                print(f"固件版本 (FW Ver)  : {info.get('fw_ver', '未知')}")
                print(f"MAC 地址 (MAC)     : {info.get('mac', '未知')}")
                print(f"当前 IP 地址       : {info.get('netif', {}).get('localIp', args.ip)}")
                print(f"WiFi SSID          : {info.get('ap', {}).get('ssid', '未知')}")
                print(f"WiFi 信号强度(RSSI): {info.get('ap', {}).get('rssi', '未知')} dBm")
                print(f"运行时间 (Life)    : {info.get('life', 0)} 秒")
                print("=" * 45)
            return

        status = plug.get_power_status()

        if args.json:
            print(json.dumps(status, indent=2, ensure_ascii=False))
        else:
            state_text = "🟢 开启 (ON)" if status["is_on"] else "🔴 关闭 (OFF)"
            print("\n" + "=" * 45)
            print("       ⚡ 小米智能插座 实时电力监测 ⚡")
            print("=" * 45)
            print(f" 采集时间    : {status['timestamp']}")
            print(f" 设备 IP     : {args.ip}")
            print(f" 开关状态    : {state_text}")
            print(f" 实时功率    : \033[1;32m{status['power_w']:.2f} W\033[0m")
            print(f" 实时电流    : {status['current_a']:.2f} A")
            print(f" 内部温度    : {status['temperature_c']:.1f} ℃")
            print(f" WiFi指示灯  : {status['wifi_led']}")
            print("=" * 45 + "\n")

    except Exception as e:
        print(f"[错误] 无法获取设备数据: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
