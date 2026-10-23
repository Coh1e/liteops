# 迁移验收状态

更新：2026-10-24。用户要求暂停目标；当前未达到交付完成条件。进度、证据边界和下一步命令保存于 RESUME-PLAN.md；暂停后不继续测试或发布。下方旧阻塞和失败段落属于历史记录，以本段最新状态为准。

最新代码：真实 Vim 经 SSH → Plink → Hub HwndTerminal 的测试夹具及应用验收分支已实现，C++ 编译和 Python 语法检查通过，运行验收尚未执行（最近启动被自动审批额度拦截，并非测试失败）。本地原生/MSYS Vim 测试与人工加密 PPK 验收已通过。源码证明仍 stale，现有预览包不包含后续全部变更；完整迁移目标仍未完成。

真实 Vim 本地测试桥已通过：同一持久化驱动 tui-host-tests.py 对官网原生 Windows Vim 9.2 与本机 MSYS Vim 9.2 均验证文件显示、实际 96×30 网格、编辑持久化和进程退出四项，见 tui-host-native-results.json、tui-host-local-results.json。测试桥修复了重定向父标准句柄向 PTY 子进程传播；终端类型改为实际内置 xterm，驱动应答长文件名提示，并按独立按键时序发送 Escape。旧 MSYS/原生失败证据保留在 tui-host-msys-baseline.json、tui-host-native-baseline.json。来源和下载哈希见 tests/vim-fixture-tools.txt。此结果仍仅为测试桥的本地 ConPTY 程序检查，尚未证明真实 Vim 经 SSH 和 HwndTerminal/ConEmu 的完整交互。

加密 PPK 人工验收完成：操作者在原生 Pageant 中输入一次性测试口令后，Hub 与 WinSCP 均完成代理公钥认证，五项检查通过（app-agent-encrypted-integration.json），未使用真实密钥。早期本地 Vim 未显示文本的失败已被上段测试桥修复和通过结果取代，旧失败证据保留在 baseline 报告；SSH/HwndTerminal 全链路仍待运行。

静默数据丢失补充：modern、ConEmu、自动回退经本机转发层丢弃 SSH 数据（TCP 不关闭）各九项检查通过，见 app-blackhole.json、app-conemu-blackhole.json、app-fallback-blackhole.json。确认旧后端存活、服务器未收到探针，显式取消低于两秒并释放进程，恢复转发后新认证与输入成功。此证据不覆盖系统级 TCP 丢包/重传超时或自动死连接检测，不能据此宣称所有静默断网超时行为已验收。新增测试使源码证明与预览包需要再次更新。

独立验收报告归档修复：认证取消和关闭窗口测试使用各自前缀，避免删除普通 SSH/原生 PuTTY 成功报告。verify-source.ps1 在发布证明前要求 18 份报告存在且 passed；缺失即拒绝。当前源码变更后必须重新执行验证，不能复用上一轮证明。

审批恢复后的验收更新：用户确认额度恢复后重新提交运行检查，ConEmu 与缺少现代终端 DLL 的自动回退各七项 PendingAuth 检查通过（app-conemu-pending-auth.json、app-fallback-pending-auth.json）。下述额度阻塞文字作为历史记录保留，当前已解除这项运行限制；完整交互与最终源码/包审计仍未完成。

当前阻塞审计（2026-10-23）：审批额度问题已连续多轮存在，未再次提交受阻运行请求，也没有改用其他权限路径绕过审批。恢复后先运行现代/ConEmu/自动回退的认证取消与独立工具退出边界回归，再执行当前 verify-source.ps1；完整 IME/剪贴板/鼠标/跨屏、全屏程序、加密 PPK 解锁及旧系统矩阵仍需实际环境或人工验收，不能以编译/静态检查替代。完整目标保持未完成，现进入 blocked 等待外部条件变化；现有源码与预览包均保留。

迁移契约已整理到 MIGRATION-CONTRACT.md：逐项记录模块、旧版 JSON/密码库兼容、工具配置与生命周期，并对照旧 C# 流程。六个原始迁移阶段的证据及缺口记录在 evidence/migration-requirement-audit.json，结论为 incomplete；没有把未运行的交互或兼容测试缩减为已完成。

源码清单校验补充：验证与打包改用 source-inputs.ps1 枚举同一完整输入集合，除已有文件哈希变化外，也拒绝新增、删除、重复证明和越界路径。六项独立合成文件检查通过（source-input-results.json），没有运行受额度限制的 GUI/组件测试。原独立源码证明因当前修改继续失效，必须在运行验收恢复后重新生成，不能据此发布新包。

