# LunaChat 消息类型表（87 种）

> 对应 `Common/Public/LunaProtocol/MessageTypes.h`，两端只引用常量、不手写字符串。
> 方向：`C→S` 客户端发起，`S→C` 服务端推送/回包，`双向` 表示请求与转发都用同一 type。

| 统计项 | 数量 |
| --- | --- |
| 消息类型总数 | 87 |
| 客户端发起（C→S） | 39 |
| 服务端推送/回包（S→C） | 36 |
| 双向复用（请求 + 转发同 type） | 12 |
| 业务模块 | 9 |

## 1. 连接与账户（9）

| type | 方向 | 说明 |
| --- | --- | --- |
| `register` | C→S | 账号注册，昵称可省略（默认取用户名） |
| `login` | C→S | 登录，服务端比对 SHA-256 后把 userId 绑定到会话 |
| `login_resp` | S→C | 登录/注册统一回包（ok、msg、userId、nickname） |
| `heartbeat` | C→S | 心跳，客户端每 15s 一次 |
| `heartbeat_resp` | S→C | 心跳回执，服务端 45s 收不到判定掉线 |
| `user_list` | S→C | 全量在线用户列表，登录/上下线时推送 |
| `hello` | C→S | 阶段 1 自检用，已由 login 取代，仅保留兼容 |
| `kick` | S→C | 被管理员踢下线（携带原因） |
| `error` | S→C | 统一错误回包（code、msg） |

## 2. 好友与个人资料（10）

| type | 方向 | 说明 |
| --- | --- | --- |
| `friend_request` | C→S | 发起好友申请，落库 status=pending |
| `friend_accept` | C→S | 同意申请，双向写入 accepted |
| `friend_list` | C→S | 请求好友列表 |
| `friend_list_resp` | S→C | 好友列表（含在线状态、备注名） |
| `friend_notify` | S→C | 好友申请 / 通过 / 上线等通知 |
| `profile_get` | C→S | 拉取个人资料 |
| `profile_update` | C→S | 修改昵称等资料 |
| `profile_resp` | S→C | 资料回包 |
| `avatar_get` | C→S | 按 userId 拉头像（群头像另见 group_avatar_get） |
| `avatar_resp` | S→C | 头像 base64 回包 |

## 3. 群组与群资料（14）

| type | 方向 | 说明 |
| --- | --- | --- |
| `group_create` | C→S | 创建群，创建者自动成为 owner |
| `group_create_resp` | S→C | 创建结果（含新群 id） |
| `group_list` | C→S | 请求我加入的群 |
| `group_list_resp` | S→C | 群列表（含角色、未读数） |
| `group_invite` | C→S | 邀请好友入群 |
| `group_invite_resp` | S→C | 邀请结果 |
| `group_info_get` | C→S | 拉群资料与成员列表 |
| `group_info_resp` | S→C | 群资料回包 |
| `group_info_update` | C→S | 修改群名 / 公告 / 头像（管理员） |
| `group_leave` | C→S | 退群，群主需先转让 |
| `group_transfer` | C→S | 转让群主 |
| `group_dissolve` | C→S | 解散群（仅群主） |
| `group_avatar_get` | C→S | 拉群头像 |
| `group_avatar_resp` | S→C | 群头像 base64 回包 |

## 4. 聊天与会话（6）

| type | 方向 | 说明 |
| --- | --- | --- |
| `chat_group` | 双向 | 群聊消息：C 发送，S 记库并广播给在线成员 |
| `chat_private` | 双向 | 私聊消息：对方离线时转存 offline_message |
| `chat_broadcast` | S→C | 全员系统广播 |
| `chat_history` | C→S | 拉取历史消息（按会话 + 时间游标） |
| `chat_history_resp` | S→C | 历史消息回包 |
| `offline_msg` | S→C | 登录后一次性补推的离线消息批量包 |

## 5. 文件与语音（9）

