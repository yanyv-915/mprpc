import socket
import struct
import time
import random
import threading
from enum import IntEnum

# 与 C++ 服务端完全一致
class OpCode(IntEnum):
    SET = 1
    GET = 2
    DEL = 3
    SEARCH = 4

# 你服务端支持的3种数据类型 ✅
class DataType(IntEnum):
    FLOAT32 = 1    # float
    INT16 = 2      # int16_t
    UINT8 = 3      # uint8_t
    UNKNOWN = 0

class VectorClient:
    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.magic = 0x4647
        self.sock = None

    def connect(self):
        if self.sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            self.sock.connect((self.host, self.port))

    # ------------------------------
    # 3种发送函数，严格对应服务端类型
    # ------------------------------

    # 1. 发送 FLOAT32 (float)
    def send_float(self, key_id, vector):
        dim = len(vector)
        header = struct.pack("<HBBQI", self.magic, OpCode.SET, DataType.FLOAT32, key_id, dim)
        body = struct.pack(f"<{dim}f", *vector)
        self.sock.sendall(header + body)
        return self.sock.recv(1024)

    # 2. 发送 INT16 (int16_t)
    def send_int16(self, key_id, vector):
        dim = len(vector)
        header = struct.pack("<HBBQI", self.magic, OpCode.SET, DataType.INT16, key_id, dim)
        body = struct.pack(f"<{dim}h", *vector)  # h = int16_t
        self.sock.sendall(header + body)
        return self.sock.recv(1024)

    # 3. 发送 UINT8 (uint8_t)
    def send_uint8(self, key_id, vector):
        dim = len(vector)
        header = struct.pack("<HBBQI", self.magic, OpCode.SET, DataType.UINT8, key_id, dim)
        body = struct.pack(f"<{dim}B", *vector)  # B = uint8_t
        self.sock.sendall(header + body)
        return self.sock.recv(1024)

# ------------------------------------------------
# 压测线程：你可以自由切换 send_float / send_int16 / send_uint8
# ------------------------------------------------
def worker(client, num_requests, dim):
    client.connect()
    for _ in range(num_requests):
        key_id = random.getrandbits(64)
        
        # ========== 选择一种发送 ==========
        # 1. float
        vec = [random.random() for _ in range(dim)]
        client.send_float(key_id, vec)

        # 2. int16
        # vec = [random.randint(-32768, 32767) for _ in range(dim)]
        # client.send_int16(key_id, vec)

        # 3. uint8
        # vec = [random.randint(0, 255) for _ in range(dim)]
        # client.send_uint8(key_id, vec)
        
if __name__ == "__main__":
    thread_count = 10
    requests_per_thread = 1000
    dim = 4

    start = time.time()
    threads = []
    for _ in range(thread_count):
        client = VectorClient()
        t = threading.Thread(target=worker, args=(client, requests_per_thread, dim))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print(f"完成！QPS: {thread_count*requests_per_thread/(time.time()-start):.2f}")