# 图标资源目录

## 群聊标题栏图标（24~64 px，PNG 或 SVG 皆可，建议透明底、浅灰色 #c9ccd3）

| 文件名 | 用途 | 显示位置 |
|---|---|---|
| `group_info.png` | 群资料 | 群聊窗口标题栏右侧第一个图标 |
| `group_activity.png` | 群活动 | 群聊窗口标题栏右侧第二个图标 |

替换方式：直接覆盖同名文件 → 重新编译（`resources.qrc` 会重新打包）。

新增图标时：
1. 放进本目录；
2. 在 `Client/resources/resources.qrc` 的 `/icons` 前缀下加一行 `<file alias="xxx.png">icons/xxx.png</file>`；
3. 代码里用 `QIcon(QStringLiteral(":/icons/xxx.png"))` 加载。

## 左侧图标栏（IconRail）

目前用**中文字符**（聊/圈/活/配/我）而不是图片，避免字体缺字形；如果以后要换成图片，
在 `MainWindow.cpp` 里把 `m_rail->addItem(...)` 的 glyph 参数改成图标资源路径即可
（需要给 `IconRail::addItem` 增加一个"按图标加载"的重载）。
