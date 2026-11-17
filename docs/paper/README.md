# 论文 / 答辩素材索引

本目录集中放毕业论文（设计）能直接引用的图、表与说明文字，源文件都在仓库里，改完代码可重新生成。

## 素材清单

| 文件 | 类型 | 对应论文位置 | 说明 |
| --- | --- | --- | --- |
| `figures/01_architecture.png` | 架构图 | 第 3 章 系统设计 | 客户端 / 服务端 / 数据库三层，含 TCP 帧与心跳 |
| `figures/02_frame_format.png` | 协议图 | 3.x 通信协议 | 4 字节长度 + JSON 帧、约束、拆包流程 |
| `03_message_types.md` | 表 | 3.x 消息类型 | 87 种消息，按 9 个模块分组，含方向与说明 |
| `figures/03_er.png` | ER 图 | 4.x 数据库设计 | 16 张表、逻辑外键、域配色 |
| `04_database.md` | 表 + 说明 | 4.x 数据库设计 | 实体一览、关系基数、索引、设计要点 |
| `figures/04_activity_state.png` | 状态机 | 5.x 群活动 | published → ongoing → ended / cancelled + 报名子状态 |
| `figures/05_match_state.png` | 状态机 | 5.x 随机匹配 | 空闲 / 排队 / 会话中，超时与结束路径 |
| `06_test_cases.md` | 表 | 第 6 章 测试 | 38 条用例，含新增音效与打包验证 |
| `07_screenshots.md` | 清单 | 第 5 章 界面 | 39 张现有截图 + 4 张待补，含建议图题 |

## 图题建议（可直接粘进 Word）

- 图 3-1 LunaChat 系统总体架构图
- 图 3-2 通信帧格式与拆包流程图
- 图 4-1 LunaChat 数据库 ER 图
- 图 5-1 群活动状态机
- 图 5-2 随机匹配状态机

## 重新生成

```powershell
# 插图（Pillow 绘制，输出到 docs/paper/figures）
python tools\make_paper_figures.py

# 占位提示音（可选，输出到 runtime/sounds）
python tools\make_placeholder_sounds.py
```

> 插图脚本依赖 Windows 自带字体（微软雅黑 `msyh.ttc`、宋体 `simsun.ttc`、Consolas），
> 换机器若字体缺失，改脚本顶部的 `FONT_DIR` 即可。

## 写作提示

1. 架构图里"客户端服务层"的四个组件（LunaClient / Recorder / SoundPlayer / AppSettings）正好对应
   4 个可独立讲的设计点，答辩时按"网络、音频采集、提示音、配置持久化"展开。
2. 帧格式图重点讲**为什么用长度前缀**：定长 4 字节 → 半包/粘包都能 O(1) 定位边界，
   比文本分隔符稳；同时用 1MB 上限防恶意大包。
3. ER 图讲解顺序建议：user 为中心 → 群组域 → 内容域 → 社交域，最后点一句"逻辑外键、便于演示环境重建"。
4. 状态机图要配合代码位置讲：
   - 活动：`DbManager::refreshActivityStatuses()`（每分钟巡检）+ `ActivityHandler::handleCancel()`
   - 匹配：`MatchHandler::tryMatch()` / `notifyTimeout()` / `onUserOffline()`
5. 测试用例表里 TC-27、TC-28、TC-35 标了"待确认"，答辩前手工跑一遍并补截图，
   避免老师追问"音效到底响没响"。
