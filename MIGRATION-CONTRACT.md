# C++ 迁移契约与旧版对照

2026-10-23。对应上级 CPP-MIGRATION-PLAN.md 第 1 步。本文记录当前数据与资源约定，不宣称运行验收已完成；未完成项仍以 MIGRATION-STATUS.md 为准。

## 模块边界

| 边界 | 当前代码 | 对调用者的约定 |
| --- | --- | --- |
| 会话与启动上下文 | include/hub.hpp、src/core.cpp | Session 是可编辑持久化配置；ConnectionContext 是启动快照。启动后修改会话不改变活动标签的目标或身份。 |
| 密码库 | src/crypto.cpp | Vault 负责创建、解锁、读取、更新、删除、锁定和加密落盘。未解锁时拒绝读取与写入；界面层负责会话引用删除保护。 |
| 工具配置 | include/tools.hpp、src/tools.cpp | ManagedProfile 使用独立临时命名空间；显式回写和恢复有独立记录与冲突检查。不能把临时运行配置当作用户原生会话覆盖。 |
| 现代终端 | include/terminal.hpp、src/terminal.cpp | TerminalControl 负责呈现与输入转换；PtyProcess 负责 ConPTY、Plink、输出队列、尺寸与关闭。外壳不实现终端解析器。 |
| 兼容前端 | include/conemu.hpp、src/conemu.cpp | ConEmuFrontend 用官方嵌入机制启动并识别本产品拥有的 Plink，绑定实际密码接收 PID；拓扑不要求与 ConPTY 相同。 |
| 凭据交付 | PasswordPipe | 当前用户受限管道、实际工具 PID 绑定、取消与总时限；参数中只出现通道名。交付成功不等同于服务器认证成功。 |
| 编排与界面 | src/app.cpp | Application/Tab/External 组合上述对象，管理菜单、树、标签、活动快照、共享上游及独立工具。 |
| 组件与交付 | Components、build.ps1、verify-source.ps1、package.ps1 | 固定版本与哈希、许可随附；独立源码运行验证成功后绑定源码全集和二进制哈希，再允许打包。 |

## 数据兼容

会话文档保持 Version=1、Groups、Sessions。Session 的 JSON 字段保持旧 C# 大小写：Id、Name、Group、Host、Port、User、Auth、CredentialId、KeyFile、RemoteDirectory、Bookmarks、PuttyProfile、Source。认证值为 agent/password/ask。Groups 允许保存没有会话的空分组。

普通 JSON 导出清除 CredentialId，password 改为 ask；普通导入生成新会话 ID、清除凭据引用，按名称合并分组，不合并密码库。完整首轮迁移则保留会话/凭据 ID，验证旧库与主密码后提交 sessions.json 和 vault.bin，保持源文件不变，拒绝覆盖已有目标；中断由迁移日志恢复。

密码库保持旧版 PHV1 内容格式及当前 Windows 用户 DPAPI 包装，使用 PBKDF2-SHA256、AES-CBC 和 HMAC。双向兼容证据必须来自旧 C# 写入/C++ 读取及 C++ 更新/C# 读取，不能仅靠格式描述认定兼容。Windows 用户绑定意味着不能把复制 vault.bin 视作跨用户迁移。

## 配置与资源归属