| type | 方向 | 说明 |
| --- | --- | --- |
| `file_meta` | 双向 | 文件元信息（名、大小、接收方），服务端回 file_resp |
| `file_chunk` | 双向 | 64KB 原始数据分块，base64 后解码落盘 |
| `file_done` | 双向 | 传输结束，服务端校验大小并转发 |
| `file_resp` | S→C | 接收 / 拒绝 / 失败回执 |
| `file_shared` | S→C | 文件已存入群共享文件夹的通知 |
| `voice_meta` | 双向 | 语音元信息（时长、格式、大小） |
| `voice_chunk` | 双向 | 语音分块传输 |
| `voice_done` | 双向 | 语音传输结束 |
| `voice_resp` | 双向 | 语音送达回执 / 失败原因 |

## 6. 群共享文件夹（8）

| type | 方向 | 说明 |
| --- | --- | --- |
| `group_files_list` | C→S | 列出目录内容 |
| `group_files_resp` | S→C | 目录内容（文件、大小、上传者） |
| `group_file_mkdir` | C→S | 新建文件夹 |
| `group_file_delete` | C→S | 删除文件或空目录 |
| `group_file_delete_resp` | S→C | 删除结果 |
| `group_file_download` | C→S | 请求下载共享文件 |
| `group_file_chunk` | S→C | 下载分块 |
| `group_file_download_done` | S→C | 下载完成回执 |

## 7. 朋友圈（15）

| type | 方向 | 说明 |
| --- | --- | --- |
| `moment_publish` | C→S | 发布动态（文字 + 最多 9 张图） |
| `moment_list` | 双向 | C 请求时间线；S 也用它推送刷新后的列表 |
| `moment_list_resp` | S→C | 时间线数据（含点赞、评论、图片索引） |
| `moment_like` | C→S | 点赞 / 取消赞 |
| `moment_like_resp` | S→C | 点赞结果 |
| `moment_comment` | C→S | 发表评论 |
| `moment_comment_resp` | S→C | 评论结果 |
| `moment_delete` | C→S | 删除自己的动态（软删 deleted=1） |
| `moment_delete_resp` | S→C | 删除结果 |
| `moment_notify` | S→C | 新动态 / 点赞 / 评论通知 |
| `moment_image` | C→S | 按需拉取某张图片 |
| `moment_image_resp` | S→C | 图片 base64 数据 |
| `moment_bg_get` | C→S | 拉朋友圈背景（存服务端 backgrounds/bg_<id>.jpg） |
| `moment_bg_resp` | S→C | 背景图数据 |
| `moment_bg_update` | C→S | 设置自己的朋友圈背景 |

## 8. 随机匹配（6）

| type | 方向 | 说明 |
| --- | --- | --- |
| `match_start` | C→S | 发起匹配（mode=text / voice） |
| `match_cancel` | C→S | 取消排队，服务端回 match_timeout |
| `match_result` | S→C | 配对成功（sessionId、peerUserId、peerNickname） |
| `match_timeout` | S→C | 15s 无人应答或取消排队的回执 |
| `match_end` | 双向 | 结束临时会话，服务端通知另一方 |
| `match_message` | 双向 | 临时会话消息（服务端回送发送者一份，避免本地回显重复） |

## 9. 群活动（10）

| type | 方向 | 说明 |
| --- | --- | --- |
| `activity_create` | C→S | 发布活动（仅群主 / 管理员） |
| `activity_list` | C→S | 请求群活动列表 |
| `activity_list_resp` | S→C | 活动列表（含报名数、我是否已报名） |
| `activity_detail` | C→S | 请求活动详情 |
| `activity_detail_resp` | S→C | 详情（含报名成员昵称） |
| `activity_signup` | C→S | 报名（要求 published 且未满） |
| `activity_signup_cancel` | C→S | 取消报名 |
| `activity_cancel` | C→S | 取消活动（群主 / 管理员） |
| `activity_notify` | S→C | 群内广播：发布 / 报名 / 取消报名 / 取消活动 |
| `activity_end` | S→C | 活动结束通知，当前版本预留未启用 |
