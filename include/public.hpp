#ifndef PUBLIC_H
#define PUBLIC_H

/*
    server 和 client 的公共文件

*/

// enum是cpp的枚举类型 会从上一个值中自动递增，更便于维护
enum EnMsgType
{
    LOGIN_MSG = 1, // 登录消息
    LOGIN_MSG_ACK, // 登录响应消息
    LOGINOUT_MSG,   // 注销消息
    REG_MSG,       // 注册消息
    REG_MSG_ACK,   // 注册响应消息

    ONE_CHAT_MSG,   // 一对一聊天消息
    ADD_FRIEND_MSG, // 添加好友消息

    CREATE_GROUP_MSG, // 群组创建消息
    ADD_GROUP_MSG,    // 加入群组消息
    GROUP_CHAT_MSG,   // 群组聊天消息

};

#endif