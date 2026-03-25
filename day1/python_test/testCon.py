import socket
import threading
import time

# 仅保持长连接，不发送请求
def keep_conn_alive(client_id):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    try:
        s.connect(("127.0.0.1", 8080))
        print(f"[客户端 {client_id}] 连接成功，保持连接不退出...")
        # 无限阻塞，保持连接
        while True:
            time.sleep(1)
            # 可选：发送心跳包，防止服务端超时断开
            # s.sendall(b"PING\n")
            # resp = s.recv(1024)
    except (TimeoutError, OSError) as e:
        print(f"[客户端 {client_id}] 连接异常: {e}")
    finally:
        try:
            s.close()
        except:
            pass
        print(f"[客户端 {client_id}] 连接关闭")

def run_keep_alive_test(num_clients):
    threads = []
    # 启动多个客户端建立长连接
    for i in range(num_clients):
        t = threading.Thread(target=keep_conn_alive, args=(i,))
        t.daemon = True
        threads.append(t)
        t.start()
        time.sleep(0.001)  # 避免瞬间建连过多
    
    # 主线程监控
    try:
        while True:
            alive = sum(1 for t in threads if t.is_alive())
            print(f"\n当前存活连接数: {alive}/{num_clients} (按 Ctrl+C 退出)")
            time.sleep(3)
    except KeyboardInterrupt:
        print("\n测试终止，关闭所有连接...")

# 测试：建立10个长连接，不发送请求，仅保持连接
run_keep_alive_test(10000000)