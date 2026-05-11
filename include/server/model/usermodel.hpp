#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表的数据操作类
class userModel
{
public:
    // User表增加的方法
    bool insert(User &user);

    // 根据主键查询用户信息
    User query(int id);

    // 更新用户状态信息
    bool updateState(User user);

    // 重置用户状态信息
    void resetState();
};

#endif