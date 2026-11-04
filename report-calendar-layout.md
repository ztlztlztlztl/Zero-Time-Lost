# Calendar 标签、选择与周视图调整

日期：2026-11-04  
分支：`v2.0-phase3-calendar`

本轮按对话确认修改三处交互，保留已有 Calendar 功能。

## 改动

- 全天标签只填写日期和内容，新增、编辑、详情不再显示备注；DDL 和区间事务保留备注。
- 日程点击卡片锁定唯一高亮，点击另一张切换；点击 Calendar 内空白、日期栏或其他控件清除。按后续澄清，只有已选中的卡片忽略额外悬停，其他卡片照常响应；选中区间与另一个悬停区间可同时显示，移开后只撤去临时效果。移除卡片虚线框，键盘焦点用边框表示。
- 周视图保留空白时段压缩，同时根据完整数据扩张繁忙时段。每小时取七天所需高度的最大值，取消 144 像素上限，全部事务节点单列排开，通过滚动查看。DDL 占一行，区间开始、结束各占一行；长标题省略，悬停查看全文。月视图保留原有溢出列表。

周布局先为完整数据分配文字位置，再按筛选条件隐藏节点；筛选后行高和剩余节点位置不变。按后续澄清，每个时段从顶部以固定行距按时间顺序排列，时间写在文字中，移除比例时间点与连接线。区间背景依据开始、结束文字的位置定位，“现在”线按节点时间顺序插入；绘制和点击命中共用卡片矩形。

需求与方案分别见 `docs/calendar-layout/01-需求理解.md`、`docs/calendar-layout/02-方案设计.md`，使用说明和 README 已更新。

## 验证

| 检查 | 结果 |
| --- | --- |
| Release 构建 | 通过 |
| CTest 全量回归 | 最新澄清修正后 10／10 组通过，约 41 秒 |
| 新增业务用例 | 标签无备注、日程选择与清除、繁忙周布局三项通过 |
| 密集布局 | 同一分钟 45 个节点、小时末与下一小时开头、00:00／23:59、同小时区间双端点、筛选前后位置、窄窗口和加大字体均通过 |
| 点击命中 | 在高行的开头、中间、末尾滚动后逐项打开，详情身份正确 |
| 缩放 | 125%、150%、200% 下三项新增业务检查均通过 |
| 截图 | 检查标签编辑器、日程高亮和密集周视图；使用 Qt offscreen 渲染 |

最新修正的记录位于 `build/calendar-layout-correction-results.txt`、`build/calendar-layout-correction-qa.txt`、`build/calendar-layout-correction-busy.txt`、`build/calendar-correction-scale-*.txt`，截图位于 `build/qa-calendar-layout-correction/`。完整回归记录位于 `build/Sidekick/Testing/Temporary/LastTest.log`。

本轮没有提交或推送 Git。

## 部署

你从托盘正常退出后，已用新 Release 包覆盖 `build/ZTL-package-recurrence/` 中的 103 个程序文件。逐文件 SHA256 与待验收包一致，原有 14 个数据文件在覆盖前后的相对路径与哈希相同；没有保留旧版程序副本。

正式包的 `--self-check`、`--self-check-mail` 均返回 0。自检使用独立数据目录及仅含 Windows 系统目录的 PATH，并清除开发环境的 Qt／Python 路径覆盖；自检后再次核对个人数据哈希，结果未变。

最新校验清单为 `build/calendar-correction-deployment-verification.json`，覆盖前数据清单为 `build/calendar-correction-data-before.json`。`build/ZTL-package-calendar-staging/` 保留新版验收包，前一轮自动审批已阻止清理该临时目录，本轮未重试删除。

正式程序入口为 `build/ZTL-package-recurrence/Z.T.L.exe`，SHA256：

```text
63CFE9D925F23115CA8B72F047244A0AF3862CC1C9095B32C18DFBADEDD6C868
```
