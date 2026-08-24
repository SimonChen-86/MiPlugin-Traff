"""
小米智能插座/插线板局域网 miIO 通信协议封装模块
支持免外部重量级依赖，仅依赖 cryptography
"""

import socket
import struct
import hashlib
import json
import time
from typing import Dict, Any, Optional, Tuple
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding


class MiioProtocol:
    """小米 miIO 局域网加密通信协议底层实现"""

    def __init__(self, ip: str, token_hex: str, port: int = 54321, timeout: float = 3.0):
        self.ip = ip
        self.port = port
        self.timeout = timeout
        self.token_hex = token_hex
        self.token = bytes.fromhex(token_hex)

        # 根据 Token 派生 AES 密钥与初始向量 IV
        self.key = hashlib.md5(self.token).digest()
        self.iv = hashlib.md5(self.key + self.token).digest()

        self.device_id = None
        self.server_stamp = None
        self.local_stamp_time = None
        self.msg_id = 1

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(self.timeout)

    def _encrypt(self, plaintext: bytes) -> bytes:
        """使用 AES-128-CBC 对数据进行 PKCS7 填充后加密"""
        padder = padding.PKCS7(128).padder()
        padded_data = padder.update(plaintext) + padder.finalize()
        cipher = Cipher(algorithms.AES(self.key), modes.CBC(self.iv))
        encryptor = cipher.encryptor()
        return encryptor.update(padded_data) + encryptor.finalize()

    def _decrypt(self, ciphertext: bytes) -> bytes:
        """使用 AES-128-CBC 解密并去除 PKCS7 填充"""
        cipher = Cipher(algorithms.AES(self.key), modes.CBC(self.iv))
        decryptor = cipher.decryptor()
        padded_data = decryptor.update(ciphertext) + decryptor.finalize()
        unpadder = padding.PKCS7(128).unpadder()
        return unpadder.update(padded_data) + unpadder.finalize()

    def handshake(self, retries: int = 3) -> Tuple[int, int]:
        """发送 32 字节 Hello 握手报文，同步设备 ID 与时间戳（支持自动重试）"""
        hello_packet = bytes.fromhex(
            "21310020ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        )
        last_err = None
        for attempt in range(retries):
            try:
                self.sock.sendto(hello_packet, (self.ip, self.port))
                data, _ = self.sock.recvfrom(1024)

                if len(data) < 32 or data[:2] != b"\x21\x31":
                    raise ValueError(f"握手失败，收到无效响应报文: {data.hex()}")

                _, _, _, device_id, stamp = struct.unpack("!HHIII", data[:16])
                self.device_id = device_id
                self.server_stamp = stamp
                self.local_stamp_time = time.time()
                return self.device_id, self.server_stamp
            except Exception as e:
                last_err = e
                time.sleep(0.3)
        raise last_err

    def send_command(self, method: str, params: Optional[list] = None, retries: int = 3) -> Dict[str, Any]:
        """向设备发送加密 JSON-RPC 指令并接收解密结果（支持自动重试）"""
        if params is None:
            params = []

        last_err = None
        for attempt in range(retries):
            try:
                # 时间戳超过 50 秒或未握手时自动重新握手
                if self.device_id is None or (time.time() - self.local_stamp_time > 50):
                    self.handshake()

                current_stamp = self.server_stamp + int(time.time() - self.local_stamp_time)
                self.msg_id += 1

                payload = json.dumps(
                    {"id": self.msg_id, "method": method, "params": params},
                    separators=(',', ':')
                ).encode('utf-8')

                encrypted = self._encrypt(payload)
                length = 32 + len(encrypted)

                header = struct.pack("!HHIII", 0x2131, length, 0, self.device_id, current_stamp)
                checksum = hashlib.md5(header + self.token + encrypted).digest()
                packet = header + checksum + encrypted

                self.sock.sendto(packet, (self.ip, self.port))
                resp_data, _ = self.sock.recvfrom(4096)

                if len(resp_data) < 32:
                    raise ValueError(f"响应报文长度不足: {resp_data.hex()}")

                resp_encrypted = resp_data[32:]
                if not resp_encrypted:
                    return {}

                decrypted = self._decrypt(resp_encrypted)
                return json.loads(decrypted.decode('utf-8'))
            except Exception as e:
                last_err = e
                # 出错时强制失效握手缓存，下次重试重新握手
                self.device_id = None
                time.sleep(0.4)

        raise last_err


class XiaomiPlug:
    """小米智能插座/插线板高级设备管理类"""

    def __init__(self, ip: str, token: str):
        self.ip = ip
        self.token = token
        self.protocol = MiioProtocol(ip, token)
        self.model = None

    def get_info(self) -> Dict[str, Any]:
        """获取设备基础信息（型号、固件版本、MAC、WiFi信号等）"""
        res = self.protocol.send_command("miIO.info")
        info = res.get("result", {})
        self.model = info.get("model", "unknown")
        return info

    def get_power_status(self) -> Dict[str, Any]:
        """
        获取当前设备的实时电力与运行参数
        返回包含实时功率(W)、开关状态、电流(A)、温度(℃)等的字典
        """
        # 查询插座的核心属性
        prop_list = [
            "power",
            "power_consume_rate",
            "current",
            "temperature",
            "power_factor",
            "wifi_led"
        ]
        res = self.protocol.send_command("get_prop", prop_list)
        values = res.get("result", [])

        if not values or len(values) < 4:
            raise ValueError(f"获取插座属性失败，返回数据: {res}")

        # 解析字段并进行类型转换
        power_state = values[0]  # 'on' or 'off'
        power_consume_rate = float(values[1]) if values[1] is not None else 0.0  # 实时功率(W)
        current = float(values[2]) if values[2] is not None else 0.0  # 电流(A)
        temperature = float(values[3]) if values[3] is not None else 0.0  # 内部温度(℃)
        power_factor = float(values[4]) if len(values) > 4 and values[4] is not None else 0.0
        wifi_led = values[5] if len(values) > 5 else "unknown"

        return {
            "power_state": power_state,
            "is_on": (power_state == "on"),
            "power_w": power_consume_rate,
            "current_a": current,
            "temperature_c": temperature,
            "power_factor": power_factor,
            "wifi_led": wifi_led,
            "raw_result": values,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }

    def turn_on(self) -> bool:
        """开启插座供电"""
        res = self.protocol.send_command("set_power", ["on"])
        return res.get("result") == ["ok"]

    def turn_off(self) -> bool:
        """关闭插座供电"""
        res = self.protocol.send_command("set_power", ["off"])
        return res.get("result") == ["ok"]

    def set_wifi_led(self, state: bool) -> bool:
        """设置 WiFi 指示灯开关"""
        param = "on" if state else "off"
        res = self.protocol.send_command("set_wifi_led", [param])
        return res.get("result") == ["ok"]
