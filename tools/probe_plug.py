"""
小米智能插座探测与功率获取模块
"""
import socket
import struct
import hashlib
import json
import time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding


class MiioProtocol:
    def __init__(self, ip: str, token_hex: str, port: int = 54321, timeout: float = 3.0):
        self.ip = ip
        self.port = port
        self.timeout = timeout
        self.token = bytes.fromhex(token_hex)
        
        # 密钥派生
        self.key = hashlib.md5(self.token).digest()
        self.iv = hashlib.md5(self.key + self.token).digest()
        
        self.device_id = None
        self.server_stamp = None
        self.local_stamp_time = None
        self.msg_id = 1
        
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(self.timeout)

    def _encrypt(self, plaintext: bytes) -> bytes:
        padder = padding.PKCS7(128).padder()
        padded_data = padder.update(plaintext) + padder.finalize()
        cipher = Cipher(algorithms.AES(self.key), modes.CBC(self.iv))
        encryptor = cipher.encryptor()
        return encryptor.update(padded_data) + encryptor.finalize()

    def _decrypt(self, ciphertext: bytes) -> bytes:
        cipher = Cipher(algorithms.AES(self.key), modes.CBC(self.iv))
        decryptor = cipher.decryptor()
        padded_data = decryptor.update(ciphertext) + decryptor.finalize()
        unpadder = padding.PKCS7(128).unpadder()
        return unpadder.update(padded_data) + unpadder.finalize()

    def handshake(self):
        """发送 Hello 握手包获取设备 ID 和时间戳"""
        hello_packet = bytes.fromhex(
            "21310020ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        )
        self.sock.sendto(hello_packet, (self.ip, self.port))
        data, _ = self.sock.recvfrom(1024)
        
        if len(data) < 32 or data[:2] != b"\x21\x31":
            raise ValueError(f"无效的握手响应: {data.hex()}")
        
        _, _, _, device_id, stamp = struct.unpack("!HHIII", data[:16])
        self.device_id = device_id
        self.server_stamp = stamp
        self.local_stamp_time = time.time()
        return self.device_id, self.server_stamp

    def send_command(self, method: str, params: list = None):
        """向设备发送加密 JSON-RPC 命令并返回解析结果"""
        if params is None:
            params = []
        
        # 如果未握手或距离上次握手超过 60 秒，重新握手同步时间戳
        if self.device_id is None or (time.time() - self.local_stamp_time > 60):
            self.handshake()
            
        current_stamp = self.server_stamp + int(time.time() - self.local_stamp_time)
        self.msg_id += 1
        
        payload = json.dumps({"id": self.msg_id, "method": method, "params": params}, separators=(',', ':')).encode('utf-8')
        encrypted = self._encrypt(payload)
        length = 32 + len(encrypted)
        
        header = struct.pack("!HHIII", 0x2131, length, 0, self.device_id, current_stamp)
        checksum = hashlib.md5(header + self.token + encrypted).digest()
        packet = header + checksum + encrypted
        
        self.sock.sendto(packet, (self.ip, self.port))
        resp_data, _ = self.sock.recvfrom(4096)
        
        if len(resp_data) < 32:
            raise ValueError(f"响应报文过短: {resp_data.hex()}")
            
        resp_encrypted = resp_data[32:]
        if not resp_encrypted:
            return None
            
        decrypted = self._decrypt(resp_encrypted)
        return json.loads(decrypted.decode('utf-8'))


if __name__ == "__main__":
    ip = "192.168.1.100"
    token = "00000000000000000000000000000000"
    
    print(f"[*] 正在连接设备 {ip}...")
    dev = MiioProtocol(ip, token)
    
    dev_id, stamp = dev.handshake()
    print(f"[+] 握手成功! Device ID: {hex(dev_id)} ({dev_id}), Stamp: {stamp}")
    
    info = dev.send_command("miIO.info")
    print(f"[+] 设备信息 (miIO.info):\n{json.dumps(info, indent=2, ensure_ascii=False)}")
