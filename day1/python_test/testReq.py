import socket
import struct
import random
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# ================= 配置区 =================
HOST = '127.0.0.1'
PORT = 8080
MAGIC = 0x4647
DIM = 4             # 需与服务端配置一致
NUM_CLIENTS = 20    # 并发客户端数量（线程数）
REQ_PER_CLIENT = 5000  # 每个客户端发送的请求数
SET_RATIO = 1.0     # 写入操作占比 (0.9 代表 90% 是 SET, 10% 是 SEARCH)

# ================= 协议定义 =================
class OpCode:
    SET = 1
    GET = 2
    DEL = 3
    SEARCH = 4

class DataType:
    FLOAT32 = 1

class VectorClient:
    def __init__(self, host=HOST, port=PORT, magic=MAGIC):
        self.host = host
        self.port = port
        self.magic = magic
        self.sock = None
        # 对应 C++ #pragma pack(1) 的 16 字节: magic(H), op(B), type(B), key(Q), dim(I)
        self.header_struct = struct.Struct("<HBBQI") 

    def connect(self):
        """长连接：只调用一次"""
        if self.sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(30.0)  # 长连接超时放大
            self.sock.connect((self.host, self.port))

    def _recv_exactly(self, n):
        data = b''
        while len(data) < n:
            packet = self.sock.recv(n - len(data))
            if not packet:
                raise ConnectionError("连接被服务器关闭")
            data += packet
        return data

    def set_vector(self, key_id, vector):
        """复用连接发送 SET 请求"""
        dim = len(vector)
        header = self.header_struct.pack(self.magic, OpCode.SET, DataType.FLOAT32, key_id, dim)
        body = struct.pack(f"<{dim}f", *vector)
        self.sock.sendall(header + body)
        
        resp = self._recv_exactly(16)
        if resp:
            res_header = self.header_struct.unpack(resp)
            return res_header[3] == 1
        return False

    def search(self, vector, top_k=3):
        """复用连接发送 SEARCH 请求"""
        dim = len(vector)
        header = self.header_struct.pack(self.magic, OpCode.SEARCH, DataType.FLOAT32, top_k, dim)
        body = struct.pack(f"<{dim}f", *vector)
        self.sock.sendall(header + body)
        
        resp = self._recv_exactly(16)
        if not resp:
            raise ConnectionError("接收响应头失败")
        
        res_header = self.header_struct.unpack(resp)
        result_count = res_header[3]
        # 读取结果向量（长连接必须读完，否则粘包）
        if result_count > 0:
            self._recv_exactly(result_count * dim * 4)
        return True

    def close(self):
        """压测结束后统一关闭"""
        if self.sock:
            self.sock.close()
            self.sock = None

# ================= 压测逻辑 =================
def worker(client_id):
    """长连接核心：一个 client 一个连接，发完所有请求再关闭"""
    client = VectorClient()
    success = 0
    try:
        client.connect()  # 【只连接一次】
        # 循环发送所有请求，不断开
        for i in range(REQ_PER_CLIENT):
            key = client_id * 1000000 + i
            
            if random.random() < SET_RATIO:
                vec = [random.random() for _ in range(DIM)]
                if client.set_vector(key, vec):
                    success += 1
            else:
                query = [random.random() for _ in range(DIM)]
                if client.search(query, top_k=3):
                    success += 1
    except Exception as e:
        print(f"[!] 客户端 {client_id} 异常: {e}")
    finally:
        client.close()  # 【所有请求发完才关闭】
    return success

def run_test():
    print(f"[*] 准备启动长连接压测...")
    print(f"[*] 配置: {NUM_CLIENTS} 线程 | 每线程 {REQ_PER_CLIENT} 请求 | 写入占比 {SET_RATIO*100}%")
    
    start_time = time.perf_counter()
    total_req = NUM_CLIENTS * REQ_PER_CLIENT
    completed_req = 0
    
    with ThreadPoolExecutor(max_workers=NUM_CLIENTS) as executor:
        futures = [executor.submit(worker, i) for i in range(NUM_CLIENTS)]
        
        for future in as_completed(futures):
            completed_req += future.result()
            
    end_time = time.perf_counter()
    duration = end_time - start_time
    
    print("\n" + "="*40)
    print(f"长连接压测结果统计:")
    print(f"总耗时     : {duration:.2f} 秒")
    print(f"成功请求数 : {completed_req} / {total_req}")
    print(f"平均 QPS   : {completed_req / duration:.2f}")
    print(f"成功率     : {(completed_req / total_req)*100:.2f}%")
    print("="*40)

if __name__ == "__main__":
    run_test()