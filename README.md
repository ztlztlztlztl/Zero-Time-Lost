## Z.T.L-Zero-Time-Lost - 策划案

### 一、 功能蓝图 (Feature Map)

| 模块 | 核心功能 | 描述与预期效果 |
| --- | --- | --- |
| **主页 (Home)** | 氛围展示与情绪价值 | 流动线条背景 + 数字时钟 + 天气预报 + 每日一言 |
| **设置 (Set)** | 设置配置 | 左侧导航栏 + 右侧表单 |
| **邮件管理 (Mail)** | IMAP抓取 | 可以绑定多个邮箱，支持多账号轮询，读取正文。 |
| **DDL 管理 (Task)** | DDL同步与倒计时 | 时间轴视图。高亮即将到期的作业/考试，并在桌面右下角提供非侵入式弹窗提醒。 |
| **备忘知识 (Note)** | 富文本记录与AI标签 | 随手记与长文剪藏。支持 Markdown 渲染，支持通过 DeepSeek 提取关键词。 |
| **快捷导航 (Link)** | 常用系统与文档直达 | 聚合校园门户、常用网站、本地重要 PDF（如培养方案）的快捷入口。点击即调用系统默认程序打开。 |

---

### 二、 UI 与交互规范 (UI/UX Guidelines)

* **快捷键呼出功能：** 主界面默认全屏展示“主页”模块。通过快捷键，如alt + 数字来进入功能，通过 `QStackedWidget` 切换至对应界面。
* **切后台回首页：** 当app从前台切至后台时，自动回到主页
* **沉浸式后台常驻：** * **关闭即隐藏：** 拦截右上角的 `X` 关闭事件，改为隐藏主窗口。
* **系统托盘 (`QSystemTrayIcon`)：** 在 Windows 任务栏右下角常驻图标。右键菜单包含：打开主面板、一键同步数据、退出程序。

---

### 三、 技术架构设计 (Technical Architecture)

采用 **Qt (C++ GUI) + Python (数据抓取与逻辑) + DeepSeek (AI 分析)** 的混合架构。

**1. 数据存储方案**

* **配置文件 (`.json` / `.ini`)：** 仅存储轻量级的账号密码、UI主题设置。
* **本地数据库 (`SQLite`)：** 核心数据管家。使用 Qt 自带的 `QSqlDatabase`，建立 `ddl_tasks`、`memo_notes` 等数据表，解决后期数据量大时 JSON 读写缓慢的问题。

**2. 推荐的目录结构**

```text
ZTL_Agent/                  # 你的项目根目录
│
├── ZTL_Agent.exe           # Qt 编译出来的主程序
│
├── config/                 # 配置区
│   ├── settings.json       # 软件设置（比如：主题颜色、是否开机启动）
│   └── accounts.json       # 邮箱账号密码、api密钥等
│
├── data/                   # 核心记忆区
│   └── ztl_memory.db       # SQLite 数据库文件（存 DDL、备忘录）
│
├── cache/                  # 临时缓存区
│   └── mail_html/          # 存 Python 抓下来的长篇邮件正文
│
└── scripts/                # 工具人区（Python 脚本）
    ├── fetch_ddl.exe
    └── check_imap.exe
```

## Z.T.L-Zero-Time-Lost - 实现记录
