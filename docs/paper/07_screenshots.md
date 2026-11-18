# UI 截图清单（docs/screenshots）

> 共 74 张，按阶段命名：`stage3`–`stage9` 为早期分阶段验收，`stage11` 为界面重构后的定稿截图，
> `stage12` 为登录/注册与音效设置，`stage13` 为主窗口深色改版，`stage14` 为无边框窗口与日间主题，
> `stage15` 为加好友/新建群聊内嵌页，`stage16` 为朋友圈整页背景，`stage17` 为消息日期分组、邀请与空状态，
> `stage18` 为申请中心与群聊搜索。
> 论文插图建议统一裁掉窗口标题栏，保留 1px 浅灰描边；图题格式"图 5-x ××× 界面"。
> 重新截图时请使用夜间主题 + 1280×800 以上窗口，避免高分屏缩放导致文字发虚。

## 一、主界面与聊天

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage3_chat.png` | 早期三栏聊天界面 | 5.1 主界面布局（演进对比） |
| `stage11_bubbles.png` | 私聊气泡（左右对齐、头像、时间） | 5.2 消息气泡渲染 |
| `stage11_bubble_align.png` | 气泡左右对齐细节 | 5.2 气泡对齐规则 |
| `stage11_chat_avatars.png` | 会话列表与消息头像 | 5.2 头像加载 |
| `stage11_chat_header_avatar.png` | 聊天头部群头像 | 5.3 聊天头部 |
| `stage11_chat_header_icons.png` | 头部功能图标（文件、语音、群资料） | 5.3 头部功能入口 |
| `stage11_back_to_chat.png` | 从设置/资料页返回聊天 | 5.9 页面返回 |
| `stage11_discord_dark.png` | 深色主题整体效果 | 5.8 主题 |
| `stage11_sidebar_sections.png` | 左侧会话分组 | 5.1 会话列表 |
| `stage11_sidebar_icons.png` | 会话列表图标 | 5.1 图标规范 |

## 二、导航与图标栏

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage11_icon_rail_chat.png` | 图标栏选中"聊天" | 5.1 图标栏 |
| `stage11_icon_rail_moments.png` | 图标栏选中"朋友圈" | 5.1 页面切换 |
| `stage11_rail_icons.png` | 图标栏全部图标 | 5.1 图标清单 |
| `stage11_rail_sliding_highlight.png` | 选中态滑块动画 | 5.1 交互细节 |

## 三、好友与个人资料

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage11_friends.png` | 好友列表与在线状态 | 5.4 好友系统 |
| `stage11_profile_page.png` | 个人主页（头像、昵称、签名） | 5.4 个人资料 |
| `stage11_avatar_menu.png` | 头像右键菜单 | 5.4 更换头像 |
| `stage11_avatar_refresh.png` | 换头像后刷新 | 5.4 头像缓存刷新 |

## 四、群组

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage5_groups.png` | 群列表与创建群 | 5.5 群组管理 |
| `stage6_invite_menu.png` | 邀请好友入群菜单 | 5.5 邀请流程 |
| `stage4_admin.png` | 群成员管理（踢人 / 设管理员） | 5.5 权限模型 |
| `stage4_kick.png` | 踢人确认与结果 | 5.5 权限校验 |
| `stage11_group_info.png` | 群资料弹窗 | 5.5 群资料 |
| `stage11_group_info_from_chat.png` | 从聊天头部进入群资料 | 5.5 入口 |
| `stage11_groupinfo_page.png` | 群资料整页（成员、公告、共享文件夹） | 5.5 群资料页 |

## 五、朋友圈

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage8_moments.png` | 早期朋友圈时间线 | 5.6 功能演进 |
| `stage11_moment_page.png` | 定稿时间线（卡片、点赞、评论） | 5.6 时间线布局 |
| `stage11_moments_rework.png` | 重构后的卡片与交互 | 5.6 卡片设计 |
| `stage11_moment_banner.png` | 朋友圈背景设置 | 5.6 背景功能 |
| `stage8_image_zoom.png` | 图片九宫格与大图查看 | 5.6 图片浏览 |

## 六、随机匹配

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage9_match.png` | 早期匹配弹窗 | 5.7 功能演进 |
| `stage11_match_page.png` | 匹配页（月亮动画、模式选择、会话区） | 5.7 匹配页 |

## 七、群活动与设置

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage7_activity.png` | 早期活动列表 | 5.8 功能演进 |
| `stage11_activity_page.png` | 活动整页（列表、详情、报名） | 5.8 活动管理 |
| `stage11_settings.png` | 早期设置弹窗 | 5.9 功能演进 |
| `stage11_settings_page.png` | 设置整页（主题、音效行、音量、录音设备） | 5.9 设置页 |
| `stage12_settings_sound_row.png` | 四类音效合并为一行：复选框 + 选择文件 + 当前文件名 | 5.9 音效设置（定稿） |
| `stage11_theme_dark.png` | 夜间主题 | 5.9 主题 |
| `stage11_theme_light.png` | 日间主题 | 5.9 主题 |

## 八、登录与注册（新版）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage12_login_loading.png` | 连接服务器阶段：转圈 + "正在连接服务器…" | 5.0 启动流程（阶段一） |
| `stage12_login_logo.png` | 连接成功后图标在页面中央浮现 | 5.0 启动流程（阶段二） |
| `stage12_login_form.png` | 图标上浮到顶部后，登录表单淡入 | 5.1 登录页定稿 |
| `stage12_register.png` | 注册页：登录窗口内嵌卡片，淡出淡入切换 | 5.1 注册流程 |

