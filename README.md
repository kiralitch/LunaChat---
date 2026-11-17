# LunaChat（月之音）

基于 **Qt 5.15 + C++17** 的局域网即时通讯系统，课程设计 / 毕业设计项目。
一个服务端进程 + 任意多个客户端，数据落 MySQL，通信走自定义二进制长度前缀 + JSON 帧。

## 功能

- 账号：注册、登录、SHA-256 密码、心跳与断线重连
- 好友：申请 / 通过、在线状态、上线提示音
- 聊天：群聊、私聊、离线消息补推、历史消息分页
- 文件：≤10MB，64KB 分块、base64、进度条
- 语音：16kHz PCM 录制、分块传输、气泡内播放
- 群组：建群、邀请、踢人、管理员、转让、解散、群资料
- 群共享文件夹：目录树、上传 / 下载 / 删除
- 朋友圈：文字 + 九图、点赞、评论、背景图、软删除
- 随机匹配：文本 / 语音两条队列、15s 超时、临时会话
- 群活动：发布、报名、取消、状态自动推进、群内通知
- 设置：夜间 / 日间主题、四类提示音（开关 + 选文件）、音量、录音设备
- 提示音：消息 / 好友上线 / 匹配成功 / 活动提醒，统一 `SoundPlayer`

## 目录

```
Client/            客户端（Public 头文件 / Private 实现 / resources）
  Public/Core/       AppSettings、Recorder、SoundPlayer
  Private/UI/Pages/  ChatPage、MomentPage、MatchPage、ActivityPage...
Common/            协议、帧编解码、SHA-256、数据库访问（两端共用）
Server/            服务端（LunaServer、MessageDispatcher、Handlers、AdminWindow）
sql/schema.sql     建库脚本（16 张表 + 演示账号）
runtime/           随包分发的资源：sounds/ 提示音、backgrounds/ 背景
tools/             package.ps1 打包、make_paper_figures.py 论文插图
docs/              demo.md 演示脚本、paper/ 论文素材、screenshots/ 截图
```

## 快速开始

```powershell
# 1) 建库（会创建 luna_chat 与演示账号 luna / star，密码 123456）
mysql -h127.0.0.1 -uroot -p --default-character-set=utf8mb4 -e "source sql/schema.sql"

# 2) 配置数据库：复制模板并填写密码
copy Server\config.ini.example Server\config.ini

# 3) 编译（VS2022 + Qt 5.15.2 msvc2019_64）
msbuild LunaChat.sln /p:Configuration=Debug /p:Platform=x64 /m

# 4) 运行
x64\Debug\Server.exe
x64\Debug\Client.exe
```

客户端默认连 `127.0.0.1:8888`，服务端监听所有网卡，局域网内其他机器改一下客户端地址即可。

## 打包

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

产出 `dist\LunaChat-Release\`（exe + Qt 运行库 + 插件 + `sounds\` + `backgrounds\` + `sql\` + `config.ini`）
与同名 zip。目标机器需安装 VC++ 2015-2022 运行库。

## 文档

- 演示脚本：[docs/demo.md](docs/demo.md)
- 论文素材索引：[docs/paper/README.md](docs/paper/README.md)
  - 架构图、帧格式图、ER 图、活动 / 匹配状态机（`docs/paper/figures/`）
  - 消息类型表（87 种）、数据库设计、测试用例表、UI 截图清单
- 协议常量：`Common/Public/LunaProtocol/MessageTypes.h`

## 约定

- 所有源码 UTF-8 with BOM（MSVC 不加 BOM 会把中文按 GBK 解析）
- 新消息类型先在 `MessageTypes.h` 登记，两端只引用常量
- 服务端不信任客户端：权限、状态、大小都在服务端再校验一遍
