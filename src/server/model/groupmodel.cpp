#include "groupmodel.hpp"
#include "db.h"

#include <vector>

// 创建群组
bool groupModel::createGroup(Group &group)
{
    // 组装sql语句
    // 向all_group表中添加一项
    char sql[1024] = {0};
    sprintf(sql, "insert into all_group(groupname, groupdesc) values('%s', '%s')",
            group.getGroupName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql)) // 如果组名重复会创建失败
        {
            // 创建成功后设立群组id
            group.setID(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

// 加入群组
void groupModel::addGroup(int userid, int groupid, string role)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into group_user values(%d, %d, '%s')",
            groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在群组的信息
vector<Group> groupModel::queryGroups(int userid)
{
    /*
    1.先根据userid在groupuser表中查询出该用户的所属信息
    2.再根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from all_group a inner join \
        group_user b on a.id = b.groupid where b.userid = %d",
            userid);

    vector<Group> groupVec;
    MySQL mysql;
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
                Group group;
                group.setID(atoi(row[0]));
                group.setGroupName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }

            mysql_free_result(res);
        }

        // 查询群组的成员信息
        for (Group &group : groupVec)
        {
            // 遍历每个群组，以群组中的userid来查询出users表中的群组成员的详细信息
            sprintf(sql, "select a.id,a.name,a.state,b.grouprole from users a inner join \
                group_user b on b.userid = a.id where b.groupid = %d",
                    group.getID());

            MYSQL_RES *res = mysql.query(sql);
            if (res != nullptr)
            {
                // 查询成功
                MYSQL_ROW row;
                // 逐行读取消息
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    GroupUser gpuser;
                    gpuser.setId(atoi(row[0]));
                    gpuser.setName(row[1]);
                    gpuser.setState(row[2]);
                    gpuser.setRole(row[3]);
                    group.getUsers().push_back(gpuser); // 把群组成员添加到群组中，因为这是个GroupUser类的vector，所以可以直接添加GroupUser对象
                }
                mysql_free_result(res);
            }
        }
    }
    return groupVec; // 最终得到的是userid用户所在的所有群组，以及每个群组的所含有的所有成员信息
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用于群聊业务给群组其他成员群发消息
vector<int> groupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from group_user where groupid = %d and userid != %d", groupid, userid);

    MySQL mysql;
    vector<int> gpuidvec;

    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查询成功
            MYSQL_ROW row;
            // 逐行读取消息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                // 把一行消息追加到vector容器的最后一个位置。 vector容器的容量会自动增加，通常是两倍
                gpuidvec.push_back(atoi(row[0]));
            }

            mysql_free_result(res);
        }
    }

    return gpuidvec; // 返回群组中除发送者(userid)外的所有成员ID列表，用于群聊消息转发，避免消息回发给自己
}
