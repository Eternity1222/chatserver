# 数据库说明

本目录用于保存聊天服务器项目的 MySQL 数据库初始化脚本。

项目默认使用数据库名：`chat`，其中密码默认为：`1234`。

## 文件结构

```text
sql/
├── README.md
├── schema.sql
└── chat_full.sql
```

## 文件说明

| 文件 | 说明 |
|---|---|
| `schema.sql` | 仅包含数据库和表结构，不包含测试数据，适合初始化干净的数据库环境 |
| `chat_full.sql` | 包含数据库表结构和测试数据，适合快速复现开发或测试环境 |

## 环境要求

- MySQL
- 已安装 `mysql` 客户端命令
- 已安装 `mysqldump` 命令

可以使用以下命令检查：

```bash
mysql --version
mysqldump --version
```

## 导入数据库

### 仅导入表结构

适合第一次初始化项目数据库，但不需要测试数据的场景。

```bash
mysql -u root -p < sql/schema.sql
```

执行后根据提示输入 MySQL 密码。

### 导入表结构和测试数据

适合快速恢复开发或测试环境。

```bash
mysql -u root -p < sql/chat_full.sql
```

执行后根据提示输入 MySQL 密码。

## 重新导出数据库

请在项目根目录执行以下命令。

### 仅导出表结构

```bash
mysqldump -u root -p --databases chat --no-data > sql/schema.sql
```

### 导出表结构和测试数据

```bash
mysqldump -u root -p --databases chat --routines --triggers --events > sql/chat_full.sql
```

## 关于密码参数

推荐使用：

```bash
-p
```

这种写法不会把密码直接写在命令中，执行后会提示输入密码，更安全。

不推荐使用：

```bash
-p1234
```

这种写法虽然可以直接执行，但密码会保存在 shell 历史记录中，存在泄露风险。

注意：如果使用 `-p1234`，`-p` 和密码之间不能有空格。

## 注意事项

- `schema.sql` 只包含表结构，不包含任何测试数据。
- `chat_full.sql` 包含测试数据，可能包含测试账号、密码和聊天记录。
- 提交到公开仓库前，请确认 `chat_full.sql` 中没有真实用户数据或敏感信息。
- 如果本地数据库中已经存在同名数据库 `chat`，导入前请确认是否需要备份旧数据。