# G3519 电赛工程整合归档设计

日期:2026-08-10
状态:已获用户批准

## 背景

NUEDC-2026(SAIS@SJTU)比赛已结束。`D:\ClaudeCodeProject` 下遗留 4 个 G3519 工程,外加 GitHub 上 3 个相关仓库,需要整合清理:

| 本地目录 | git 历史 | 状态 |
|---|---|---|
| `G3519_control` | 5 commits(control_new 的祖先) | 旧 checkout,历史已被 control_new 完全包含 |
| `G3519_control_new` | 12 commits(起始 `9dd8244`) | 控制题最终版,当前工作目录 |
| `G3519_signal` | 多 commits(起始 `6c2ea5b`) | 信号题唯一工程 |
| `G3519_signal_new` | 无 | 空目录 |

| GitHub 仓库 | 创建时间 | 现状 |
|---|---|---|
| `shuidisjtu/G3519` | 2026-07-22 | 基础平台,仅 1 个提交 `60029d7`(main) |
| `shuidisjtu/NUEDC2026-G3519-Control` | 2026-07-26 | 控制题当前 remote |
| `shuidisjtu/NUEDC2026-Signal` | 2026-07-28 | 信号题当前 remote |

**关键事实**:用户最初规划是"G3519 一个仓库、控制题/信号题作为两个分支",实际执行变成了三个独立仓库。三条 git 历史(`60029d7`、`9dd8244` 线、`6c2ea5b` 线)互不相交,无共同祖先。

## 目标

1. 恢复最初规划:G3519 统一仓库,main(基础平台)/ control / signal 三个分支
2. 去重:删除 `G3519_control`(历史已包含)与空目录 `G3519_signal_new`
3. 比赛资料入 git 存档:`F:\电赛\备赛知识相关\3519\` 硬件手册(8 文件 12M)入 control 分支
4. 两工程的未提交内容(比赛文档、gen_test 代码)提交保存

## 非目标

- 不重组本地目录结构(目录名保持 `G3519_control_new` / `G3519_signal`,避免破坏 IAR/README 路径)
- 不处理 `2026TI`(E 题拼图装置,Py 工程)、`translate`(AD2 手册 PDF)
- 不重写任何 git 历史(三根历史以多根分支形式共存,git 原生支持)
- 不删除 GitHub 旧仓库,仅归档

## 目标结构

```
GitHub: shuidisjtu/G3519(默认分支 = main)
├── main      ← 基础平台原始状态(60029d7)+ 三分支结构说明提交
├── control   ← control_new 全部历史 + 硬件资料 + 比赛资料
└── signal    ← signal 全部历史 + 新文档
```

旧仓库 `NUEDC2026-G3519-Control` / `NUEDC2026-Signal` → GitHub archive(只读保留)。

## 本地布局(不变)

| 目录 | 分支 | 动作 |
|---|---|---|
| `G3519_control_new` | control | 保留,统一仓库 git 管理点 |
| `G3519_signal` | signal | 保留,remote 改指向 G3519 |
| `G3519_control` | — | 移回收站 |
| `G3519_signal_new` | — | 移回收站 |

## 执行步骤

1. **资料入库**:复制 `F:\电赛\备赛知识相关\3519\` 全部文件 → `empty_mspm0g3519/docs/development_reference/3519_hardware/`
2. **control_new 提交**(本地,不 push 旧仓库):
   - 3519_hardware 硬件手册 + 比赛文档(`2026_control_guess.md`、`2026_material_list.md`)+ `iar/gen_test/`
   - README/CLAUDE.md 中"从 G3519 拆分"文案更新
   - 整合设计文档
3. **signal 提交**(本地):未提交文档(新增/删除)+ 文案更新
4. **统一仓库**(以 control_new 为管理点):
   - fetch G3519 远端 → `main` 分支(60029d7)
   - fetch signal 本地仓库 → `signal` 分支
   - remote origin 改指向 `https://github.com/shuidisjtu/G3519.git`
   - 在 main 上加三分支结构说明提交
5. **push** main/control/signal 到 GitHub;`gh repo archive` 归档两个旧仓库;确认 G3519 默认分支 = main
6. **清理**:`G3519_control`、`G3519_signal_new` 移回收站
7. **验证**:
   - 三分支就位、内容正确(control 含最新控制题代码,signal 含最新信号题代码)
   - 两个工作目录 `git status` 干净,remote 指向 G3519
   - 目录清单只剩预期内容
   - GitHub 上三分支可访问、默认分支正确

## 风险与保护

- 多根历史:git 原生支持,零重写、零数据丢失
- 删除目录走回收站,可恢复
- 已确认 `G3519_control` 无独有文件(跟踪文件全部包含于 control_new)
- 归档保留链接,旧链接不失效
