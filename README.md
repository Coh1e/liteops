# LiteOps

> 2026-10-24：已拆分为独立项目。当前状态与迁移后的构建前提以 [HANDOFF.md](HANDOFF.md) 为准；协作规则见 [AGENTS.md](AGENTS.md)。下文保留历史实现说明，旧证据、旧路径与验证数字不代表本轮运行结果。依赖、组件与本机预设未随源码迁入。

当前是开发中的 C++23 / Win32 版本，原 C# 版本保留在本机归档 `D:/Projects/rea/archive/legacy-workspace/integrator/`。尚未完成迁移验收，不替换旧版默认入口。

## 编译与运行

```powershell
./build.ps1
./bin/PuTTYHub.exe
```

构建脚本使用本机已有的 Visual Studio Build Tools 与 Windows SDK，不修改全局环境。当前实测平台为 Windows x64；终端依赖 ConPTY，不承诺旧版 Windows 支持。菜单“终端前端”可选择自动、现代终端或 ConEmu。自动模式根据系统接口及固定组件能力选择。主程序通过菜单或顶部按钮连接终端、打开独立 WinSCP、启动原生 PuTTY。会话属性中的认证方式为 `agent`、`password` 或 `ask`。

程序数据保存于本目录 `data/`，临时文件在 `runtime/`。通过“工具 → 下载 / 修复官方组件”下载固定版本并校验 SHA-256。新建 SSH 会话使用独立命名的 PuTTY 运行配置；可引用原生 PuTTY 配置以保留高级设置。启动后保存连接快照，从活动标签打开 WinSCP 不重新读取修改后的会话条目。

普通密码可用主密码库保存，也可仅在连接时输入。本次密码通过当前用户 DPAPI 加密缓存在活动标签中，用于随后启动 WinSCP；锁定密码库会清除这些缓存。工具进程只收到受限命名管道名称。服务端 MFA、主机密钥确认等仍保留原生交互。

## 紧凑界面与键盘操作

会话菜单与树右键菜单可新建分组；保存的空分组也显示在未筛选的树中。选中空分组后可按 Delete 或使用删除菜单，确认后删除；非空分组必须先移动或删除其中会话。搜索时只显示匹配会话及所在分组。JSON 导入也保留没有会话的空分组，重复导入按名称合并分组；普通会话导入继续生成新 ID 并移除凭据引用。

外围界面采用系统字体和紧凑控件，会话树支持鼠标右键及 Shift+F10 菜单。搜索框按 Esc 清空、向下键进入会话树、Enter 连接首个匹配会话；会话树中 Enter 连接、F2 属性、Delete 确认删除。Tab / Shift+Tab 在外围控件间切换焦点。

| 快捷键 | 操作 |
| --- | --- |
| Ctrl+Shift+F | 搜索会话并选中现有查询 |
| Ctrl+Shift+N | 新建会话 |
| Ctrl+Tab / Ctrl+Shift+Tab | 下一个 / 上一个终端标签 |
| Ctrl+Shift+W | 关闭当前标签 |
| Ctrl+Shift+S | 从当前标签打开 WinSCP |
| Ctrl+Shift+C / Ctrl+Shift+V | 现代终端选区复制 / 粘贴 |

这些快捷键由 Hub 自己的窗口处理。焦点在跨进程 ConEmu 内时遵循 ConEmu 的按键规则，可使用 Hub 菜单与按钮执行管理操作。普通 Ctrl+C、Ctrl+V、Ctrl+W、Enter、Tab 和功能键在终端内继续交给终端；外围会话树的 Enter、F2、Delete 不全局占用。当前代码与编译验证不代替完整键盘和视觉人工验收。

Windows 锁屏通知、手动锁定与五分钟系统输入空闲使用同一凭据清理路径：锁定密码库，清除活动标签与保留共享上游的 DPAPI 凭据缓存，取消尚未结束的密码交付；已经建立的 SSH/WinSCP 连接继续由原生进程维持。空闲时间使用当前 Windows 会话的最后输入，并以本次 Hub 启动时间为上限（启动前的空闲不计入），不把 ConEmu 内的正常操作误判为 Hub 空闲。锁屏通知注册失败时在状态栏提示，空闲与手动锁定仍有效。

