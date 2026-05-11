#include "offlinemessagemodel.hpp"
#include "db.h"

#include <vector>

// 存储用户离线消息
void offlinemsgModel::insert(int userid, string msg)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offline_message values(%d, '%s')", userid, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除用户离线消息
void offlinemsgModel::remove(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offline_message where userid=%d", userid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户离线消息
vector<string> offlinemsgModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offline_message where userid = %d", userid);

    MySQL mysql;
    vector<string> vec;

    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql); // 由于是用主键查询，所以只会查询成功一行

        if (res != nullptr)
        {
            // 查询成功

            // 把用户所有的离线消息放到vector中返回
            MYSQL_ROW row;
            // 逐行读取消息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                // 把一行消息追加到vector容器的最后一个位置。 vector容器的容量会自动增加，通常是两倍
                vec.push_back(row[0]);
            }

            mysql_free_result(res);
            return vec;
        }
    }

    return vec;
}
