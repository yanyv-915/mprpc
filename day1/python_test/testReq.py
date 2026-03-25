import socket
import threading
import time
import random
from queue import Queue  # 用于缓存待处理的消息

# 全局配置
RECV_BUFFER_SIZE = 4096  # 接收缓冲区大小
HEARTBEAT_INTERVAL = 5    # 心跳发送间隔（秒）
MSG_DELIMITER = b"\n"     # 消息分隔符（解决粘包）

def recv_msg_worker(client_id, s, msg_queue, stop_event):
    """
    独立的消息接收线程：持续读取服务端消息，存入队列
    client_id: 客户端ID
    s: 已连接的socket
    msg_queue: 消息队列（缓存收到的消息）
    stop_event: 线程停止信号
    """
    recv_buffer = b""  # 缓存未处理的字节（解决粘包）
    while not stop_event.is_set():
        try:
            # 非阻塞读取（避免卡死），超时1秒
            s.settimeout(1.0)
            data = s.recv(RECV_BUFFER_SIZE)
            if not data:
                # 服务端关闭连接
                print(f"[客户端 {client_id}] 服务端主动断开连接")
                stop_event.set()
                break
            
            # 拼接缓冲区，按分隔符拆分消息
            recv_buffer += data
            while MSG_DELIMITER in recv_buffer:
                msg_bytes, recv_buffer = recv_buffer.split(MSG_DELIMITER, 1)
                if msg_bytes:  # 跳过空消息
                    msg = msg_bytes.decode("utf-8").strip()
                    msg_queue.put(msg)
                    print(f"[客户端 {client_id}] 收到服务端消息: {msg}")
        
        except socket.timeout:
            # 超时是正常的（没消息时会触发），继续循环
            continue
        except OSError as e:
            print(f"[客户端 {client_id}] 接收消息失败: {e}")
            stop_event.set()
            break

def send_heartbeat(client_id, s, stop_event):
    """
    心跳发送线程：定期发送心跳，维持长连接
    """
    while not stop_event.is_set():
        try:
            s.sendall(b"PING\n")
            time.sleep(HEARTBEAT_INTERVAL)
        except OSError as e:
            print(f"[客户端 {client_id}] 发送心跳失败: {e}")
            stop_event.set()
            break

def long_conn_stress_test(client_id, total_requests, interval=0.001):
    """
    长连接压力测试客户端（支持正常收发消息）
    client_id: 客户端唯一标识
    total_requests: 每个客户端要发送的总请求数（0表示无限发送）
    interval: 每次发送请求的间隔（秒）
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    stop_event = threading.Event()  # 线程停止信号
    msg_queue = Queue()             # 消息队列（存储收到的服务端消息）
    
    try:
        # 建立长连接
        s.connect(("127.0.0.1", 8080))
        print(f"[客户端 {client_id}] 建立长连接成功")
        
        # 启动接收消息线程（核心：独立线程，不阻塞发送）
        recv_thread = threading.Thread(
            target=recv_msg_worker,
            args=(client_id, s, msg_queue, stop_event),
            daemon=True
        )
        recv_thread.start()
        
        # 启动心跳线程
        heartbeat_thread = threading.Thread(
            target=send_heartbeat,
            args=(client_id, s, stop_event),
            daemon=True
        )
        heartbeat_thread.start()
        
        # 循环发送业务请求
        send_count = 0
        while not stop_event.is_set() and ((total_requests == 0) or (send_count < total_requests)):
            try:
                # 构造请求
                msg = f"SET key_{client_id}_{send_count} 1.1,2.2,3.3\n"
                s.sendall(msg.encode("utf-8"))
                send_count += 1
                print(f"[客户端 {client_id}] 发送第{send_count}条请求: {msg.strip()}")
                
                time.sleep(interval)  # 控制发送速率
            except socket.timeout:
                print(f"[客户端 {client_id}] 发送请求超时，已发送{send_count}条")
                continue
            except OSError as e:
                print(f"[客户端 {client_id}] 发送请求失败: {e}，已发送{send_count}条")
                stop_event.set()
                break
        
        print(f"[客户端 {client_id}] 完成发送{send_count}条请求，保持连接监听消息...")
        
        # 发送完请求后，保持连接并监听消息（直到手动终止）
        while not stop_event.is_set():
            time.sleep(1)
        
    except (TimeoutError, OSError) as e:
        print(f"[客户端 {client_id}] 连接失败: {e}")
    finally:
        # 停止所有子线程，关闭连接
        stop_event.set()
        try:
            s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        s.close()
        print(f"[客户端 {client_id}] 连接已关闭")

def run_long_conn_test(num_clients, req_per_client=0, interval=0.001):
    start_time = time.time()
    threads = []
    
    # 启动多个长连接客户端
    for i in range(num_clients):
        t = threading.Thread(
            target=long_conn_stress_test,
            args=(i, req_per_client, interval)
        )
        threads.append(t)
        t.daemon = True
        t.start()
        time.sleep(0.001)  # 错开启动时间，避免建连压力
    
    # 主线程监控
    try:
        while True:
            alive_count = sum(1 for t in threads if t.is_alive())
            print(f"\n=== 实时统计 ===")
            print(f"存活客户端数: {alive_count}/{num_clients}")
            print(f"运行时长: {time.time()-start_time:.2f} 秒")
            print("按 Ctrl+C 终止测试...")
            time.sleep(2)
    except KeyboardInterrupt:
        print("\n\n=== 测试终止 ===")
        end_time = time.time()
        duration = end_time - start_time
        print(f"总运行时长: {duration:.2f} 秒")
        print("所有客户端连接将被关闭...")

# 测试：5个客户端，每个发10条请求后保持连接监听消息
run_long_conn_test(1, 1)