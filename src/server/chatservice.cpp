#include "chatservice.hpp"
#include "public.hpp"
#include "user.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"

#include <vector>
#include <muduo/base/Logging.h>

using namespace std;
using namespace muduo;
using namespace muduo::net;

// 获取单例对象的接口函数
chatservice *chatservice::instance()
{
    static chatservice service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
chatservice::chatservice()
{
    // msgid==1 就调用login函数 处理登录业务
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&chatservice::login, this, _1, _2, _3)});
    // 处理注销业务
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&chatservice::loginout, this, _1, _2, _3)});
    // 调用reg函数 处理注册业务
    _msgHandlerMap.insert({REG_MSG, std::bind(&chatservice::reg, this, _1, _2, _3)});

    // 调用onechat函数 处理一对一聊天业务
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&chatservice::oneChat, this, _1, _2, _3)});
    // 添加好友
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&chatservice::addFriend, this, _1, _2, _3)});

    // 创建群组
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&chatservice::createGroup, this, _1, _2, _3)});
    // 加入群组
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&chatservice::addGroup, this, _1, _2, _3)});
    // 群组聊天
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&chatservice::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&chatservice::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 处理服务器异常，业务重置(所有异常均可在此实现)
void chatservice::reset()
{
    // 重置用户状态信息
    _usermodel.resetState();
}

// 获取消息对应的处理器
MsgHandler chatservice::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid); // 寻找是否存在msgid对应的处理器
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid: " << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id password
void chatservice::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do login service!!";
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _usermodel.query(id);
    if (user.getId() == id && user.getPwd() == pwd) // 双重验证
    {
        if (user.getState() == "online")
        {
            // 登陆失败,用户已经在线
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is online, input anther account!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，存储用户连接信息
            {
                // 用一个花括号来确定互斥锁的作用域
                lock_guard<mutex> lock(_connMutex); // 互斥锁 并且函数lock_guard会自动解锁 因此要选择好锁的范围
                _userConnMap.insert({id, conn});
            }

            // 向redis订阅channel(id)
            if (!_redis.subscribe(id))
            {
                // redis订阅失败，记录日志并回滚
                LOG_ERROR << "redis subscribe failed! userid: " << id;
                {
                    lock_guard<mutex> lock(_connMutex);
                    _userConnMap.erase(id);
                }
                json response;
                response["msgid"] = LOGIN_MSG_ACK;
                response["errno"] = 1;
                response["errmsg"] = "server internal error, please try again!";
                conn->send(response.dump());
                return;
            }

            // 首次订阅成功后启动Redis观察者线程
            _redis.start_observer();

            // 登陆成功,且是第一次登录,需要更新用户状态 offline -> online
            user.setState("online");
            _usermodel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询用户是否有离线消息
            vector<string> offlinemsgvec = _offlinemsgmodel.query(id);
            if (!offlinemsgvec.empty())
            {
                response["offlinemsg"] = offlinemsgvec;
                // 在读取完用户的离线消息之后，就要立刻删除，以防重复读取。
                _offlinemsgmodel.remove(id);
            }

            // 查询用户的好友列表，并返回
            vector<User> friendvec = _friendmodel.query(id);
            if (!friendvec.empty())
            {
                vector<string> vec1;
                for (User &user : friendvec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();

                    vec1.push_back(js.dump());
                }

                response["friends"] = vec1;
            }

            // 查询用户的群组消息
            vector<Group> groupUserVec = _groupmodel.queryGroups(id);
            if (!groupUserVec.empty())
            {
                vector<string> groupv;
                for (Group &group : groupUserVec)
                {
                    json grpjs;
                    grpjs["id"] = group.getID();
                    grpjs["groupname"] = group.getGroupName();
                    grpjs["groupdesc"] = group.getDesc();
                    vector<string> grpUserv;
                    for (GroupUser &grpuser : group.getUsers()) // 遍历各个群组的用户列表
                    {
                        json grpuserjs;
                        grpuserjs["id"] = grpuser.getId();
                        grpuserjs["name"] = grpuser.getName();
                        grpuserjs["state"] = grpuser.getState();
                        grpuserjs["role"] = grpuser.getRole();
                        grpUserv.push_back(grpuserjs.dump());
                    }
                    grpjs["users"] = grpUserv;      // 群组用户列表
                    groupv.push_back(grpjs.dump()); // 有关于群组的信息，例如群组id等
                }
                response["groups"] = groupv;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password error!";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void chatservice::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do reg service!!";
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _usermodel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 添加好友业务
void chatservice::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendmodel.insert(userid, friendid);
}

// 处理注销业务
void chatservice::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 注销就是退出登录，向redis取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _usermodel.updateState(user);
}

// 处理客户端异常退出
void chatservice::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;

    { // 这里注意互斥锁的作用范围
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {

                user.setId(it->first); // 先把用户id存起来
                // 从map表中删除用户的连接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 异常退出登录，向redis取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _usermodel.updateState(user);
    }
}

// 一对一聊天业务
void chatservice::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();

    // 注意锁的作用域
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // 用户在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线。若在线说明没有和当前登录的用户在同一个服务器，但是也需要转发消息
    User user = _usermodel.query(toid);
    if (user.getState() == "online")
    {
        // 用户在线，转发消息
        if (_redis.publish(toid, js.dump()))
        {
            return;
        }
        // publish失败，记录日志并降级为离线消息
        LOG_ERROR << "redis publish failed! toid: " << toid;
    }

    // toid用户离线，存储到离线消息表
    _offlinemsgmodel.insert(toid, js.dump());
}

// 创建群组业务
void chatservice::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string desc = js["desc"];

    // 存储新创建的群组的信息
    Group group(-1, groupname, desc);   // 先使用默认id
    if (_groupmodel.createGroup(group)) // 由于群名设置了唯一，所以这里先做一步判断
    {
        // 存储群组创建者的信息
        _groupmodel.addGroup(userid, group.getID(), "creator");
    }
}

// 加入群组业务
void chatservice::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupmodel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void chatservice::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> uidvec = _groupmodel.queryGroupUsers(userid, groupid);

    // 在for循环外进行加锁
    lock_guard<mutex> lock(_connMutex);
    for (int id : uidvec)
    {

        auto it = _userConnMap.find(id); // 如果没有找到，会返回map的.end()迭代器
        if (it != _userConnMap.end())    // 用户在线
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else // 用户离线，就存储离线消息即可
        {
            // 查询用户是否在线
            User user = _usermodel.query(id);
            if (user.getState() == "online")
            {
                // 用户在线，转发群消息
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlinemsgmodel.insert(id, js.dump());
            }
        }
    }
}

// 设置上报消息的回调，这个是由redis调用的
void chatservice::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        // 转发消息
        it->second->send(msg);
        return;
    }

    // 用户不在线，存储离线消息
    _offlinemsgmodel.insert(userid, msg);
}