`bin/PuTTYHub.exe --credential-lock-test` 使用独立测试目录与合成密码库，验证真实主窗口的锁屏消息分发、299999/300000 毫秒边界及手动锁定，结果见 `evidence/credential-lock-results.json`。该检查不实际锁住 Windows，不等同于物理锁屏验收。

工具菜单“管理保存的密码”提供原生列表，显示凭据名称与持久化会话引用数，不显示密码；可更新同一凭据（影响全部引用会话），删除时拒绝仍被会话引用的条目，并要求确认。解除会话密码绑定不会删除凭据。管理窗口打开期间自动锁定仍生效，锁定后清空列表；关闭后重新打开会要求解锁。已运行连接继续使用启动时缓存的凭据，更新密码只影响后续读取密码库的连接。

`--credential-lock-test` 同时检查管理列表元数据及 96/144/192 DPI 下按钮可达性，结果见 `evidence/credential-manager-results.json`。同一入口还通过界面共用的生产操作验证更新后重新解锁、引用删除保护（文件哈希保持不变）、未引用删除持久化、锁定与过期更新拒绝，见 `evidence/credential-actions-results.json`。更新/删除的确认框与密码输入完整人工交互尚未验收。

`--credential-lock-test` 还验证生产分组操作与实际树控件：空分组可见、新建持久化、非空删除拒绝、空组删除持久化，见 `evidence/group-results.json`；不替代新建/删除提示框的完整交互验收。

## 当前验证

外围控件按窗口 DPI 获取系统消息字体；属性窗口会按 DPI 重排，超出可用高度时支持滚动，并在键盘切换焦点时显示目标控件。`bin/PuTTYHub.exe --layout-test` 检查 96、144、192 DPI 下的原生控件边界、字体、文字保留及受限高度下的按钮可达性，结果在 `evidence/ui-layout-results.json`。`build.ps1 -TerminalInputTest` 与 `bin/terminal-input-tests.exe` 还验证原生终端从 96 到 192 DPI 时的字体网格变化。它们是指定 DPI 参数的 API 与几何检查，不代表实际跨显示器移动、视觉、输入法或完整剪贴板交互已经验收。实现依据为 Windows 的[每显示器 DPI 指引](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)。

```powershell
./build.ps1 -CoreTest
./bin/core-tests.exe
./tests/run-migration-tests.ps1
./tests/run-app-tests.ps1
./tests/run-app-tests.ps1 -Frontend conemu
./tests/run-app-tests.ps1 -Frontend fallback
./tests/run-app-tests.ps1 -Frontend putty
./tests/run-app-tests.ps1 -SharingFault
./tests/run-app-tests.ps1 -Frontend conemu -SharingFault
./tests/run-app-tests.ps1 -Frontend fallback -SharingFault
./tests/run-app-tests.ps1 -ServerLoss
./tests/run-app-tests.ps1 -Frontend conemu -ServerLoss
./tests/run-app-tests.ps1 -Frontend fallback -ServerLoss
./build.ps1 -ComponentTest
./bin/component-tests.exe
./build.ps1 -AgentTest
./tests/run-agent-tests.ps1
./build.ps1 -WritebackCrashTest
./bin/writeback-crash-tests.exe
```

测试现在使用本工程 `tests/deps`：先执行 `tests/setup-test-deps.ps1 -Python <Python完整路径>` 安装固定版本依赖。脚本将安装、缓存及临时文件限定在本工程，不全局安装。各测试脚本的 `-Python` 可指定解释器；默认使用 PATH 中的 python.exe。旧版 C# 参考源已封存在 tests/legacy-source，无需相邻旧工程。源码包不重复包含运行组件，测试前通过主程序下载或从配套便携包复制 components 目录。测试只使用本机假服务器、假密码和独立配置；`--integration-test` 是这一测试流程的入口，不能用于真实服务器。成功记录位于 `evidence/`，失败也应核对进程退出码，不能只看此前留下的结果文件。

