#ifndef GROUP_H
#define GROUP_H

#include "groupusers.hpp"

#include <string>
#include <vector>
using namespace std;

// User表的ORM类
class Group
{
public:
    Group(int id = -1, string groupname = "", string desc = "")
    {
        this->id = id;
        this->groupname = groupname;
        this->desc = desc;
    }

    void setID(int id) { this->id = id; };
    void setGroupName(string groupname) { this->groupname = groupname; };
    void setDesc(string desc) { this->desc = desc; };

    int getID() { return this->id; };
    string getGroupName() { return this->groupname; };
    string getDesc() { return this->desc; };

    vector<GroupUser> &getUsers() { return this->users; };

private:
    int id;
    string groupname;
    string desc; // 组功能的描述
    vector<GroupUser> users; // 用于获取群组成员，这里不使用User类，由于GroupUser类有role信息
};

#endif