#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <atomic>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接服务器
    bool connect();

    // 向redis指定的通道channel发布消息
    bool publish(int channel, string message);

    // 向redis指定的通道cubscribe订阅消息
    bool subscribe(int channel);

    // 向redis指定的通道unssubscribe取消订阅消息
    bool unsubscribe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void observe_channel_message();

    // 启动观察者线程（仅在首次订阅后调用）
    void start_observer();

    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int, string)> fn);

private:
    /*
    为什么需要两个连接 ：
        因为 Redis 的 SUBSCRIBE 命令会 阻塞 当前线程等待消息，而 PUBLISH 是立即返回的。
        如果共用同一个连接，订阅的阻塞会阻塞发布操作。因此用两个独立连接分别处理发布和订阅，互不干扰。
    */

    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subscribe_context;

    // 标记观察者线程是否已启动
    atomic<bool> _observer_started{false};

    // 回调操作，收到订阅的消息，给server层上报
    function<void(int, string)> _notify_message_handler;
};

#endif