## 九、主窗口深色改版（stage13）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage13_main_chat.png` | 夜紫主题主界面：渐变会话选中、玻璃输入框、渐变发送键 | 5.2 主界面定稿 |
| `stage13_settings.png` | 设置页：页头 + 玻璃卡片 + 紫色滑杆 + 渐变保存键 | 5.9 设置页定稿 |
| `stage13_moments.png` | 朋友圈：夜紫渐变背景、玻璃动态卡片 | 5.6 朋友圈定稿 |
| `stage13_match.png` | 随机匹配：页头、月牙光晕、渐变主按钮 | 5.7 匹配页定稿 |

## 十、无边框主窗口与浅色主题（stage14）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage14_main_dark.png` | 无边框主窗口 + 自绘标题栏（深色） | 5.2 窗口框架 |
| `stage14_settings_dark.png` | 设置页（深色）：页头 + 玻璃卡片 | 5.9 设置页 |
| `stage14_profile_dark.png` | 个人资料（深色）：头像 + 资料卡片 | 5.4 个人资料 |
| `stage14_groupinfo_dark.png` | 群资料（深色）：群头像/公告/成员双卡 | 5.5 群资料 |
| `stage14_groupfiles_dark.png` | 群共享文件夹（深色）：页头 + 工具栏 + 列表卡 | 5.5 共享文件夹 |
| `stage14_activity_dark.png` | 群活动（深色）：页头 + 表格卡 + 操作栏 | 5.8 群活动 |
| `stage14_main_light.png` | 日间主题主界面（深色图标栏 + 浅色内容） | 5.2 主题切换 |
| `stage14_settings_light.png` | 日间主题设置页 | 5.9 主题切换 |

## 十一、加好友与新建群聊（stage15）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage15_add_friend.png` | 加好友页：页头 + 搜索卡片 + 空状态 | 5.4 加好友 |
| `stage15_add_friend_search.png` | 关键词“星”搜到用户，已是好友时按钮置灰 | 5.4 用户搜索 |
| `stage15_create_group.png` | 新建群聊页：群头像 + 群名称 + 群公告 | 5.5 建群 |
| `stage15_group_created.png` | 创建成功后自动回到聊天并选中新群 | 5.5 建群结果 |
| `stage15_add_friend_light.png` | 日间主题的加好友页 | 5.4 主题适配 |
| `stage15_create_group_light.png` | 日间主题的新建群聊页 | 5.5 主题适配 |

## 十二、朋友圈整页背景（stage16）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage16_moments_background.png` | 背景图铺满整页，半透明卡片浮在背景之上 | 5.6 朋友圈背景 |

## 十三、消息分组、邀请与空状态（stage17）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage17_chat_dates.png` | 消息按日期分组，中间显示日期分隔行 | 5.3 聊天记录 |
| `stage17_invite_search.png` | 邀请成员内嵌页：搜索用户 + 行内邀请 | 5.5 邀请成员 |
| `stage17_activity_empty.png` | 群活动空状态：日历插画 + 引导文案 | 5.8 空状态 |
| `stage17_groupfiles_empty.png` | 群文件空状态：文件夹插画 + 引导文案 | 5.5 空状态 |

## 十四、申请中心与群聊搜索（stage18）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage18_requests_badge.png` | 群聊工具区四个入口：新建群 / 加好友 / 搜索群聊 / 申请消息（带红点） | 5.4 申请提醒 |
| `stage18_friend_requests.png` | 申请中心（好友申请页签）：申请人 + 同意/拒绝 | 5.4 好友申请 |
| `stage18_group_requests.png` | 申请中心（群聊申请页签）：申请人 + 目标群 + 同意/拒绝 | 5.5 入群审批 |
| `stage18_group_search.png` | 独立搜索群聊页：未搜索时显示随机推荐群 + 申请加入 | 5.5 群聊搜索 |
| `stage18_recommend_friends.png` | 加好友页：未搜索时显示随机推荐好友 | 5.4 推荐好友 |

## 十五、朋友圈内嵌评论（stage19）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage19_moment_comment_editor.png` | 点“评论”后在帖子下方展开输入框 | 5.6 内嵌评论 |
| `stage19_moment_comment_done.png` | 发送后输入框消失，评论出现在帖子下方 | 5.6 评论结果 |

## 十六、好友列表离线状态（stage21）

| 文件 | 内容 | 建议用途 |
| --- | --- | --- |
| `stage21_offline_friend.png` | 好友栏同时显示在线（●）与离线（○、灰色）好友 | 5.3 好友列表 |

## 十七、待补截图（答辩前）

| 场景 | 建议文件名 | 说明 |
| --- | --- | --- |
| 选择音效文件对话框 | `stage12_sound_picker.png` | 证明 wav/mp3/ogg 过滤 |
| 匹配成功响铃瞬间 | `stage12_match_sound.png` | 截图无法体现声音，可配波形/日志说明 |
| 群共享文件夹 | `stage12_group_files.png` | 目录树 + 上传下载按钮 |
| 打包产物目录 | `stage12_dist.png` | dist 下 exe + sounds + backgrounds |
