import socket
import threading
import time
from queue import Queue
import numpy as np

# ====================== TCP客户端全局配置 ======================
RECV_BUFFER_SIZE = 4096    # 接收缓冲区
MSG_DELIMITER = b"\n"     # 消息分隔符(解决TCP粘包)
SERVER_IP = "127.0.0.1"   # 服务端IP
SERVER_PORT = 8080        # 服务端端口

# ====================== 向量生成配置 ======================
vector_config = {
    "dimension": 16
}

# 全局变量：用于统计搜索耗时（单客户端使用）
search_start_time = 0.0

# ====================== 消息接收线程 ======================
def recv_msg_worker(client_id, s, msg_queue, stop_event):
    global search_start_time
    recv_buffer = b""
    while not stop_event.is_set():
        try:
            s.settimeout(1.0)
            data = s.recv(RECV_BUFFER_SIZE)
            if not data:
                print(f"\n[客户端 {client_id}] 服务端断开连接")
                stop_event.set()
                break
            
            recv_buffer += data
            while MSG_DELIMITER in recv_buffer:
                msg_bytes, recv_buffer = recv_buffer.split(MSG_DELIMITER, 1)
                if msg_bytes:
                    msg = msg_bytes.decode("utf-8").strip()
                    msg_queue.put(msg)
                    print(f"\n[客户端 {client_id}] 服务端返回: {msg}")
                    
                    # ====================== 搜索耗时统计 ======================
                    if search_start_time > 0:
                        cost_time = time.time() - search_start_time
                        print(f"⏱️  [搜索耗时] 客户端往返总耗时: {cost_time:.4f} 秒")
                        search_start_time = 0.0  # 重置
        
        except socket.timeout:
            continue
        except OSError as e:
            print(f"\n[客户端 {client_id}] 接收消息失败: {e}")
            stop_event.set()
            break

# ====================== 向量生成工具 ======================
def generate_random_vector(dim: int) -> str:
    vec = np.random.rand(dim)
    return ",".join([f"{x:.6f}" for x in vec])

# ====================== 发送指令到服务端 ======================
# 1. 发送存储向量指令
def send_set_to_server(s, vec_id: int, dim: int, vector_str: str):
    msg = f"SET {vec_id} {dim} {vector_str}\n"
    s.sendall(msg.encode("utf-8"))

# 2. 发送检索查询指令
def send_search_to_server(s, dim: int, query_vec_str: str):
    global search_start_time
    msg = f"SEARCH {dim} {query_vec_str}\n"
    s.sendall(msg.encode("utf-8"))
    # 记录搜索开始时间
    search_start_time = time.time()

# ====================== 命令交互线程 ======================
def local_cmd_worker(client_id, s, stop_event):
    print("\n" + "="*70)
    print(f"📡 向量客户端 (无心跳·长连接·耗时统计)")
    print("🎯 客户端：生成向量+发送 | 服务端：存储+检索")
    print("⏱️  支持：插入耗时 / 搜索往返耗时 统计")
    print("="*70)
    print("✅ 命令列表：")
    print("  set 16        → 设置向量维度")
    print("  send          → 单条插入(记录耗时)")
    print("  batch 1000    → 批量插入(记录总耗时)")
    print("  search        → 搜索查询(记录往返耗时)")
    print("  exit          → 关闭连接退出")
    print("="*70)

    vec_count = 0
    while not stop_event.is_set():
        try:
            cmd = input("\n> ").strip()
            if not cmd:
                continue

            parts = cmd.split()
            command = parts[0]

            # 1. 设置向量维度
            if command == "set":
                if len(parts) >= 2:
                    try:
                        dim = int(parts[1])
                        vector_config["dimension"] = dim
                        print(f"✅ 向量维度已设置为: {dim}")
                    except ValueError:
                        print("❌ 请输入数字，例：set 16")

            # 2. 单条插入（记录耗时）
            elif command == "send":
                dim = vector_config["dimension"]
                start = time.time()  # 开始计时
                vec_str = generate_random_vector(dim)
                vec_count += 1
                send_set_to_server(s, vec_count, dim, vec_str)
                cost_time = time.time() - start  # 结束计时
                # print(f"[插入成功] ID={vec_count}, 维度={dim} | ⏱️  耗时: {cost_time:.4f}s")

            # 3. 批量插入（记录总耗时）
            elif command == "batch":
                if len(parts) < 2:
                    print("❌ 格式：batch 100000")
                    continue
                
                try:
                    total = int(parts[1])
                except ValueError:
                    print("❌ 请输入数字，例：batch 100000")
                    continue

                dim = vector_config["dimension"]
                print(f"\n📤 开始批量发送 {total} 条 {dim}维向量...")
                batch_start = time.time()  # 批量开始计时
                success = 0
                for _ in range(total):
                    if stop_event.is_set():
                        break
                    try:
                        vec_str = generate_random_vector(dim)
                        vec_count += 1
                        send_set_to_server(s, vec_count, dim, vec_str)
                        success += 1
                        time.sleep(0.0001)
                    except Exception as e:
                        print(f"\n❌ 发送中断: {str(e)}")
                        break
                batch_cost = time.time() - batch_start  # 批量结束计时
                print(f"\n✅ 批量发送完成！成功={success}/{total} | ⏱️  总耗时: {batch_cost:.4f}s")

            # 4. 搜索查询（记录往返耗时）
            elif command == "search":
                dim = vector_config["dimension"]
                query_vec = generate_random_vector(dim)
                send_search_to_server(s, dim, query_vec)
                print(f"[查询已发送] 维度={dim}，等待服务端检索结果...")

            # 5. 退出
            elif command == "exit":
                print(f"\n[客户端 {client_id}] 关闭连接...")
                stop_event.set()
                break

            else:
                print("❌ 无效命令")

        except Exception as e:
            print(f"\n❌ 命令异常: {str(e)}")

# ====================== 主客户端逻辑 ======================
def vector_client(client_id=1):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    stop_event = threading.Event()
    msg_queue = Queue()

    try:
        s.connect((SERVER_IP, SERVER_PORT))
        print(f"[客户端 {client_id}] ✅ 长连接建立成功")

        # 启动消息接收线程
        threading.Thread(
            target=recv_msg_worker,
            args=(client_id, s, msg_queue, stop_event),
            daemon=True
        ).start()

        # 命令交互
        local_cmd_worker(client_id, s, stop_event)

    except Exception as e:
        print(f"[客户端 {client_id}] ❌ 连接失败: {e}")
    finally:
        stop_event.set()
        try:
            s.shutdown(socket.SH_RDWR)
        except OSError:
            pass
        s.close()
        print(f"[客户端 {client_id}] 🔌 连接已关闭")

if __name__ == "__main__":
    vector_client(client_id=1)