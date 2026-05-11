#include "usermodel.hpp"
#include "user.hpp"
#include "db.h"
#include <iostream>
using namespace std;

// User表的增加方法
bool userModel::insert(User &user)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into users(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功用户的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
};

// 根据主键查询用户信息
User userModel::query(int id)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from users where id = %d", id);

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql); // 由于是用主键查询，所以只会查询成功一行

        if (res != nullptr)
        {
            // 查询成功
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                User user;
                // 以下四项一一对应数据库中的id name password state
                user.setId(atoi(row[0])); // 类型转换
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);
                mysql_free_result(res); // 记得释放内存
                return user;
            }
            mysql_free_result(res); // 记得释放内存
        }
    }
    return User(); // 直接调用类的构造函数
}

// 更新用户状态信息
bool userModel::updateState(User user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "update users set state = '%s' where id = %d",
            user.getState().c_str(), user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }

    return false;
}

// 重置用户状态信息
void userModel::resetState()
{
    // 1.组装sql语句
    char sql[1024] = {"update users set state = 'offline' where state = 'online'"};

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