兼容前端认证取消扩展：ConEmu 与自动回退的 PendingAuth 分支已实现并完成 C++ 编译，尚未执行运行验收。自动审批因额度限制拒绝了两条测试的启动，提示 2026-10-27 14:33 后重试；不是测试失败，也没有生成通过结果。恢复后执行 run-app-tests.ps1 -Frontend conemu -PendingAuth 及 -Frontend fallback -PendingAuth，均指定项目测试 Python。不得用现代前端的结果替代这两条证据。

认证中断补充：现代终端通过真实 Plink 与暂缓认证响应的本地 SSH 假服务器验证，认证尚未完成时关闭标签低于两秒，后端退出、临时配置清理、旧连接不能复用，随后新连接认证及输入成功，共七项检查（app-pending-auth.json）。该项尚未覆盖 ConEmu 认证等待或静默网络丢包。

原生工具立即关闭边界已修复：run-native-close-tests.ps1 通过实际 Hub 原生 PuTTY 入口创建暂停子进程，关闭主窗口后恢复子进程，SSH 认证成功；随后只终止自己的假 PuTTY，核对 Hub 收尾进程退出及临时配置被清理（native-close-results.json）。修复前失败保留在 native-close-baseline.json。实现是在主窗口关闭、终端标签释放和密码库锁定后保留后台资源收尾：密码交付完成/失败后释放通道，原生配置保留到对应 PuTTY 进程退出；WinSCP 无需配置保管，交付结束后不保留 Hub。此行为不承诺 Hub 进程随主窗口立即退出，也不保证强制结束 Hub 时还能完成未交付认证。普通原生入口五项回归通过。同组 WinSCP 边界现已实测：来自活动 SSH 快照的暂停 WinSCP 在主窗口关闭后完成 SFTP 认证与目录列表，Hub 进程随后退出而 WinSCP 继续运行（winscp-close-results.json）。20261022-154752 预览包尚不含修复，源码哈希证明及发布包需重新生成。

交付验证补充：新增 verify-source.ps1，提供从独立复制源码构建及运行回归的可重复入口，成功后绑定源码哈希。当前完整迁移、独立 PTY/ConEmu、人工交互与最终包审计仍各自需要对应证据。

旧版分组对照：原生版现补齐新建分组、显示已保存空组与删除空组入口（会话菜单、树右键与 Delete），拒绝删除仍有会话的分组。生产操作、保存后重开及实际树控件四项检查通过（group-results.json）；提示框完整交互尚待验收。JSON 导出/导入现保留无会话的空组，重复导入不重复建组；属性保存和原生导入同步保留分组名。新增三项往返检查及旧 C# 密码库双向互操作通过，最新 core-results.json 共 58 项。

旧版功能对照补充：从 `integrator/src/MainForm.cs` 核对发现原生版缺失锁屏与五分钟空闲自动锁定，现已补齐；同时取消未完成密码交付，清除活动与保留上游凭据缓存。主窗口消息分发、空闲阈值两侧和手动锁定四项检查通过，锁屏通知注册成功，见 `evidence/credential-lock-results.json`；未实际锁住 Windows。首轮真实连接回归暴露启动前桌面空闲会立即取消新交付，已将空闲时长限制为不超过本次应用运行时间，保留旧版启动计时语义；后续回归结果以最新 app-integration.json 为准。旧版保存密码列表管理/更新/删除入口现已接入工具菜单，显示引用数、拒绝删除仍被引用凭据，并在自动锁定后清空列表。原生列表元数据和三档 DPI 按钮几何检查通过（credential-manager-results.json），生产操作的更新重开、引用删除无写入、删除持久化、锁定拒绝及过期更新五项检查通过（credential-actions-results.json）；完整更新/删除人工交互仍待验收。

2026-10-22 自动验收续跑：终端 DPI/API 8 项与属性窗口 96/144/192 DPI 几何检查通过；主应用 Plink 共享会话及独立 WinSCP 11 项回归通过。密码管道回归发现取消期间 `FlushFileBuffers` 返回错误 995 后仍发布交付成功；现检查返回值，并在同一锁内协调取消与成功状态发布。最终主应用重建后的现代终端与 ConEmu 各 11 项 Plink/WinSCP 回归、独立原生 PuTTY 5 项回归再次通过。`evidence/pipe-baseline-1.json`、`pipe-baseline-2.json` 保存修复前失败证据，`evidence/pipe-results.json` 保存修复后三项通过结果。新增真实无响应读端检查先在 65 秒失败（`pipe-timeout-baseline.json`），随后增加整个交付工作线程的 60 秒截止与重复取消同步 I/O，读端仍保持连接时于 60 秒结束（`pipe-timeout-results.json`）。未据此宣称所有取消时序已覆盖，实际跨屏/IME 与最终源码验证、重新打包仍待完成。

