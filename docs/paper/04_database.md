# 数据库设计（luna_chat，17 张表）

> 建库脚本：`sql/schema.sql`；ER 图：`docs/paper/figures/03_er.png`（可直接插入论文第 4 章）。

## 4.1 实体一览

| 域 | 表 | 作用 |
| --- | --- | --- |
| 用户域 | `user` | 账号、昵称、头像、在线状态；密码存 SHA-256 |
| 用户域 | `friend` | 好友关系，`status` 区分 pending / accepted |
| 用户域 | `login_log` | 登录审计（时间、IP） |
| 群组域 | `chat_group` | 群名、群主、群公告、共享文件夹路径 |
| 群组域 | `group_member` | 群成员与角色（owner / admin / member） |
| 群组域 | `group_join_request` | 群搜索后的加入申请（pending / accepted / rejected） |
| 群组域 | `group_activity` | 群活动（时间、地点、人数上限、状态） |
| 群组域 | `activity_signup` | 活动报名记录 |
| 内容域 | `message` | 群聊 / 私聊消息（text / file / voice） |
| 内容域 | `offline_message` | 离线消息队列，登录后补推 |
| 内容域 | `file_record` | 文件元信息与落盘路径 |
| 内容域 | `voice_record` | 语音元信息（时长、格式、码率） |
| 社交域 | `moment` | 朋友圈正文，软删标记 `deleted` |
| 社交域 | `moment_image` | 动态图片（最多 9 张，`sort_order` 保序） |
| 社交域 | `moment_like` | 点赞，`(moment_id, user_id)` 唯一 |
| 社交域 | `moment_comment` | 评论 |
| 社交域 | `match_session` | 随机匹配会话（双方、模式、状态） |

## 4.2 关系与基数

> 说明：`schema.sql` 未建物理外键，图中与下表均为**逻辑外键**。这样做是为了演示环境可以随时清库重建、
> 也避免删用户时被外键连锁阻塞；一致性由服务端 Handler 保证（例如退群前检查群主转让）。

| 关系 | 基数 | 外键字段 | 说明 |
| --- | --- | --- | --- |
| user – friend | 1:N / 1:N | friend.user_id、friend.friend_id | 一条记录同时指向申请人 |
| user – login_log | 1:N | login_log.user_id | 每次登录一条 |
| user – match_session | 1:N / 1:N | match_session.user_a、user_b | 同一次匹配对应两行 |
| user – chat_group | 1:N | chat_group.owner_id | 群主 |
| user – group_member | 1:N | group_member.user_id | 一个用户可加入多个群 |
| chat_group – group_member | 1:N | group_member.group_id | 群成员表 |
| chat_group – group_activity | 1:N | group_activity.group_id | 活动归属群 |
| user – group_activity | 1:N | group_activity.creator_id | 发起人 |
| group_activity – activity_signup | 1:N | activity_signup.activity_id | 报名记录 |
| user – activity_signup | 1:N | activity_signup.user_id | 用户报名 |
| user – message | 1:N / 1:N | message.from_user_id、to_user_id | 发送者 / 私聊接收者 |
| chat_group – message | 1:N | message.group_id | 群消息（与 to_user_id 互斥） |
| user – offline_message | 1:N | offline_message.to_user_id | 离线队列 |
| user – file_record | 1:N | file_record.from_user_id、to_user_id | 文件收发 |
| chat_group – file_record | 1:N | file_record.group_id | 群文件 |
| user – voice_record | 1:N | voice_record.from_user_id | 语音发送者 |
| user – moment | 1:N | moment.user_id | 动态作者 |
| moment – moment_image | 1:N | moment_image.moment_id | 一帖多图 |
| moment – moment_like | 1:N | moment_like.moment_id | 一帖多赞 |
| moment – moment_comment | 1:N | moment_comment.moment_id | 一帖多评论 |
| user – moment_like | 1:N | moment_like.user_id | 谁点的赞 |
| user – moment_comment | 1:N | moment_comment.user_id | 谁评论的 |

## 4.3 关键索引

| 表 | 索引 | 用途 |
| --- | --- | --- |
| user | `uk_username(username)` | 登录查重、防重名 |
| friend | `uk_friend(user_id, friend_id)`、`idx_friend_id` | 防重复申请；反向查"谁加了我" |
| group_member | `uk_group_user(group_id, user_id)`、`idx_user` | 防重复入群；查"我的群" |
| message | `idx_group_time(group_id, created_at)`、`idx_pair_time(from_user_id, to_user_id, created_at)` | 历史消息按会话 + 时间翻页 |
| offline_message | `idx_to_user(to_user_id, delivered)` | 登录时取未投递消息 |
| group_activity | `idx_group_status(group_id, status)` | 活动列表、巡检扫描 |
| activity_signup | `uk_activity_user(activity_id, user_id)` | 一人一报名，重复报名走更新 |
| moment_like | `uk_moment_user(moment_id, user_id)` | 一人一赞 |
| match_session | `uk_session_id(session_id)` | 会话 UUID 唯一 |

## 4.4 设计要点（答辩可讲）

1. **密码不存明文**：客户端明文只在内存里，服务端 `LunaCrypto::sha256Hex` 后入库，比对也按哈希。
2. **离线消息与消息表分离**：`offline_message.payload_json` 存完整帧，登录时批量取出并标记 `delivered=1`，
   投递逻辑与聊天业务解耦，也方便排查"消息到底发没发出去"。
3. **状态用 ENUM 落地**：活动 `published/ongoing/ended/cancelled`、匹配 `active/ended`、
   报名 `signed/cancelled`，避免出现"字符串拼错导致状态机走偏"。
4. **软删除**：朋友圈用 `deleted` 标记，列表查询统一带 `deleted=0`；点赞/评论保留原记录便于统计。
5. **文件与语音只存元信息**：实际二进制落在 `received/`、`group_files/`、`avatars/` 等目录，
   数据库只存路径与大小，避免 BLOB 把库撑大。
6. **演示数据**：脚本内置 `luna` / `star` 两个账号（密码 123456）和"校园大厅"群，答辩现场可直接登录。
