import argparse
import time
import sys

# 确保在 Windows 控制台下支持 UTF-8 输出
if sys.platform.startswith('win'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

from miio_device import XiaomiPlug

DEFAULT_IP = "192.168.1.100"
DEFAULT_TOKEN = "00000000000000000000000000000000"


def parse_args():
    parser = argparse.ArgumentParser(description="持续监控小米智能插座实时功率与统计用电量")
    parser.add_argument("--ip", default=DEFAULT_IP, help="设备局域网 IP 地址 (默认: %(default)s)")
    parser.add_argument("--token", default=DEFAULT_TOKEN, help="设备 Token (默认: %(default)s)")
    parser.add_argument("--interval", type=float, default=2.0, help="采样刷新间隔秒数 (默认: %(default)s 秒)")
    parser.add_argument("--count", type=int, default=0, help="采样次数 (0 为无限持续监控，默认: 0)")
    return parser.parse_args()


def main():
    args = parse_args()
    plug = XiaomiPlug(args.ip, args.token)

    print(f"[*] 正在初始化连接插座 ({args.ip})...")
    try:
        info = plug.get_info()
        print(f"[+] 连接成功! 型号: {info.get('model', '未知')} | 固件: {info.get('fw_ver', '未知')}")
    except Exception as e:
        print(f"[-] 连接失败: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"[*] 开始持续监控 (采样间隔: {args.interval}s，按 Ctrl+C 可停止监控并查看统计报告)")
    print("-" * 75)
    print(f"{'时间':^19} | {'开关':^6} | {'实时功率 (W)':^12} | {'电流 (A)':^8} | {'温度 (℃)':^8}")
    print("-" * 75)

    samples = []
    start_time = time.time()
    last_time = start_time
    total_energy_joules = 0.0  # 累计能量（焦耳 J）
    iterations = 0

    try:
        while True:
            current_loop_time = time.time()
            try:
                data = plug.get_power_status()
                now_str = data["timestamp"]
                state_str = "ON" if data["is_on"] else "OFF"
                p_w = data["power_w"]
                curr_a = data["current_a"]
                temp_c = data["temperature_c"]

                # 累加电量计算 (能量 = 功率 * 时间跨度)
                time_delta = current_loop_time - last_time
                if iterations > 0:
                    total_energy_joules += p_w * time_delta
                last_time = current_loop_time

                samples.append(p_w)
                iterations += 1

                print(f"{now_str} | {state_str:^6} | {p_w:^12.2f} | {curr_a:^8.2f} | {temp_c:^8.1f}")

                if args.count > 0 and iterations >= args.count:
                    break

            except Exception as read_err:
                print(f"[{time.strftime('%H:%M:%S')}] 读取异常: {read_err}")

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n\n[*] 监控被用户终止。")

    total_duration = time.time() - start_time
    kwh = total_energy_joules / 3600000.0  # 1 kWh = 3.6e6 焦耳

    if samples:
        avg_w = sum(samples) / len(samples)
        max_w = max(samples)
        min_w = min(samples)
        print("\n" + "=" * 55)
        print("          📊 本次用电监控统计报告 📊")
        print("=" * 55)
        print(f" 监控总时长   : {total_duration:.1f} 秒 ({total_duration / 60:.2f} 分钟)")
        print(f" 采样点总数   : {len(samples)} 次")
        print(f" 最大实时功率 : {max_w:.2f} W")
        print(f" 最小实时功率 : {min_w:.2f} W")
        print(f" 平均实时功率 : {avg_w:.2f} W")
        print(f" 累计消耗电量 : {kwh:.5f} 度 (kWh)")
        print("=" * 55 + "\n")


if __name__ == "__main__":
    main()