最新 DPI 补充：系统字体按窗口 DPI 缓存；主窗口字体会随 DPI 更新，属性窗口支持重排、受限高度滚动和焦点可见性。96/144/192 DPI 的属性控件几何检查、原生终端 96→192 DPI 网格检查，以及修改后的主应用 SSH/WinSCP 回归已通过。范围见 `evidence/ui-layout-results.json` 和 `evidence/terminal-input-results.json`；实际跨屏、视觉及 IME 仍未验收。上一轮 12:08 的预览包不包含本轮 DPI 修改，独立源码验证与发布包需再次更新。

本轮补充：外围改为系统字体与紧凑布局，接入会话右键菜单、搜索/新建/标签/WinSCP 快捷键及外围焦点切换；仍需视觉与完整键盘人工验收。原生终端 Tab、中文、emoji 的粘贴现已通过真实 SSH 字节检查，相关输入修复及边界见 `FRONTENDS.md`，不等同于系统 IME 已验收。系统输入转换和尺寸适配更新后，两种前端主应用共享标签及 WinSCP 回归均通过；最新键盘改动的回归结果以对应 evidence 文件为准。

| 计划项目 | 当前证据 | 剩余要求 |
| --- | --- | --- |
| 契约、原版保留 | C++ 工程独立；原 C# 与暂停工程保留；会话格式兼容；生成假密码库的双向互操作记录 | 整理最终模块与迁移契约，核对旧版完整流程 |
| 终端路线 | HwndTerminal、ConPTY、Plink 原型和主应用；ConEmu 原型、主应用选择及自动回退 | 中文 IME、鼠标、复制粘贴、全屏程序、DPI、断线与退出完整验收；旧系统矩阵 |
| 基础服务 | 58 条基础与互操作检查记录；受限密码管道；配置归属与清理；固定组件下载、哈希与取消测试 | 已验证迁移拒绝覆盖、错误密码及提交中断恢复；原生配置回写及 Plink 实际加载已验证；恢复入口、模拟中断和后续修改冲突检查已接入并验证；真实写入与恢复进程中断均已验证（writeback-crash-results.json）；仍需迁移及回写对话框完整流程 |
| 外壳与 WinSCP | 会话树、搜索、属性、标签、原生工具入口；两条终端路线的活动快照到独立双栏 WinSCP 流程；原生 PuTTY GUI 的实际启动、密码交付及 SSH 认证 | 书签增删和按活动快照打开 WinSCP、密码解除绑定入口已接入；仍待完整用户交互验收 |
| 整合验收 | 同身份双标签一次认证；不同凭据独立认证；首标签关闭保留上游；末标签释放上游；WinSCP 保持独立 | Pageant 代理签名与跨工具认证已通过；加密 PPK 首次 passphrase 人工解锁及跨工具认证已通过；上游真实进程崩溃、下游退出、新连接认证及清理已通过三条路线；服务端关闭 TCP 后的退出与重新认证也已通过三条路线；仍待静默丢包/超时、认证中断及隐藏上游 shell 交互；真实全屏程序 |
| 交付 | 可运行开发二进制、源码/便携预览包、组件许可、清单及解压启动测试 | 独立解压源码构建、迁移与 SSH/PTY 已验证；仍待最终发布包和逐项完成审计 |

主要证据：`evidence/core-results.json`、`pty-results.json`、`app-integration.json`、`app-conemu-integration.json`、`app-fallback-integration.json`、`component-results.json`、`app-agent-integration.json`、`app-native-putty-integration.json`、`writeback-crash-results.json`、三条 `app-*-sharing-fault.json`（现代路线为 app-sharing-fault.json）。这些证据各自只证明其检查覆盖的行为；不能合并解释为迁移已经完成。

ConEmu 的真实回退测试使用独立组件目录，只复制 PuTTY 与 ConEmu，确实不包含现代终端 DLL。测试继续走主应用的自动选择逻辑，并验证后续 SSH 与 SFTP 行为。最新构建还检查了导入表，确认 ConPTY 不是启动期硬依赖；这不等同于旧 Windows 实机验证。

全屏程序测试环境检查：WSL 能列出 Ubuntu，但启动只读 Python 工具清单查询时返回 HCS_E_SERVICE_NOT_AVAILABLE；尚未验证其中的 Vim/top。未安装软件、修改发行版配置或调整 Windows 服务。记录见 evidence/linux-environment.json；不能将已安装的发行版名称视为可用 Linux 验收环境。