目前证据覆盖：会话持久化与导入、密码库与错误密码/篡改处理、固定组件校验、真实 SSH 认证和尺寸更新、原生主窗口启动、活动标签到独立双栏 WinSCP 的认证与 SFTP 列表、会话修改后快照不变、关闭 SSH 标签时 WinSCP 保持运行，两个终端标签共用一次 SSH 认证、首标签关闭后上游保留、最后标签关闭后释放上游，以及从空目录下载全部组件、许可文件、篡改拒绝和取消清理。

原生 PuTTY 独立入口现已通过实际主应用的 GUI 启动、进程绑定密码交付和回环 SSH 认证测试，证据为 `evidence/app-native-putty-integration.json`。该入口继续使用原版 EXE 与独立配置；不经现代终端前端，也不注入终端适配器。测试明确固定假服务端主机密钥，结束时只清理其自己启动的测试子进程；生产流程仍保留原生确认。此证据不覆盖完整原生 GUI 交互、启动后立即退出 Hub 的竞态或原生 PuTTY 退出行为。

## 旧版会话与密码库迁移

会话菜单的“写出 / 回写 PuTTY 配置”可把所选会话写入明确指定的原生会话名。确认窗口列出主机、端口、用户和私钥路径；只更新这四项、SSH 协议及会话存在标记，已有目标的代理、外观等其他原生字段保留，密码与凭据引用不写出。空用户名或私钥会清空对应原生字段；不会自动修改 Hub 的 PuTTY 配置引用。默认设置、Hub 临时配置及已有非 SSH 目标禁止更新。

回写前，原字段的类型与原始字节保存到受限的 `runtime/putty-writeback/*.json`，写入报错时尝试恢复原值。此实现不依赖本机不可用的注册表事务支持，不承诺跨字段原子提交。新版备份同时记录写前与预期写入值；启动时提示未完成记录，会话菜单提供恢复入口，文件选择默认打开备份目录。恢复先检查所有连接字段：若存在既不等于原值、也不等于本次写入值的字段，则在任何恢复写入前拒绝，保留后续原生编辑。同一目标的 Hub 写入与恢复使用互斥锁；原生 PuTTY 的并发写入不受此锁约束，操作期间应避免同时保存同一配置。恢复只处理六个连接字段，不删除整个配置键，因此保留后来增加的其他设置。旧版 v1 备份缺少预期值，拒绝自动恢复。备份状态包括 prepared、committed、rolled_back、restore_required、restoring 和 restored，此目录不进入发布包。

真实进程中断验收见 `evidence/writeback-crash-results.json`：独立驱动调用现有生产代码，观察到持久化未完成记录和实际字段变化后，只终止自己创建的假配置写入进程；另一次在恢复进程已还原一个字段后终止它。随后通过生产恢复接口完成两次恢复并核对原始连接字段、无关设置及最终记录状态。测试通过目录与注册表通知观察，不向产品加入故障开关；CPU 亲和性、线程优先级及子进程作业只用于驱动，结束时恢复或清理。范围是进程崩溃恢复，不包含断电持久性或恢复对话框的完整人工验收。失败的测试专属未完成记录可能标为 test_aborted，不参与产品恢复。

PuTTY 原生会话名使用 Windows 当前代码页并执行原生百分号转义（包括前导点）。当前代码页无法表示的名称或连接字段明确拒绝，避免原生工具读到替代字符；Hub 自己的会话库继续使用 Unicode。回写与原生 `-load` 的实际互操作由独立测试验证。

在尚未建立原生会话库及密码库的新配置中，选择“会话 → 完整迁移旧版会话与密码库”，再选择旧版数据目录的 `sessions.json`。如果同目录存在 `vault.bin`，输入旧密码库主密码。程序先验证会话格式、Windows 当前用户、主密码及凭据引用，然后复制两个原始数据库；会话 ID 与凭据 ID 保持不变，旧版文件不被修改。迁移后的密码库保持锁定。

