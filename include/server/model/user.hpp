#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

// User表的ORM类
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }

    // 设置
    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setPwd(string pwd) { this->password = pwd; }
    void setState(string state) { this->state = state; }

    // 获取
    int getId() { return this->id; }
    string getName() { return this->name; }
    string getPwd() { return this->password; }
    string getState() { return this->state; }

protected: // 使用protected主要是为了支持继承，子类可以访问这些属性，同时又不会让外部直接访问
    int id;
    string name;
    string password;
    string state;
};

#endif