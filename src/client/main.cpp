#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前系统登录的好友列表
vector<User> g_currentUserFriendList;
// 记录当前系统登录的群组列表
vector<Group> g_currentUserGroupList;
// 显示当前登陆成功用户的基本信息
void showCurrentUserData();

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天需要添加时间）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);
bool isShowMainMenu = false;

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error!" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error!" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用户用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 注册 登录 退出
        cout << "==============================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "==============================" << endl;
        cout << "Please input your choice: ";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, sizeof(pwd));

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

            if (-1 == len)
            {
                cerr << "send login msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0);
                if (-1 == len)
                {
                    cerr << "recv login response error!" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 登陆失败
                    {
                        cerr << responsejs["errmsg"] << endl; // 返回登陆失败的错误提示消息
                    }
                    else
                    {
                        // 登陆成功
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录好友列表
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录群组列表
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1 = responsejs["groups"];
                            for (string &str : vec1)
                            {
                                json grpjs = json::parse(str);
                                Group group;
                                group.setID(grpjs["id"].get<int>());
                                group.setGroupName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    json grpuserjs = json::parse(userstr);
                                    GroupUser grpuser;
                                    grpuser.setId(grpuserjs["id"].get<int>());
                                    grpuser.setName(grpuserjs["name"]);
                                    grpuser.setState(grpuserjs["state"]);
                                    grpuser.setRole(grpuserjs["role"]);
                                    group.getUsers().push_back(grpuser); // 存储每个群组的成员
                                }

                                g_currentUserGroupList.push_back(group);
                            }
                        }

                        // 显示登陆用户的基本信息
                        showCurrentUserData();

                        // 显示离线消息等  个人聊天信息或者群组聊天消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> offvec = responsejs["offlinemsg"];
                            for (string &str : offvec)
                            {
                                json js = json::parse(str);
                                if (ONE_CHAT_MSG == js["msgid"].get<int>()) // 一对一聊天消息
                                {
                                    // time + [id] + name + " said: " + xxx
                                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }
                                else if (GROUP_CHAT_MSG == js["msgid"].get<int>()) // 群组聊天消息
                                {

                                    cout << "Group message[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        static int readThreadCount = 0;
                        if (readThreadCount == 0) // 防止重复创建接收线程
                        {
                            // 登陆成功，启动接收线程负责接收数据
                            std::thread readTask(readTaskHandler, clientfd); // 在底层相当于调用linux的pthread_create
                            // 分离线程，避免主线程阻塞,使其在后台运行
                            readTask.detach(); // 在底层相当于调用linux的pthread_detach
                            readThreadCount++;
                        }

                        isShowMainMenu = true;
                        // 进入聊天主界面
                        mainMenu(clientfd);
                    }
                }
            }
            break;
        }

        case 2: // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, sizeof(name)); // 不用cin>>name,是因为它读取到空格后就停止了，不能读取到空格后的字符
            cout << "password:";
            cin.getline(pwd, sizeof(pwd));

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0); // 接收注册响应
                if (-1 == len)                                   // 接收失败
                {
                    cerr << "recv reg response error!" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 注册失败
                    {
                        cerr << name << " is already exist, register failed!" << endl;
                    }
                    else // 注册成功
                    {
                        cout << name << " register success! Userid is" << responsejs["id"]
                             << ", do not forget it!" << endl;
                    }
                }
            }
            break;
        }

        case 3: // quit业务
            close(clientfd);
            exit(0);
            break;

        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 显示当前登陆成功用户的基本信息
