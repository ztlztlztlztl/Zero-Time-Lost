# Z.T.L v2.0

Z.T.L 是个人使用的 Windows 桌面工具，将时间、天气、邮件、日历、纸条、书签和加密秘密集中在一个应用中。v2.0 配套独立的服务器维护工具与自托管云端服务，客户端负责日常操作，云端在电脑关闭后继续收信和更新天气。

## 已实现的功能

| 模块 | 功能 |
| --- | --- |
| Home | 时间与日期、天气、今日全天标签、进行中的区间事务、DDL 和未读邮件提示；随天气与昼夜变化的动态海景 |
| Mail | 多账号 IMAP、定时与手动收信、历史摘要、正文按需下载、高级筛选、收藏、本地删除与恢复、转为日程或纸条 |
| Calendar | DDL、全天标签、同日区间事务；日程／周／月视图、有限循环、单次例外、完成／暂停、提醒、筛选和撤销 |
| Note | 普通纸条、网站／文件／文件夹书签、标签、拖动排序；主密码保护的秘密、自动锁定、加密恢复草稿和密库备份 |
| 云端 | HTTPS 访问、Calendar 与普通 Note 数据同步、离线待同步队列、冲突选择、后台 IMAP 任务、天气缓存和自动备份 |
| 服务器维护 | 独立选择服务端发布包、检测 SSH 指纹、首次部署、保留数据更新、重建、日志与状态、重启、备份／恢复、导出连接文件 |
| 桌面与数据 | 常驻侧边栏、黑底彩色主题、可关闭的动效、托盘、开机启动、窗口状态恢复、安装目录内存储、完整本地备份与恢复 |

邮件摘要保存成功后，收信端会把原邮箱对应邮件标为已读；Z.T.L 内的未读状态由阅读操作独立维护。区间事务只展示状态，不发提醒。全天标签只有日期和内容。

当前同步范围包含 Calendar、普通纸条、网站书签、排序、共享设置和邮件状态。本地文件／文件夹书签、窗口与设备设置留在本机。秘密密库和 Note 主密码不上传云端。v2.0 未提供模型助手、手机端或秘密跨设备同步。

## 三个发布包

项目根目录的 `v2-release/` 存放本次可部署版本：

| 发布文件 | 用途 |
| --- | --- |
| `ZTL-Desktop-2.0.0.zip` | Windows 客户端，解压后运行 `ZTL/Z.T.L.exe` |
| `ZTL-ServerManager-2.0.0.zip` | Windows 服务器维护工具，解压后运行 `ZTL-ServerManager/Z.T.L.ServerManager.exe` |
| `ztl-server-2.0.0.zip` | Linux 服务端程序，交给维护工具部署，保持 ZIP 原样选择 |

这些包不含个人配置、邮箱凭据、服务器地址、令牌、证书、数据库或测试数据。客户端与维护工具各自附带 Qt 和 Python 运行时；解压完整目录后即可启动。首次运行会在各自安装目录下创建 `data/`。

使用顺序：维护工具部署服务器并导出 `.ztlconn` → 客户端设置 Note 主密码并导入连接文件 → 在设置中补充天气与邮箱。具体操作见 [instruct.md](instruct.md)。`SHA256SUMS.txt` 列出三个 ZIP 的校验值。

发布包保存在本地，Git 仓库只跟踪源码、构建与测试脚本和两份说明。需要从源码重建时按下节操作。

## 源码结构

```text
.
├── desktop/       Windows Qt 客户端与客户端测试
├── deploy/        Windows 服务器维护工具与部署引擎
├── server/        FastAPI 服务、SQLite 存储、后台任务与服务端测试
├── shared/        共用邮件适配器、备份逻辑、版本和发布清单
├── scripts/       构建、运行时准备、打包、卸载与联调脚本
├── tests/         Python 回归测试
├── CMakeLists.txt
├── README.md
└── instruct.md
```

## 构建与验证

客户端和维护工具使用 C++17、Qt 6.7 以上及 CMake；本次 Windows 构建环境为 Qt 6.11.0 MinGW 64 位。脚本默认 Qt 安装位置为 `D:\Qt\6.11.0\mingw_64`，可通过 `-QtRoot` 或 `QT_ROOT` 修改。首次构建会下载并校验嵌入式 Python，维护工具还会准备 SSH 依赖。

在仓库根目录执行：

```powershell
.\desktop\build.ps1
.\deploy\build.ps1
.\desktop\package.ps1 -Destination D:\ZTL
.\deploy\package.ps1 -Destination D:\ZTL-ServerManager
python server/package.py --output D:\ztl-server-2.0.0.zip
```

`shared/product-version.txt` 是三个组件共用的版本号。Windows 构建分别输出到 `build/Desktop/Release` 和 `build/ServerManager/Release`。服务端发布脚本会校验 Docker 复制清单，并生成版本、协议和逐文件校验信息；维护工具独立选择发布版本。

运行测试：

```powershell
ctest --test-dir build/Desktop/Release --output-on-failure
ctest --test-dir build/ServerManager/Release --output-on-failure
python -m pip install -r server/requirements-dev.txt -r deploy/requirements-dev.txt
python -m pytest -q
python scripts/test_cloud_integration.py
```

Python 测试和 HTTPS 联调使用临时数据库与本机测试服务。开发时可用 `ZTL_DATA_DIR` 指定客户端临时数据目录；维护工具对应 `ZTL_MANAGER_DATA_DIR`。正常部署不需要设置这些变量。
