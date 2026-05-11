#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    : _publish_context(nullptr), _subscribe_context(nullptr)
{
}

Redis::~Redis()
{
    // 释放资源
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接服务器
bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr || _publish_context->err != 0)
    {
        if (_publish_context)
        {
            cerr << "connect redis failed! error: " << _publish_context->errstr << endl;
            redisFree(_publish_context);
            _publish_context = nullptr;
        }
        else
        {
            cerr << "connect redis failed! can't allocate context!" << endl;
        }
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr || _subscribe_context->err != 0)
    {
        if (_subscribe_context)
        {
            cerr << "connect redis failed! error: " << _subscribe_context->errstr << endl;
            redisFree(_subscribe_context);
            _subscribe_context = nullptr;
        }
        else
        {
            cerr << "connect redis failed! can't allocate context!" << endl;
        }
        return false;
    }

    // Redis认证
    redisReply *reply = nullptr;

    reply = (redisReply *)redisCommand(_publish_context, "AUTH 1234");
    if (nullptr == reply || reply->type == REDIS_REPLY_ERROR)
    {
        cerr << "auth failed for publish context!" << endl;
        if (reply) freeReplyObject(reply);
        redisFree(_publish_context);
        _publish_context = nullptr;
        redisFree(_subscribe_context);
        _subscribe_context = nullptr;
        return false;
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(_subscribe_context, "AUTH 1234");
    if (nullptr == reply || reply->type == REDIS_REPLY_ERROR)
    {
        cerr << "auth failed for subscribe context!" << endl;
        if (reply) freeReplyObject(reply);
        redisFree(_publish_context);
        _publish_context = nullptr;
        redisFree(_subscribe_context);
        _subscribe_context = nullptr;
        return false;
    }
    freeReplyObject(reply);

    cout << "connect redis-server success!" << endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish message failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }

    redisReply *reply = nullptr;
    if (REDIS_OK != redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        cerr << "subscribe get reply failed!" << endl;
        return false;
    }
    if (reply != nullptr)
    {
        freeReplyObject(reply);
    }

    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }

    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observe_channel_message()
{
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3 &&
            reply->element != nullptr && reply->element[1] != nullptr && reply->element[1]->str != nullptr &&
            reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    cerr << "observe_channel_message quit" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}

void Redis::start_observer()
{
    bool expected = false;
    if (_observer_started.compare_exchange_strong(expected, true))
    {
        thread t([this]()
                 { observe_channel_message(); });
        t.detach();
    }
}
