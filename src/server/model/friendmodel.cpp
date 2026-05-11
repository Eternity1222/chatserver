#include "friendmodel.hpp"
#include "user.hpp"
#include "db.h"

// 添加好友
void friendModel::insert(int userid, int friendid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询好友列表
vector<User> friendModel::query(int userid)
{
    char sql[1024] = {0};
    // 做一个双向查询
    sprintf(sql, "select a.id,a.name,a.state from users a inner join friend b on b.friendid = a.id where b.userid = %d union select a.id,a.name,a.state from users a inner join friend b on b.userid = a.id where b.friendid = %d", userid, userid);

    vector<User> uservec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql); // 由于是用主键查询，所以只会查询成功一行
        if (res != nullptr)
        {
            // 查询成功
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);

                uservec.push_back(user);
            }

            mysql_free_result(res);
            return uservec;
        }
    }
    return uservec;
}
