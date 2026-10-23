# 暂停检查点与恢复计划

保存日期：2026-10-24。用户明确要求暂停目标；目标“完成迁移计划”尚未完成。暂停期间不继续构建、测试或发布。

## 已确认的进度

- 现代 C++23 / Win32 外壳保留 PuTTY 生态组合路线：Plink 后端，HwndTerminal 主前端，ConEmu 兼容前端，独立 WinSCP 从活动 SSH 快照启动；不承诺复用同一 TCP 连接。
- 会话、分组、导入导出、密码库、受限密码通道、Pageant、原生工具入口、生命周期与配置恢复已有实现和分项证据。核心检查最近为 58 项。
- 操作者已在原生 Pageant 中解锁一次性加密 PPK；Hub 与 WinSCP 代理认证五项通过：evidence/app-agent-encrypted-integration.json。
- 原生 Windows Vim 和 MSYS Vim 经本地测试 ConPTY 桥，文件显示、96×30 网格、编辑保存、退出各四项通过：evidence/tui-host-native-results.json、evidence/tui-host-local-results.json。它们不等于 SSH / HwndTerminal 完整验收。
- 普通连接、认证等待取消、上游退出、原生工具关闭边界已有分项通过证据。modern / ConEmu / 自动回退静默丢弃 SSH 数据测试各九项通过；不覆盖真实系统 TCP 重传超时或自动死连接检测。

## 停在何处

新增 tests/vim-fixture.py，修改 tests/ssh-fixture.py、tests/run-app-tests.ps1 与 src/app.cpp，把真实 Vim 经本地 SSH 假服务器、固定 Plink、Hub HwndTerminal 串联。C++ 编译及 Python 语法检查通过；完整 SSH Vim 运行验收尚未执行。

该测试拟验证真实文件输出、窗口尺寸传递至 Vim、编辑保存、SSH 程序退出与 Plink 清理。最近一次启动被自动审批额度拦截（提示 Oct 24, 2026 1:24 AM 重试），不是测试失败，没有通过结果。暂停时没有新启动测试；没有已知仍运行的测试任务。

## 恢复顺序

1. 用户恢复目标后，先运行下面的 SSH Vim 测试并检查 evidence/app-vim-integration.json；失败则诊断实际输出与 fixture 事件，保留失败证据。现有 bin/tui-host.exe 已构建；源码变化时先用 build.ps1 -TuiHost 重建。
2. 通过后更新验收记录，按需要扩展 ConEmu 全屏程序路径。不要以本地 Vim 通过替代完整链路。
3. 补齐真实交互与环境验收：IME、鼠标、剪贴板、跨屏 DPI、全屏视觉、会话/密码/迁移对话框、旧 Windows 与系统网络超时。需要人工完成的终端及认证界面保留人工验收，不用编译结果替代。
4. 将新增测试接入独立源码验证，运行当前 verify-source.ps1 全套；按原始 CPP-MIGRATION-PLAN.md 逐项审计缺口。当前验证脚本要求的报告数已增至 23，完整新版尚未重跑。
5. 仅在当前输入、验证二进制与证据一致后重新打包、核对源码包及便携包。原始迁移目标未全部满足前，不替换旧默认入口、不标记完成。

恢复后的首个运行命令（PowerShell，工作目录 D:\Projects\rea\liteops）：

```powershell
./tests/run-app-tests.ps1 -Python 'C:/Users/Neo/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -Vim 'D:/Projects/rea/liteops/tests/tools/vim-9.2.0000-x64/vim/vim92/vim.exe'
```

## 交付与证明边界

evidence/source-proof-currentness.json 当前为 stale；后续黑洞网络与 Vim 修改没有获得完整新源码证明，不能复用旧证明发布。

最近预览归档仍为 publish/PuTTYHub-native-preview-portable-20261023-151155.zip 与 publish/PuTTYHub-native-preview-source-20261023-151155.zip；不包含后续全部修改。它们是历史预览，不是当前源码的最终交付。

MIGRATION-STATUS.md 下方包含按时间累积的历史记录，旧失败/阻塞状态以顶部最新说明和本检查点为准。完整阶段审计仍为 incomplete。此工作区没有 Git 提交记录可供声称已提交；进度保存为本地文件。
