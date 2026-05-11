#ifndef GROUPUSERS_H
#define GROUPUSERS_H

#include "user.hpp"

#include <string>

// 群组用户，多了一个role信息，从User类直接继承，并且复用User的其他信息
class GroupUser : public User
{
public:
    void setRole(string role) { this->role = role; };
    string getRole() { return this->role; };

private:
    string role;
};

#endif
