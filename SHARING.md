# Plink 共享连接与归属

默认现代终端使用 Plink 0.85 的原生连接共享，WinSCP 仍独立连接。`-load` 加载独立运行配置，`-share` 才启用共享能力。

## 匹配与隔离

Hub 的共享键包含完整会话模型及已复制到运行配置的原生 PuTTY 配置指纹。运行配置的归属标记和共享角色不进入配置指纹，代理等其他值进入指纹。不同配置不共用池中的上游。

首次启动设置 `ConnectionSharingUpstream=1`、`ConnectionSharingDownstream=0`，不接入用户已有的 PuTTY 上游。如果共享监听名称已被占用，Plink 可以独立连接，Hub 不将其当作已拥有的可复用监听。

复用前核对持有的进程句柄仍有效、共享命名管道的服务端 PID 属于该进程，并在短期限内检查 PuTTY 共享协议版本头。该协议头由原生上游在共享连接激活后发送；仅监听存在不算就绪。无法确认时采用新连接。下游运行配置禁用上游角色，允许下游角色。

版本头和配置名称来自固定版本官方源码：`evidence/putty-source/ssh/sharing.c`、`ssh/connection2.c`、`windows/sharing.c` 与 `conf.h`。这里不是跨版本的公共 SDK 保证。

## 生命周期

活动标签持有自己的后端。共享下游还持有上游拥有者的强引用，共享池本身只持有弱引用。关闭第一标签时隐藏其终端宿主；只要仍有下游，上游进程、输出消费和运行配置继续存在。关闭末个下游会先关闭下游进程，再释放上游。

当前上游就是第一标签的 Plink，因此其初始 shell 也会保留至最后一个使用者释放。它不是专门以 `-N` 启动的无 shell 连接管理进程。此行为需要在最终交互验收中确认，不能把关闭第一标签描述为立即终止其远端任务。退出 Hub 会释放它拥有的全部终端；独立 WinSCP 不加入此终端进程生命周期。

密码库锁定会清除活动标签及其保留上游的加密凭据缓存，不结束已认证的 SSH 连接。程序不终止用户已有的 Pageant 或其他 PuTTY 进程。

## 已验证与待验证

`tests/run-app-tests.ps1` 通过实际主应用打开两个标签，服务器记录一次 SSH 密码认证和两个 shell；改用不同凭据引用的新标签独立产生第二次认证；关闭首标签后，下游输入仍到达服务器；末标签关闭后，观察句柄确认上游进程退出。随后 WinSCP 独立认证并浏览 SFTP。证据在 `evidence/app-integration.json`。

基础测试验证等价配置指纹一致、共享角色不改变身份、代理设置变更会改变身份、异常恢复不删除活进程的运行配置。证据在 `evidence/core-results.json`。

共享上游真实进程故障现已通过三条路线验证：`tests/run-app-tests.ps1 -SharingFault`，以及附加 `-Frontend conemu` 或 `-Frontend fallback`。测试先让两个标签共用一次认证，然后仅终止对应本机假服务器的自有上游 Plink；通过后端进程句柄确认下游退出，核对原生标签文本标记为“已退出”，再打开新标签并观察第二次认证和新 shell 输入，最后关闭全部标签并确认拥有者与替代后端释放。证据为 `app-sharing-fault.json`、`app-conemu-sharing-fault.json`、`app-fallback-sharing-fault.json`。

`tests/run-app-tests.ps1 -ServerLoss` 及附加 ConEmu/fallback 选择还验证了服务端主动断线：本机假服务在收到专属控制文件后关闭已认证 SSH 连接的 TCP socket，监听器继续接受新连接。三条路线均确认上下游后端退出、标签显示已退出、旧监听不可复用、新标签重新认证并收到输入，最后释放后端。证据为 `app-server-loss.json`、`app-conemu-server-loss.json`、`app-fallback-server-loss.json`。测试不改动系统网络或真实服务器。

上述范围分别是上游进程崩溃和服务端 TCP 关闭；静默丢包/超时、未完成认证、上游正常退出、代理/隧道、高负载及完整交互仍待验收。隐藏上游初始 shell 的用户体验仍需确认。