此流程适用于同一 Windows 用户下的首轮迁移。已存在的原生会话库或密码库不会被覆盖；已有库可使用普通 JSON 导入，但普通导入会去掉密码引用，也不会合并密码库。WinSCP 偏好和组件目录不随此操作覆盖。

迁移采用受限暂存目录、文件哈希和提交日志。如果应用在两个数据库提交之间退出，下次启动会验证并继续提交；发现目标文件冲突时保留事务并报错，不覆盖冲突文件。不要手工删除仍待恢复的 `data/legacy-migration.json` 及它引用的暂存目录。

验证脚本 `tests/run-migration-tests.ps1` 用原版 C# 实现生成假数据库，并检查迁移、来源不变及反向互操作。最新 `evidence/core-results.json` 包含这些检查；这不代替迁移对话框的完整人工交互验收。
## 尚待完成

当前应用已接入受控共享：仅复用本 Hub 拥有、共享协议已就绪且会话与原生配置指纹相同的 Plink 上游；无法确认时创建独立连接。`-load` 仍只表示加载配置。第一标签的后台进程在存在下游时保留，包含其初始 shell；关闭最后标签时一并释放。当前机制与后续验证边界见 `SHARING.md`。ConEmu 兼容候选已通过独立原型的嵌入、真实 SSH、输入和远端缩放测试（见 `FRONTENDS.md`），已接入主应用的手动选择和自动回退，主应用共享标签及 WinSCP 联动验证通过。Pageant 的代理签名路径已通过实际 Hub 与 WinSCP 的公钥认证测试（见 `PAGEANT.md`），加密 PPK 的首次 passphrase 解锁仍待人工验收；中文 IME/鼠标/全屏应用及 DPI 的完整交互验收、旧版会话与密码库的显式迁移已接入并验证；已有库合并迁移尚不支持。配置回写及恢复已完成核心互操作、模拟中断和真实写入/恢复进程中断检查；完整交互与最终发布包仍待完成。会话菜单可添加、移除目录书签；连接菜单可按活动标签的书签打开 WinSCP。书签修改对新标签生效，单次打开只覆盖启动目录，不改变活动连接快照。会话菜单还提供解除保存密码绑定，保留可能被其他会话共享的凭据和现有活动标签。

终端使用固定版本的 Windows Terminal 原生控件。Plink 窗口尺寸适配器仅注入本产品拥有且哈希匹配的 0.85 x64 子进程；不改动原始 EXE。它不是跨版本稳定接口，升级组件必须重新验证。组件许可证放在 `licenses/` 及各组件目录，JSON 库许可证在 `vendor/JSON-LICENSE.MIT`。

## 开发预览包

`package.ps1` 生成标记为 development-preview 的便携包与源码包，含组件许可和文件哈希清单，排除 data/runtime。脚本解压便携包，校验清单并运行启动冒烟测试。预览通过不代表 Pageant、终端完整交互和其他迁移验收已经完成。

Pageant 加载入口位于工具菜单，可把所选会话的 PPK 交给原生 Pageant。Hub 不接收或保存私钥 passphrase。未加密一次性 PPK 的自动代理测试通过，不等同于加密 PPK 人工解锁验收。

独立源码构建验证见 `evidence/source-verification.json`：在独立目录编译主程序与核心测试，使用封存 C# 源完成迁移互操作，并执行实际 SSH/WinSCP 和 PTY 缩放检查。运行组件取自校验过的配套便携包，Python 测试库使用工程内固定版本依赖。

独立源码验证可执行 `./verify-source.ps1 -Python <完整解释器路径>`。脚本复制源码与项目内依赖到新建验证目录，从零构建主应用、核心、管道和终端输入驱动，执行脚本列出的回归，最后复核原目录与复制目录的源码哈希；仅全部成功才更新 source-verification.json。失败保留现场和活进程信息，不自动终止未知进程。此脚本不等同于完整迁移验收，也不替代 PTY/ConEmu、人工交互及旧系统矩阵。