void showCurrentUserData()
{
    cout << "=====================login user========================" << endl;
    cout << "current login user: id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;

    cout << "---------------------friend list-----------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &ufriend : g_currentUserFriendList)
        {
            cout << ufriend.getId() << " " << ufriend.getName() << " " << ufriend.getState() << endl;
        }
    }
    else
    {
        cout << "no friend!" << endl;
    }

    cout << "---------------------group list-----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getID() << " " << group.getGroupName() << " " << group.getDesc() << endl;
            cout << "Group[" << group.getID() << "] user list:" << endl;
            for (GroupUser &groupuser : group.getUsers())
            {
                cout << groupuser.getId() << " " << groupuser.getName() << " " << groupuser.getState() << " " << groupuser.getRole() << endl;
            }
        }
    }
    else
    {
        cout << "no group!" << endl;
    }

    cout << "======================================================" << endl;
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, sizeof(buffer), 0); // 这里会阻塞
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收chatserver转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        if (ONE_CHAT_MSG == js["msgid"].get<int>()) // 一对一聊天消息
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (GROUP_CHAT_MSG == js["msgid"].get<int>()) // 群组聊天消息
        {

            cout << "Group message[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 获取系统时间（聊天需要添加时间）
string getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    char buffer[20] = {0};
    if (localtime_r(&now_time_t, &now_tm) != nullptr)
    {
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &now_tm);
    }
    return string(buffer);
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式为：help"},
    {"chat", "一对一聊天，格式为：chat:friendid:message"},
    {"addfriend", "添加好友，格式为：addfriend:friendid"},
    {"creategroup", "创建群组，格式为：creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式为：addgroup:groupid"},
    {"groupchat", "群聊消息，格式为：groupchat:groupid:message"},
    {"loginout", "注销，格式为：loginout"}};

// 注册系统支持的客户端命令处理。要注意符合函数类型
unordered_map<string, function<void(int, string)>> commandHandlerMap = { // 其中函数的参数int表示客户端socket，string表示用户输入的命令字符串
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 打印command列表
void help(int, string)
{
    cout << "show command list >>>" << endl;
    for (auto &command : commandMap)
    {
        cout << command.first << ": " << command.second << endl;
    }
    cout << endl;
}

// "chat" command handler chat:friendid:message
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cout << "chat command invalid! example|| chat:friendid:message" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    // 对于服务器端的json字段对应的组装
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime(); // 获取当前时间
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send chat msg error -> " << request << endl;
    }
}

// "addfriend" command handler addfriend:friendid
void addfriend(int clientfd, string str) // 只是简单实现，默认添加好友一定成功
{
    int friendid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send addfriend msg error -> " << request << endl;
    }
}

// "creategroup" command handler creategroup:groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cout << "creategroup command invalid! example|| creategroup:groupname:groupdesc" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["desc"] = groupdesc;
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send creategroup msg error -> " << request << endl;
    }
}

// "addgroup" command handler addgroup:groupid
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send addgroup msg error -> " << request << endl;
    }
}

// "groupchat" command handler groupchat:groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cout << "groupchat command invalid! example|| groupchat:groupid:message" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime(); // 获取当前时间
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send groupchat msg error -> " << request << endl;
    }
}

// "loginout" command handler loginout
void loginout(int clientfd, string)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string request = js.dump();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cout << "send loginout msg error -> " << request << endl;
    }
    else
    {
        cout<< "user " << g_currentUser.getId() << " logout!" << endl;
        isShowMainMenu = false; // 注销成功，显示登录界面
    }
}

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help(); // 可以不用带参数，默认有参数，登陆进来之后先展示客户端有哪些功能

    char buffer[1024] = {0};
    while (isShowMainMenu)
    {
        cin.getline(buffer, sizeof(buffer)); // 获取用户输入的命令字符串
        string commandbuf(buffer);
        string command;                 // 存储命令字符串
        int idx = commandbuf.find(":"); // 以冒号作为分隔符，获取命令字符串。得到的是冒号所在的位置
        if (idx == -1)
        {
            command = commandbuf; // 因为如help、loginout命令是没有冒号的
        }
        else
        {
            command = commandbuf.substr(0, idx); // 获取冒号前面的字符串作为命令
        }
        auto it = commandHandlerMap.find(command); // 在commandHandlerMap中寻找用户输入的命令
        if (it == commandHandlerMap.end())         // 没找到，即命令错误
        {
            cout << "invalid command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainmenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // commandbuf.size()-idx 是指截取的字符串的长度
    }
}
