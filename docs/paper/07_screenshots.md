# UI 截图清单（docs/screenshots）

> 共 39 张，按阶段命名：`stage3`–`stage9` 为早期分阶段验收，`stage11` 为界面重构后的定稿截图，
> `stage12` 为最后一轮（音效设置合并行）验证截图。
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

## 八、待补截图（答辩前）

| 场景 | 建议文件名 | 说明 |
| --- | --- | --- |
| 选择音效文件对话框 | `stage12_sound_picker.png` | 证明 wav/mp3/ogg 过滤 |
| 匹配成功响铃瞬间 | `stage12_match_sound.png` | 截图无法体现声音，可配波形/日志说明 |
| 群共享文件夹 | `stage12_group_files.png` | 目录树 + 上传下载按钮 |
| 打包产物目录 | `stage12_dist.png` | dist 下 exe + sounds + backgrounds |