关闭主窗口会结束 Hub 终端并锁定密码库。为避免独立工具尚未读取配置/密码就被清理，Hub 可保留无界面的资源收尾进程：密码交付按既有时限完成或失败后释放管道；原生 PuTTY 的临时配置保留到其进程退出，再清理并退出 Hub。WinSCP 的交付结束后不需要 Hub 继续常驻。强制结束 Hub 仍可能中断未完成交付。`tests/run-native-close-tests.ps1 -Python <完整路径>` 验证主窗口关闭在先、暂停工具恢复后认证、工具退出后配置和收尾进程清理；结果见 `evidence/native-close-results.json`。

同一驱动加 `-Tool winscp` 会先建立真实 SSH 标签，再通过活动快照启动暂停的 WinSCP，关闭 Hub 主窗口后恢复 WinSCP，核对 SFTP 列表及 Hub 进程已退出而 WinSCP 仍在运行，结果见 `evidence/winscp-close-results.json`。测试结束仅清理本次启动的工具进程。

`tests/run-app-tests.ps1 -Python <完整路径> -PendingAuth` 用假服务器暂缓首个认证响应，在真实现代终端标签中取消并重连，验证关闭耗时、后端/临时配置释放、共享就绪拒绝和新认证输入。结果见 `evidence/app-pending-auth.json`。此项只覆盖现代前端的认证等待，不代表静默网络丢包或 ConEmu 对应场景已验收。

认证取消驱动现支持 `-Frontend conemu -PendingAuth` 与 `-Frontend fallback -PendingAuth`，兼容分支已编译。截止 2026-10-23，两条运行检查因自动审批额度限制尚未执行，不能视为已通过。对应结果分别为 app-conemu-pending-auth.json 和 app-fallback-pending-auth.json。

源码清单由 `source-inputs.ps1` 统一枚举，独立验证与打包均核对整个输入集合及哈希；新增、删除或修改文件都使旧证明失效，重复/越界证明条目也被拒绝。`tests/run-source-input-tests.ps1` 用独立合成文件验证六项接受/拒绝行为，结果见 `evidence/source-input-results.json`；不启动 GUI 或连接组件。

后续恢复验收：ConEmu 与自动回退的 PendingAuth 各七项现已实测通过，先前 not_run 记录已更新；不代表静默断网或完整人工交互已完成。独立源码驱动现在还构建 PtyTest 并执行 run-pty-tests.ps1，检查真实原生配置加载、制表符/CJK/emoji UTF-8 及远端 PTY 尺寸更新。

`run-app-tests.ps1 -Blackhole` 支持 modern/conemu/fallback：本机转发层接收并丢弃 SSH 数据，不关闭 TCP，确认旧后端仍存活、输入不到服务器、显式取消低于两秒，恢复转发后重新认证和输入成功。三条路线各九项检查通过。范围是应用可见的静默数据丢失与显式取消，不是操作系统级 TCP 丢包/重传计时或自动死连接检测；未修改系统网络。

2026-10-23 已由操作者完成一次性加密 PPK 的人工 Pageant 解锁，随后 Hub/WinSCP 代理公钥认证通过，见 app-agent-encrypted-integration.json。build.ps1 -TuiHost 仅构建测试桥；当前本机 MSYS Vim 启动画面检查失败，不能把已编译桥或成功版本查询当作全屏程序验收。该桥尚未接入产品或 SSH 假服务器。

Vim 测试桥后续修复已验证：`build.ps1 -TuiHost` 后执行 `python -B tests/tui-host-tests.py --vim <vim.exe完整路径>`，原生 Windows 与 MSYS Vim 均通过文件显示、96×30 尺寸、保存与退出四项。只使用 tests/ssh-fixture/vim-fixture-* 一次性文件；不使用 UI 自动化、不修改组件源码。测试程序应答正常文件信息提示并分开发送 Escape，原始 VT 输出只含合成数据并保留在对应 .vt-output。此本地结果不能代替真实 SSH/HwndTerminal 全屏验收。官方 ZIP 来源与哈希在 tests/vim-fixture-tools.txt，测试工具不随产品包分发。
