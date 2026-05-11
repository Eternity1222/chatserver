# chatserver

基于 Muduo 网络库的集群聊天服务器，配合 Nginx TCP 负载均衡环境运行。

## 环境依赖

- 编译器支持 C++11
- CMake >= 3.10
- [Muduo](https://github.com/chenshuo/muduo) 网络库
- MySQL / MariaDB 客户端库 (`libmysqlclient`)
- [hiredis](https://github.com/redis/hiredis)（Redis 客户端库）
- pthread

## 编译步骤

### 主项目（服务端 + 客户端）

```bash
cd build
cmake ..
make
```

编译完成后，可执行文件会输出到项目根目录的 `bin/` 文件夹：
- `bin/chatserver` — 服务端
- `bin/chatclient` — 客户端

### 测试项目（testmuduo）

```bash
cd test/testmuduo
mkdir build && cd build
cmake ..
make
```

编译完成后，可执行文件输出到 `test/testmuduo/bin/`：
- `bin/server` — Muduo 测试服务端

## 项目结构

```
chatserver/
├── include/          # 头文件
│   └── server/       # 服务端头文件
├── src/              # 源代码
│   ├── server/       # 服务端
│   └── client/       # 客户端
├── test/             # 测试代码
│   └── testmuduo/    # Muduo 基础测试
├── thirdparty/       # 第三方库（如 json.hpp）
├── CMakeLists.txt    # 顶层 CMake 配置
└── README.md
```