| 对象 | 所有者及结束条件 |
| --- | --- |
| Hub 会话库、密码库、WinSCP 偏好 | Hub 数据目录；普通导入和完整迁移各自遵守覆盖规则。 |
| PuTTYHub.Managed.* 配置 | 创建它的 ManagedProfile；记录进程身份并在正常释放时清理，启动恢复只清理可确认已失效的本产品记录。 |
| 用户原生 PuTTY 配置 | 用户；导入只保留引用。只有明确选择写出/回写时才更新六个允许字段，并保存恢复记录。 |
| 终端标签及 Plink | Hub；关闭标签释放自身资源。被下游引用的首标签上游继续保留，最后一个引用释放后结束。 |
| 密码库与活动凭据缓存 | 锁屏、手动锁定、五分钟有效空闲时清理；现有已认证原生连接不因此结束。 |
| 原生 PuTTY 与 WinSCP 窗口 | 独立工具进程。关闭 Hub 主窗口时不终止它们；Hub 可能无界面保管未完成密码交付和原生 PuTTY 临时配置。 |
| 窗口关闭后的收尾 | 交付完成、失败或工具退出后释放管道；PuTTY 配置保留到其进程退出。WinSCP 无配置保管需求，交付结束后 Hub 可退出，WinSCP 继续运行。强制结束 Hub 不保证未完成交付。 |
| 用户 Pageant 与已加载密钥 | 用户共享资源。Hub 提供加载入口，不接管用户已有代理，也不因关闭一个终端卸载共享密钥。 |

## 连接语义

Plink -load 只表示加载原生配置。共享复用还需要本 Hub 拥有的活进程、共享协议就绪及完整配置/身份指纹一致；等待认证或死亡的上游不能认定为可复用。

从活动 SSH 标签打开 WinSCP 使用启动快照及可用认证材料，创建独立 SSH/SFTP 连接；不承诺复用 Plink 的 TCP、SSH 或 MFA 状态。只能可靠映射的代理设置才允许自动启动，无法映射时明确拒绝，不能静默直连。目录使用初始目录或书签；不跟随 shell 内 cd、再次 ssh、sudo 或容器切换。

现代终端使用固定的 HwndTerminal 组件。尺寸/UTF-8 适配仅用于本产品拥有、哈希匹配的 Plink，不代表任意 PuTTY 版本可兼容，也不把安装 wt.exe 当作控件可用证明。原生 PuTTY 入口保持独立原版进程。

## 旧版行为对照

对照来源为保留的 integrator/src/Model.cs、MainForm.cs、Vault.cs、Launch.cs；旧工程与暂停的 reconstruction 均不替换。

| 旧版流程 | C++ 对应或既定架构变化 | 证据边界 |
| --- | --- | --- |
| 会话新增、属性、复制、分组、删除、搜索 | 原生树与属性窗口；保存空组，拒绝删除非空组 | core-results、group-results；完整提示框与键盘交互未全部验收 |
| 导入 PuTTY/WinSCP/JSON、导出无密码会话 | 相应工具适配、SessionStore；另有完整首轮数据库迁移 | 核心及 C# 互操作检查；迁移对话框仍需完整验收 |
| 密码管理、锁屏/空闲锁定 | 原生凭据列表、共享生产更新/删除操作、统一缓存清理 | credential-*；密码输入与确认框仍需完整验收 |
| 外部 PuTTY 被嵌入、可分离标签 | 按新计划改为 Plink+终端前端，关闭 Hub 终端会结束连接；原生 PuTTY 为独立入口 | 不能沿用旧版“所有标签都可分离继续运行”的承诺 |
| 选中会话打开 SFTP | 按新计划从活动 SSH 快照启动独立双栏 WinSCP | app-*integration、winscp-close；不宣称同一 SSH 传输复用 |
| 目录书签、Pageant、PuTTYgen | 对应菜单入口；PSCP/PSFTP 原生入口同时保留 | 书签完整交互及加密 PPK 首次解锁尚需验收 |
| 组件管理 | 固定组件下载/修复与哈希检查 | component-results；升级组件必须重新验证 |

## 完成门槛

本文不能把未验收行为改名为完成。计划第 2/5 步仍需要真实 IME、剪贴板与鼠标、跨显示器 DPI、全屏程序、完整取消/断线及兼容矩阵证据；第 4 步仍需要原生外壳的完整用户流程验收；第 6 步需要当前源码的独立构建、更新后的源码/便携包、许可与清单检查及最终逐项审计。审批额度导致未运行的检查必须保持 not_run，旧结果不能替代新源码验收。
