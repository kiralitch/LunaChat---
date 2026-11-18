# -*- coding: utf-8 -*-
"""生成论文/答辩用插图（Pillow 绘制，输出 PNG 到 docs/paper/figures）。

图：
  01_architecture.png   系统总体架构
  02_frame_format.png   通信帧格式与拆包流程
  03_er.png             数据库 ER 图（16 张表）
  04_activity_state.png 群活动状态机
  05_match_state.png    随机匹配状态机

重新生成：python tools/make_paper_figures.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "paper", "figures")
os.makedirs(OUT, exist_ok=True)

FONT_DIR = "C:/Windows/Fonts"
INK = (30, 34, 42)
MUTED = (98, 104, 116)
LINE = (120, 128, 142)
WHITE = (255, 255, 255)

BLUE = (67, 105, 168)
BLUE_FILL = (233, 240, 252)
GREEN = (54, 132, 96)
GREEN_FILL = (231, 245, 237)
ORANGE = (188, 120, 46)
ORANGE_FILL = (253, 243, 228)
PURPLE = (122, 86, 186)
PURPLE_FILL = (243, 237, 251)
RED = (176, 74, 74)
RED_FILL = (252, 236, 236)
GRAY_FILL = (247, 248, 250)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONT_DIR, name), size)


F_TITLE = font("msyhbd.ttc", 42)
F_H2 = font("msyhbd.ttc", 28)
F_BOX = font("msyhbd.ttc", 23)
F_TEXT = font("msyh.ttc", 20)
F_SMALL = font("msyh.ttc", 17)
F_TINY = font("msyh.ttc", 15)
F_MONO = font("consola.ttf", 18)
F_MONO_CJK = font("simsun.ttc", 19)


def canvas(w, h):
    img = Image.new("RGB", (w, h), WHITE)
    d = ImageDraw.Draw(img)
    return img, d


def text_w(d, text, f):
    return d.textbbox((0, 0), text, font=f)[2]


def wrap(d, text, f, max_w):
    """按宽度断行；中文没有空格，逐字符累加。"""
    lines, cur = [], ""
    for ch in text:
        if ch == "\n":
            lines.append(cur)
            cur = ""
            continue
        if text_w(d, cur + ch, f) > max_w and cur:
            lines.append(cur)
            cur = ch
        else:
            cur += ch
    if cur:
        lines.append(cur)
    return lines


def center_text(d, rect, text, f, fill=INK):
    x0, y0, x1, y1 = rect
    bb = d.textbbox((0, 0), text, font=f)
    d.text((x0 + (x1 - x0 - (bb[2] - bb[0])) / 2 - bb[0],
            y0 + (y1 - y0 - (bb[3] - bb[1])) / 2 - bb[1]), text, font=f, fill=fill)


def label(d, xy, text, f, fill=INK):
    d.text(xy, text, font=f, fill=fill)


def rrect(d, rect, r=12, fill=None, outline=None, w=2):
    d.rounded_rectangle(rect, radius=r, fill=fill, outline=outline, width=w)


def panel(d, rect, title, accent, fill):
    """带标题条的分组容器。"""
    rrect(d, rect, r=16, fill=fill, outline=accent, w=2)
    label(d, (rect[0] + 20, rect[1] + 10), title, F_H2, accent)


def head_box(d, rect, title, accent, body_lines, body_font=F_TEXT, line_h=27,
             fill=WHITE, min_h=None):
    """标题条 + 正文行的卡片。"""
    x0, y0, x1, y1 = rect
    if min_h:
        y1 = max(y1, y0 + min_h)
    rrect(d, (x0, y0, x1, y1), r=10, fill=fill, outline=accent, w=2)
    d.rounded_rectangle((x0, y0, x1, y0 + 42), radius=10, fill=accent)
    d.rectangle((x0, y0 + 30, x1, y0 + 42), fill=accent)
    center_text(d, (x0, y0, x1, y0 + 42), title, F_BOX, WHITE)
    y = y0 + 54
    for ln in body_lines:
        label(d, (x0 + 16, y), ln, body_font, INK)
        y += line_h
    return (x0, y0, x1, y1)


def arrow(d, p1, p2, color=LINE, w=3, head=13):
    d.line([p1, p2], fill=color, width=w)
    import math
    ang = math.atan2(p2[1] - p1[1], p2[0] - p1[0])
    for s in (-1, 1):
        a = ang + s * 0.42
        d.line([p2, (p2[0] - head * math.cos(a), p2[1] - head * math.sin(a))], fill=color, width=w)


def arrow_box(d, a, b, color=LINE, w=3):
    """在两个矩形之间画带箭头的水平/垂直连线（自动挑最近的一对边）。"""
    ax0, ay0, ax1, ay1 = a
    bx0, by0, bx1, by1 = b
    acx, acy = (ax0 + ax1) / 2, (ay0 + ay1) / 2
    bcx, bcy = (bx0 + bx1) / 2, (by0 + by1) / 2
    if abs(bcx - acx) > abs(bcy - acy):
        if bcx > acx:
            arrow(d, (ax1, acy), (bx0, bcy), color, w)
        else:
            arrow(d, (ax0, acy), (bx1, bcy), color, w)
    else:
        if bcy > acy:
            arrow(d, (acx, ay1), (bcx, by0), color, w)
        else:
            arrow(d, (acx, ay0), (bcx, by1), color, w)


# --------------------------------------------------------------------------- 架构图
def architecture():
    W, H = 2400, 1620
    img, d = canvas(W, H)

    label(d, (60, 34), "LunaChat 系统总体架构", F_TITLE, INK)
    label(d, (60, 92), "客户端 / 服务端 / 数据库三层结构，TCP 长连接 + JSON 帧通信",
          F_TEXT, MUTED)

    # ---- 客户端
    panel(d, (60, 140, 2340, 620), "① 客户端 Client.exe（Qt 5.15 · C++17 · Qt Widgets）", BLUE, BLUE_FILL)
    ui = head_box(d, (90, 215, 900, 590), "界面层：窗口与页面", BLUE, [
        "LoginWindow  登录 / 内嵌注册卡片",
        "MainWindow  会话列表 + 聊天区",
        "ChatPage  气泡 / 表情 / 图片 / 语音条",
        "MomentPage  朋友圈   MatchPage  随机匹配",
        "ActivityPage  群活动   GroupFilesPage  共享文件夹",
        "ProfilePage / GroupInfoPage / SettingsPage",
    ])
    net = head_box(d, (940, 215, 1660, 590), "客户端服务层", BLUE, [
        "LunaClient：QTcpSocket + FrameCodec",
        "        心跳 15s / 断线退避重连",
        "Recorder：QAudioInput 采集 16kHz PCM",
        "SoundPlayer：sounds/<kind>.* 统一播放",
        "文件 / 图片落盘：received/ avatars/ moments/",
        "AppSettings：主题 / 音效 / 录音设备",
    ], line_h=44)
    res = head_box(d, (1700, 215, 2310, 590), "资源与本地数据", BLUE, [
        "resources.qrc：QSS 主题 + 图标",
        "sounds/：message online match activity",
        "backgrounds/：聊天与朋友圈背景",
        "received/：接收的文件与语音",
        "avatars/ · moments/：头像与图片",
    ], line_h=58)

    # ---- 传输层
    rrect(d, (420, 680, 1980, 760), r=40, fill=(240, 244, 250), outline=BLUE, w=2)
    center_text(d, (420, 680, 1980, 760),
                "TCP 长连接 127.0.0.1:8888   ｜   帧 = 4 字节大端长度 + UTF-8 JSON（≤1MB）   ｜   心跳 15s / 45s 判离线",
                F_TEXT, INK)
    arrow(d, (1200, 620), (1200, 680), BLUE)
    arrow(d, (1200, 760), (1200, 820), BLUE)

    # ---- 服务端
    panel(d, (60, 820, 2340, 1230), "② 服务端 Server.exe（QTcpServer · 单进程多会话）", GREEN, GREEN_FILL)

    srv = head_box(d, (90, 895, 700, 1200), "网络层 LunaServer", GREEN, [
        "QTcpServer 监听 8888",
        "ClientSession：读缓冲 + 拆包",
        "FrameTooLarge / InvalidJson 断开",
        "登录态 userId 绑定会话",
    ], line_h=44)
    disp = head_box(d, (740, 895, 1140, 1200), "分发层", GREEN, [
        "MessageDispatcher",
        "按 type 路由到 Handler",
        "未登记类型回 error",
    ], line_h=44)
    handlers = head_box(d, (1180, 895, 1700, 1200), "业务层 Handlers ×9", GREEN, [
        "Chat / Friend / Group / File",
        "Voice / Moment / Match",
        "Activity / Profile",
        "组装回包与推送帧",
    ], line_h=44)
    db = head_box(d, (1740, 895, 2310, 1200), "数据访问层 DbManager", GREEN, [
        "QSqlDatabase 单例 + 预编译 SQL",
        "用户 / 群 / 消息 / 活动 / 朋友圈",
        "事务写入，返回值统一错误串",
        "在线用户与文件在线表",
    ], line_h=44)
    for a, b in ((srv, disp), (disp, handlers), (db, handlers)):
        arrow_box(d, a, b, GREEN)
    arrow(d, (1200, 760), (1200, 820), BLUE)

    # ---- 数据库
    panel(d, (60, 1290, 2340, 1580), "③ 数据层", ORANGE, ORANGE_FILL)
    head_box(d, (90, 1355, 1500, 1560), "MySQL 8.x · luna_chat", ORANGE, [
        "16 张表：user / friend / chat_group / group_member / message / offline_message",
        "file_record / voice_record / login_log / group_activity / activity_signup",
        "moment / moment_image / moment_like / moment_comment / match_session",
    ], body_font=F_SMALL, line_h=34)
    head_box(d, (1540, 1355, 2310, 1560), "磁盘目录（服务端）", ORANGE, [
        "received/ 语音与文件　avatars/ 头像",
        "moments/ 朋友圈图片",
        "group_files/ 群共享文件夹",
    ], body_font=F_SMALL, line_h=34)
    arrow(d, (1440, 1200), (1440, 1290), ORANGE)

    img.save(os.path.join(OUT, "01_architecture.png"), quality=95)


# --------------------------------------------------------------------------- 帧格式
def frame_format():
    W, H = 2000, 1320
    img, d = canvas(W, H)

    label(d, (60, 30), "通信帧格式与拆包流程", F_TITLE, INK)
    label(d, (60, 90), "所有业务消息都封装成同一格式：长度前缀保证按帧读取，JSON 负载便于扩展字段",
          F_TEXT, MUTED)

    # 字节布局
    x0, y0, x1, y1 = 120, 170, 1880, 285
    split = x0 + 300
    rrect(d, (x0, y0, split, y1), r=8, fill=(226, 236, 252), outline=BLUE, w=3)
    rrect(d, (split, y0, x1, y1), r=8, fill=(232, 246, 238), outline=GREEN, w=3)
    center_text(d, (x0, y0, split, y1), "4 字节长度 L\n（大端 uint32）", F_BOX, BLUE)
    center_text(d, (split, y0, x1, y1), "L 字节 JSON 负载（UTF-8）", F_BOX, GREEN)

    label(d, (x0, y1 + 8), "偏移 0", F_SMALL, MUTED)
    label(d, (split + 8, y1 + 8), "偏移 4", F_SMALL, MUTED)
    label(d, (x1 - 120, y1 + 8), "偏移 4+L-1", F_SMALL, MUTED)

    # JSON 示例
    json_lines = [
        "{",
        '  "type": "chat_group",',
        '  "ts": 1731840000000,',
        '  "data": {',
        '    "groupId": 1,',
        '    "content": "晚上一起自习吗？"',
        "  }",
        "}",
    ]
    rrect(d, (120, 360, 980, 760), r=12, fill=(248, 249, 251), outline=INK, w=2)
    label(d, (140, 372), "负载示例（一帧聊消息）", F_H2, INK)
    y = 430
    for ln in json_lines:
        label(d, (150, y), ln, F_MONO_CJK, (40, 44, 52))
        y += 38

    # 约束
    rrect(d, (1060, 360, 1880, 760), r=12, fill=(253, 246, 240), outline=ORANGE, w=2)
    label(d, (1080, 372), "协议约束", F_H2, ORANGE)
    rules = [
        "• 单帧负载 ≤ 1MB（kMaxFrameBytes），超限直接断开",
        "• 负载必须是 JSON 对象；解析失败回 error 后断开",
        "• 文件 / 语音按 64KB 原始数据分块，base64 后 ≈87KB",
        "• 必填字段：type（字符串）、data（对象）",
        "• 时间戳 ts 为服务端毫秒时间，客户端只读",
        "• 未知 type 由分发层统一回 error，不崩溃",
    ]
    y = 430
    for r in rules:
        label(d, (1090, y), r, F_TEXT, INK)
        y += 48

    # 拆包流程
    label(d, (120, 830), "拆包流程（FrameCodec：半包 / 粘包都在这里消化）", F_H2, INK)
    flow = [
        ("socket readyRead", "把裸字节 append 进缓冲区"),
        ("nextFrame() 循环", "取出完整帧或返回 NeedMoreData"),
        ("FrameDecoded", "emit frameReceived(JSON)"),
        ("FrameTooLarge / InvalidJson", "记录日志并断开连接"),
    ]
    fx = 120
    boxes = []
    for title, sub in flow:
        rrect(d, (fx, 890, fx + 400, 1010), r=10, fill=GRAY_FILL, outline=LINE, w=2)
        center_text(d, (fx, 900, fx + 400, 945), title, F_BOX, INK)
        center_text(d, (fx, 945, fx + 400, 1005), sub, F_SMALL, MUTED)
        boxes.append((fx, 890, fx + 400, 1010))
        fx += 450
    for i in range(3):
        arrow(d, (boxes[i][2], 950), (boxes[i + 1][0], 950), LINE)

    label(d, (120, 1090), "一次 readyRead 可能只有半帧，也可能粘了多帧：缓冲区累积 + 长度前缀即可确定边界。",
          F_TEXT, MUTED)
    label(d, (120, 1140), "HTTP 式的文本协议需要自己找分隔符，这里用定长前缀，解析成本 O(1)，也避免了粘包拆错。",
          F_TEXT, MUTED)

    img.save(os.path.join(OUT, "02_frame_format.png"), quality=95)


# --------------------------------------------------------------------------- ER
ENTITIES = [
    # key, 标题, 字段, 颜色
    ("user", "user 用户", [
        "PK  id            BIGINT",
        "UK  username      VARCHAR(32)",
        "    password      CHAR(64)",
        "    nickname      VARCHAR(32)",
        "    avatar_path   VARCHAR(255)",
        "    status        ENUM",
        "    created_at    DATETIME",
    ], BLUE),
    ("friend", "friend 好友关系", [
        "PK  id            BIGINT",
        "FK  user_id       BIGINT",
        "FK  friend_id     BIGINT",
        "    status        ENUM(pending/accepted)",
        "    created_at    DATETIME",
    ], BLUE),
    ("login_log", "login_log 登录日志", [
        "PK  id            BIGINT",
        "FK  user_id       BIGINT",
        "    login_time    DATETIME",
        "    ip            VARCHAR(45)",
    ], BLUE),
    ("match_session", "match_session 匹配会话", [
        "PK  id            BIGINT",
        "UK  session_id    VARCHAR(64)",
        "FK  user_a        BIGINT",
        "FK  user_b        BIGINT",
        "    mode          ENUM(text/voice)",
        "    status        ENUM(active/ended)",
        "    started_at    DATETIME",
        "    ended_at      DATETIME",
    ], PURPLE),
    ("chat_group", "chat_group 群组", [
        "PK  id            BIGINT",
        "    group_name    VARCHAR(64)",
        "FK  owner_id      BIGINT",
        "    avatar_path   VARCHAR(512)",
        "    announcement  TEXT",
        "    folder_path   VARCHAR(512)",
        "    created_at    DATETIME",
    ], GREEN),
    ("group_member", "group_member 群成员", [
        "PK  id            BIGINT",
        "FK  group_id      BIGINT",
        "FK  user_id       BIGINT",
        "    role          ENUM(owner/admin/member)",
        "    joined_at     DATETIME",
    ], GREEN),
    ("group_activity", "group_activity 群活动", [
        "PK  id            BIGINT",
        "FK  group_id      BIGINT",
        "FK  creator_id    BIGINT",
        "    title         VARCHAR(128)",
        "    description   TEXT",
        "    location      VARCHAR(128)",
        "    start_time    DATETIME",
        "    end_time      DATETIME",
        "    signup_deadline DATETIME",
        "    max_participants INT",
        "    status        ENUM(published/ongoing/ended/cancelled)",
        "    created_at    DATETIME",
    ], GREEN),
    ("activity_signup", "activity_signup 报名", [
        "PK  id            BIGINT",
        "FK  activity_id   BIGINT",
        "FK  user_id       BIGINT",
        "    status        ENUM(signed/cancelled)",
        "    signed_at     DATETIME",
        "    cancelled_at  DATETIME",
    ], GREEN),
    ("message", "message 消息", [
        "PK  id            BIGINT",
        "FK  from_user_id  BIGINT",
        "FK  to_user_id    BIGINT",
        "FK  group_id      BIGINT",
        "    msg_type      ENUM(text/file/voice)",
        "    content       TEXT",
        "    file_id       VARCHAR(64)",
        "    voice_id      VARCHAR(64)",
        "    created_at    DATETIME",
    ], ORANGE),
    ("offline_message", "offline_message 离线消息", [
        "PK  id            BIGINT",
        "FK  to_user_id    BIGINT",
        "    payload_json  TEXT",
        "    created_at    DATETIME",
        "    delivered     TINYINT(1)",
    ], ORANGE),
    ("file_record", "file_record 文件记录", [
        "PK  id            BIGINT",
        "UK  file_id       VARCHAR(64)",
        "    file_name     VARCHAR(255)",
        "    file_size     BIGINT",
        "FK  from_user_id  BIGINT",
        "FK  to_user_id    BIGINT",
        "FK  group_id      BIGINT",
        "    saved_path    VARCHAR(512)",
        "    created_at    DATETIME",
    ], ORANGE),
    ("voice_record", "voice_record 语音记录", [
        "PK  id            BIGINT",
        "UK  voice_id      VARCHAR(64)",
        "FK  from_user_id  BIGINT",
        "FK  to_user_id    BIGINT",
        "FK  group_id      BIGINT",
        "    session_id    VARCHAR(64)",
        "    file_name     VARCHAR(255)",
        "    duration_ms   INT",
        "    file_size     BIGINT",
        "    format        VARCHAR(8)",
        "    bitrate       INT",
        "    saved_path    VARCHAR(512)",
        "    created_at    DATETIME",
    ], ORANGE),
    ("moment", "moment 朋友圈", [
        "PK  id            BIGINT",
        "FK  user_id       BIGINT",
        "    content       TEXT",
        "    created_at    DATETIME",
        "    deleted       TINYINT(1)",
    ], PURPLE),
    ("moment_image", "moment_image 图片", [
        "PK  id            BIGINT",
        "FK  moment_id     BIGINT",
        "    image_path    VARCHAR(512)",
        "    sort_order    INT",
    ], PURPLE),
    ("moment_like", "moment_like 点赞", [
        "PK  id            BIGINT",
        "FK  moment_id     BIGINT",
        "FK  user_id       BIGINT",
        "    created_at    DATETIME",
    ], PURPLE),
    ("moment_comment", "moment_comment 评论", [
        "PK  id            BIGINT",
        "FK  moment_id     BIGINT",
        "FK  user_id       BIGINT",
        "    content       VARCHAR(500)",
        "    created_at    DATETIME",
    ], PURPLE),
]

# 逻辑外键关系（schema 未建物理外键，答辩时说明"逻辑外键"）
RELATIONS = [
    ("user", "friend", "user_id / friend_id  1:N"),
    ("user", "login_log", "user_id"),
    ("user", "match_session", "user_a / user_b"),
    ("user", "chat_group", "owner_id"),
    ("user", "group_member", "user_id"),
    ("chat_group", "group_member", "group_id"),
    ("chat_group", "group_activity", "group_id"),
    ("user", "group_activity", "creator_id"),
    ("group_activity", "activity_signup", "activity_id"),
    ("user", "activity_signup", "user_id"),
    ("user", "message", "from_user_id / to_user_id"),
    ("chat_group", "message", "group_id"),
    ("user", "offline_message", "to_user_id"),
    ("user", "file_record", "from_user_id / to_user_id"),
    ("chat_group", "file_record", "group_id"),
    ("user", "voice_record", "from_user_id"),
    ("user", "moment", "user_id"),
    ("moment", "moment_image", "moment_id"),
    ("moment", "moment_like", "moment_id"),
    ("moment", "moment_comment", "moment_id"),
    ("user", "moment_like", "user_id"),
    ("user", "moment_comment", "user_id"),
]


def er_diagram():
    W, H = 2400, 2180
    img, d = canvas(W, H)

    label(d, (60, 30), "LunaChat 数据库 ER 图（luna_chat，16 张表）", F_TITLE, INK)
    label(d, (60, 92), "schema.sql 中未建物理外键，图中连线为逻辑外键；主键 PK、唯一键 UK、逻辑外键 FK",
          F_TEXT, MUTED)

    cols = [60, 620, 1180, 1740]
    rows = [150, 640, 1130, 1620]
    box_w = 500
    boxes = {}
    for i, (key, title, fields, accent) in enumerate(ENTITIES):
        c, r = i % 4, i // 4
        x0 = cols[c]
        y0 = rows[r]
        # 高度按字段数
        h = 54 + len(fields) * 26 + 16
        rect = (x0, y0, x0 + box_w, y0 + h)
        boxes[key] = rect

    # 先画关系线（会被后画的表盖住，只在空隙里露出）
    for a, b, _ in RELATIONS:
        ra, rb = boxes[a], boxes[b]
        ca = ((ra[0] + ra[2]) / 2, (ra[1] + ra[3]) / 2)
        cb = ((rb[0] + rb[2]) / 2, (rb[1] + rb[3]) / 2)
        pa = edge_point(ca, cb, ra)
        pb = edge_point(cb, ca, rb)
        d.line([pa, pb], fill=(196, 202, 214), width=2)

    # 再画表
    for i, (key, title, fields, accent) in enumerate(ENTITIES):
        rect = boxes[key]
        x0, y0, x1, y1 = rect
        rrect(d, rect, r=10, fill=WHITE, outline=accent, w=2)
        d.rounded_rectangle((x0, y0, x1, y0 + 44), radius=10, fill=accent)
        d.rectangle((x0, y0 + 32, x1, y0 + 44), fill=accent)
        center_text(d, (x0, y0, x1, y0 + 44), title, F_BOX, WHITE)
        y = y0 + 54
        for f in fields:
            d.text((x0 + 14, y), f, font=F_SMALL, fill=INK)
            y += 26

    # 图例
    legend_y = 2000
    x = 60
    for text, sl, fl in (("用户域", BLUE, BLUE_FILL), ("群组域", GREEN, GREEN_FILL),
                         ("内容域", ORANGE, ORANGE_FILL), ("社交/匹配域", PURPLE, PURPLE_FILL)):
        rrect(d, (x, legend_y, x + 30, legend_y + 22), r=5, fill=sl, outline=sl)
        label(d, (x + 40, legend_y - 2), text, F_SMALL, INK)
        x += 170
    label(d, (x + 40, legend_y - 2), "—— 逻辑外键（未建物理约束，便于演示环境清库重建）", F_SMALL, MUTED)

    img.save(os.path.join(OUT, "03_er.png"), quality=95)


def edge_point(center, other, rect):
    """从矩形中心朝 other 方向射出，求与矩形边界的交点。"""
    cx, cy = center
    dx, dy = other[0] - cx, other[1] - cy
    x0, y0, x1, y1 = rect
    ts = []
    if dx > 0:
        ts.append((x1 - cx) / dx)
    elif dx < 0:
        ts.append((x0 - cx) / dx)
    if dy > 0:
        ts.append((y1 - cy) / dy)
    elif dy < 0:
        ts.append((y0 - cy) / dy)
    t = min(t for t in ts if t > 0)
    return (cx + dx * t, cy + dy * t)


# --------------------------------------------------------------------------- 活动状态机
def activity_state():
    W, H = 1900, 1220
    img, d = canvas(W, H)
    label(d, (60, 30), "群活动状态机", F_TITLE, INK)
    label(d, (60, 92), "状态存于 group_activity.status；流转由服务端每分钟巡检 + 管理员操作共同驱动",
          F_TEXT, MUTED)

    def state(rect, title, sub, color, fill, title_color=None):
        rrect(d, rect, r=18, fill=fill, outline=color, w=3)
        center_text(d, (rect[0], rect[1] + 10, rect[2], rect[1] + 60), title, F_H2, title_color or color)
        center_text(d, (rect[0], rect[1] + 60, rect[2], rect[3] - 6), sub, F_SMALL, MUTED)

    published = (160, 220, 520, 340)
    ongoing = (780, 220, 1140, 340)
    ended = (1400, 220, 1760, 340)
    cancelled = (780, 620, 1140, 740)

    state(published, "已发布 published", "可报名 / 可取消", GREEN, GREEN_FILL)
    state(ongoing, "进行中 ongoing", "到开始时间自动流转", BLUE, BLUE_FILL)
    state(ended, "已结束 ended", "终态，不再接受操作", LINE, (245, 246, 248), (80, 84, 92))
    state(cancelled, "已取消 cancelled", "终态，保留记录", RED, RED_FILL)

    arrow(d, (520, 280), (770, 280), GREEN)
    label(d, (545, 226), "start_time 到达\n（巡检，每分钟一次）", F_SMALL, GREEN)

    arrow(d, (1140, 280), (1390, 280), BLUE)
    label(d, (1180, 226), "end_time 到达（巡检）", F_SMALL, BLUE)

    arrow(d, (330, 340), (760, 610), RED)
    label(d, (180, 450), "管理员取消\n（published / ongoing 均可）", F_SMALL, RED)

    arrow(d, (960, 340), (960, 610), RED)
    label(d, (980, 440), "管理员取消\nactivity_cancel", F_SMALL, RED)

    # 报名子状态
    label(d, (150, 800), "报名子状态（activity_signup.status）", F_H2, INK)

    ns = (330, 860, 620, 960)
    ss = (730, 860, 1020, 960)
    cs = (1130, 860, 1420, 960)
    for rect, t in ((ns, "未报名"), (ss, "已报名 signed"), (cs, "已取消 cancelled")):
        rrect(d, rect, r=12, fill=GRAY_FILL, outline=LINE, w=2)
        center_text(d, rect, t, F_TEXT, INK)
    arrow(d, (620, 910), (720, 910), GREEN)
    label(d, (632, 870), "signup", F_SMALL, GREEN)
    arrow(d, (1020, 910), (1120, 910), RED)
    label(d, (1032, 870), "signup_cancel", F_SMALL, RED)

    # 取消后可重新报名：走底部绕回
    d.line([(1275, 960), (1275, 1010)], fill=(150, 150, 150), width=2)
    d.line([(1275, 1010), (875, 1010)], fill=(150, 150, 150), width=2)
    arrow(d, (875, 1010), (875, 965), (150, 150, 150))
    label(d, (1000, 1018), "重新报名", F_SMALL, MUTED)

    label(d, (150, 1080), "约束：仅群主/管理员可发布与取消；报名要求 status=published 且未超 max_participants。",
          F_SMALL, MUTED)

    img.save(os.path.join(OUT, "04_activity_state.png"), quality=95)


# --------------------------------------------------------------------------- 匹配状态机
def match_state():
    W, H = 1900, 1020
    img, d = canvas(W, H)
    label(d, (60, 30), "随机匹配状态机", F_TITLE, INK)
    label(d, (60, 92), "客户端页面状态与 match_* 帧一一对应；服务端按文本 / 语音两条队列配对",
          F_TEXT, MUTED)

    def state(rect, title, sub, color, fill, title_color=None):
        rrect(d, rect, r=18, fill=fill, outline=color, w=3)
        center_text(d, (rect[0], rect[1] + 10, rect[2], rect[1] + 60), title, F_H2, title_color or color)
        center_text(d, (rect[0], rect[1] + 60, rect[2], rect[3] - 6), sub, F_SMALL, MUTED)

    idle = (140, 200, 520, 320)
    wait = (720, 200, 1100, 320)
    active = (1320, 200, 1700, 320)
    back = (720, 680, 1100, 800)

    state(idle, "空闲 idle", "可发起匹配 / 浏览", LINE, (245, 246, 248), (80, 84, 92))
    state(wait, "排队中 searching", "最长等待 15s", ORANGE, ORANGE_FILL)
    state(active, "会话中 active", "可收发 match_message", GREEN, GREEN_FILL)
    state(back, "回到空闲", "超时 / 取消 / 会话结束", LINE, (245, 246, 248), (80, 84, 92))

    arrow(d, (520, 260), (710, 260), ORANGE)
    label(d, (545, 212), "match_start(mode)", F_SMALL, ORANGE)

    arrow(d, (1100, 260), (1310, 260), GREEN)
    label(d, (1128, 206), "match_result\n（服务端配对成功）", F_SMALL, GREEN)

    arrow(d, (910, 320), (910, 670), RED)
    label(d, (930, 430), "match_timeout\n（15s 无人 / match_cancel）", F_SMALL, RED)

    d.line([(1510, 320), (1510, 740), (1110, 740)], fill=(140, 146, 158), width=3)
    arrow(d, (1110, 740), (1100, 740), (140, 146, 158))
    label(d, (1530, 470), "match_end\n（任一方结束 / 掉线）", F_SMALL, MUTED)

    label(d, (140, 880), "注：超时与取消复用 match_timeout 帧，客户端据此提示“暂时没匹配到人，可以再试一次”。",
          F_SMALL, MUTED)
    label(d, (140, 925), "服务端在任一方离线时结束会话并通知对方，避免“对着空气聊天”。", F_SMALL, MUTED)

    img.save(os.path.join(OUT, "05_match_state.png"), quality=95)


if __name__ == "__main__":
    architecture()
    frame_format()
    er_diagram()
    activity_state()
    match_state()
    print("figures written to", OUT)
    for name in sorted(os.listdir(OUT)):
        print(" ", name, os.path.getsize(os.path.join(OUT, name)) // 1024, "KB")
