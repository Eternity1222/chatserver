# chatserver — 集群聊天服务器

基于 **Muduo** 网络库和 **C++11** 构建的分布式集群聊天服务器。支持多实例部署，通过 **Nginx TCP 负载均衡 + Redis 发布订阅** 实现跨服务器消息转发，提供用户注册/登录、一对一聊天、好友管理、群组创建与聊天、离线消息等完整功能。

---

## 目录

- [技术栈](#技术栈)
- [架构设计](#架构设计)
- [消息协议](#消息协议)
- [数据库设计](#数据库设计)
- [环境依赖](#环境依赖)
- [编译步骤](#编译步骤)
- [运行指南](#运行指南)
- [客户端命令](#客户端命令)
- [配置说明](#配置说明)
- [项目结构](#项目结构)
- [设计亮点](#设计亮点)

---

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++11 |
| 网络库 | [Muduo](https://github.com/chenshuo/muduo) (Reactor 模式, epoll + 线程池) |
| 数据库 | MySQL / MariaDB (通过 `libmysqlclient`) |
| 缓存/消息 | Redis (通过 `hiredis` 客户端库, Pub/Sub 机制) |
| 序列化 | [JSON for Modern C++ (nlohmann/json)](https://github.com/nlohmann/json) — 单头文件库 |
| 构建系统 | CMake (>= 3.10) |
| 负载均衡 | Nginx (TCP 反向代理) |

---

## 架构设计

```
                    ┌─────────────────┐
                    │    Nginx TCP     │
                    │  负载均衡(轮询)   │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
        ┌──────────┐  ┌──────────┐  ┌──────────┐
        │Server 1  │  │Server 2  │  │Server N  │
        │  (6000)  │  │  (6001)  │  │  (600N)  │
        └────┬─────┘  └────┬─────┘  └────┬─────┘
             │              │              │
             └──────────────┼──────────────┘
                            ▼
                      ┌──────────┐
                      │  Redis   │
                      │ Pub/Sub  │
                      └──────────┘
```

**跨服务器消息转发流程：**

1. 用户 A (Server1) 给用户 B (Server2) 发送消息
2. Server1 发现 B 在线但不在自己的 `_userConnMap` 中
3. Server1 通过 `Redis PUBLISH channel=B_userid` 发布消息
4. Server2 的 Redis 订阅线程收到消息，通过回调转发给用户 B 的连接
5. 用户登录时**订阅**自己的 channel，注销/断连时**取消订阅**

---

## 消息协议

消息以 JSON 字符串形式通过 TCP 传输，通过 `msgid` 字段区分消息类型。

### 消息类型枚举

| 枚举值 | 名称 | 说明 |
|--------|------|------|
| 1 | `LOGIN_MSG` | 登录请求 |
| 2 | `LOGIN_MSG_ACK` | 登录响应 |
| 3 | `LOGINOUT_MSG` | 注销 |
| 4 | `REG_MSG` | 注册请求 |
| 5 | `REG_MSG_ACK` | 注册响应 |
| 6 | `ONE_CHAT_MSG` | 一对一聊天 |
| 7 | `ADD_FRIEND_MSG` | 添加好友 |
| 8 | `CREATE_GROUP_MSG` | 创建群组 |
| 9 | `ADD_GROUP_MSG` | 加入群组 |
| 10 | `GROUP_CHAT_MSG` | 群组聊天 |

---

## 数据库设计

数据库名：`chat`，包含以下 5 张表：

### 表结构

| 表名 | 字段 | 说明 |
|------|------|------|
| `users` | `id`, `name`, `password`, `state` (online/offline) | 用户表，`name` 唯一索引 |
| `friend` | `userid`, `friendid` | 好友关系表，联合主键 |
| `all_group` | `id`, `groupname`, `groupdesc` | 群组表，`groupname` 唯一索引 |
| `group_user` | `groupid`, `userid`, `grouprole` (creator/normal) | 群组成员关系表 |
| `offline_message` | `userid`, `message` (JSON 字符串) | 离线消息表 |

### 导入数据库

```bash
# 导入表结构 + 测试数据（推荐）
mysql -u root -p < sql/chat_full.sql

# 仅导入表结构（无测试数据）
mysql -u root -p < sql/schema.sql
```

详细说明请参考 [sql/README.md](sql/README.md)。

---

## 环境依赖

- 编译器支持 **C++11**（如 GCC >= 4.8）
- **CMake** >= 3.10
- **[Muduo](https://github.com/chenshuo/muduo)** 网络库
- **MySQL / MariaDB** 客户端库 (`libmysqlclient`)
- **[hiredis](https://github.com/redis/hiredis)**（Redis 客户端库）
- **pthread**

### 安装依赖（Ubuntu/Debian）

```bash
sudo apt install cmake g++ libmysqlclient-dev libhiredis-dev
# Muduo 需源码编译安装: https://github.com/chenshuo/muduo
```

---

## 编译步骤

```bash
cd build
cmake ..
make
```

编译完成后，可执行文件输出到 `bin/` 目录：

- `bin/chatserver` — 服务端
- `bin/chatclient` — 客户端

---

## 运行指南

### 1. 启动 Redis

```bash
redis-server --requirepass 1234
```

### 2. 导入数据库

```bash
mysql -u root -p < sql/chat_full.sql
```

### 3. 启动服务端

```bash
# ./bin/chatserver <IP> <端口>
./bin/chatserver 127.0.0.1 6000
```

### 4. 启动客户端

```bash
# ./bin/chatclient <服务器IP> <服务器端口>
./bin/chatclient 127.0.0.1 6000
```

### 测试账号

| 用户名 | 密码 | ID |
|--------|------|----|
| zhang san | 123456 | 1 |
| li si | 666666 | 2 |

---

## 客户端命令

### 登录前菜单

- `login` — 登录（输入 userid 和 password）
- `register` — 注册（输入 username 和 password）
- `quit` — 退出

### 登录后主聊天页

| 命令 | 格式 | 说明 |
|------|------|------|
| `help` | `help` | 显示帮助信息 |
| `chat` | `chat:friendid:message` | 一对一聊天 |
| `addfriend` | `addfriend:friendid` | 添加好友 |
| `creategroup` | `creategroup:groupname:groupdesc` | 创建群组 |
| `addgroup` | `addgroup:groupid` | 加入群组 |
| `groupchat` | `groupchat:groupid:message` | 群聊 |
| `loginout` | `loginout` | 注销登录 |

---

## 配置说明

项目当前将配置项**硬编码**在源文件中：

| 配置项 | 位置 | 默认值 |
|--------|------|--------|
| MySQL 地址 | `src/server/db/db.cpp` | `127.0.0.1:3306`, 用户 `root`, 密码 `1234`, 数据库 `chat` |
| Redis 地址 | `src/server/redis/redis.cpp` | `127.0.0.1:6379`, 密码 `1234` |
| 服务端 IP:端口 | 命令行参数 (`main.cpp`) | 例如 `./chatserver 127.0.0.1 6000` |
| Muduo 线程数 | `src/server/chatserver.cpp` | 4 个线程 (1 I/O + 3 工作) |

---

## 项目结构

```
chatserver/
├── bin/                    # 编译产物输出目录
│   ├── chatserver          # 服务端可执行文件
│   └── chatclient          # 客户端可执行文件
├── build/                  # CMake 构建目录
├── include/                # 头文件
│   ├── public.hpp          # 公共枚举（消息类型定义）
│   └── server/
│       ├── chatserver.hpp      # 网络服务器类（封装 Muduo TcpServer）
│       ├── chatservice.hpp     # 业务服务类（单例模式，消息路由）
│       ├── db/
│       │   └── db.h            # MySQL 数据库操作封装
│       ├── model/
│       │   ├── user.hpp                 # User 实体
│       │   ├── usermodel.hpp            # User 表数据操作
│       │   ├── friendmodel.hpp          # Friend 好友关系操作
│       │   ├── group.hpp                # Group 实体
│       │   ├── groupmodel.hpp           # Group 表数据操作
│       │   ├── groupusers.hpp           # GroupUser 实体（继承 User，带 role）
│       │   └── offlinemessagemodel.hpp  # 离线消息操作
│       └── redis/
│           └── redis.hpp    # Redis 发布/订阅封装（双连接）
├── src/
│   ├── client/
│   │   └── main.cpp         # 客户端主程序（Linux socket + 双线程）
│   └── server/
│       ├── main.cpp         # 服务端入口
│       ├── chatserver.cpp   # ChatServer 网络层实现
│       ├── chatservice.cpp  # 业务逻辑核心（消息路由与处理）
│       ├── db/
│       │   └── db.cpp       # MySQL 连接/查询/更新实现
│       ├── model/
│       │   ├── usermodel.cpp
│       │   ├── friendmodel.cpp
│       │   ├── groupmodel.cpp
│       │   └── offlinemessagemodel.cpp
│       └── redis/
│           └── redis.cpp    # Redis Pub/Sub 实现
├── sql/
│   ├── README.md            # 数据库使用说明
│   ├── schema.sql           # 仅表结构
│   └── chat_full.sql        # 表结构 + 测试数据
├── test/
│   ├── testjson/            # JSON 库测试
│   └── testmuduo/           # Muduo 网络库测试
├── thirdparty/
│   └── json.hpp             # nlohmann/json 单头文件库
├── view_problem/
│   └── problem.md           # 技术思考笔记
├── CMakeLists.txt           # 顶层 CMake 配置
└── README.md
```

---

## 设计亮点

- **单例模式** — `ChatService` 采用单例，确保全局只有一个业务处理实例
- **消息路由模式** — 通过 `msgid` → `MsgHandler` 映射表实现可扩展的消息分发，新增业务只需注册新的处理函数
- **双连接 Redis** — 发布和订阅使用独立连接，避免 `SUBSCRIBE` 阻塞 `PUBLISH`
- **原子操作** — 使用 `atomic<bool>` + `compare_exchange_strong` 确保 Redis 观察者线程仅启动一次
- **线程安全** — 对共享的 `_userConnMap` 所有读写均通过 `mutex` 保护
- **RAII 资源管理** — 使用 `lock_guard<mutex>` 管理互斥锁，MySQL/Redis 连接析构时自动释放
- **双线程客户端** — 主线程负责发送消息，子线程负责接收，信号量 (`sem_t`) 用于线程间同步
