# Pageant 集成与验证

会话属性可保存 PPK 完整路径。选择会话后，使用“工具 → 将所选会话的 PPK 加载到 Pageant”。Hub 验证路径及 PPK 文件头，把路径交给固定原生 Pageant，passphrase 由 Pageant 的原生窗口处理。Hub 不接收或保存该 passphrase，也不因关闭终端而退出用户代理或卸载共享密钥。

认证方式选择 `agent` 后，Plink 和 WinSCP 都启用 Pageant；如配置 PPK 路径，原生工具仍可用它指定身份。首次加载、MFA 和必要原生交互不承诺零提示。已有代理与密钥的生命周期仍由用户管理。

## 自动验证

```powershell
./build.ps1
./build.ps1 -AgentTest
./tests/run-agent-tests.ps1
```

驱动检测到已有 Pageant 窗口或代理管道时会拒绝执行，不添加或移除用户代理中的密钥。无现有代理时，驱动启动并验证自己拥有的原生 Pageant，再加载一次性未加密 PPK。主应用测试会让实际终端与独立 WinSCP 连接只允许公钥认证的本机假服务器。两个客户端均不传私钥文件名，也不创建密码管道，因此不能以本地私钥或密码替代代理签名。测试退出会清理自己创建的进程。

证据：`evidence/app-agent-integration.json`。这只证明代理签名与跨工具通路，不证明加密 PPK 的 passphrase 交互。

## 人工加密 PPK 验证

```powershell
./tests/run-agent-tests.ps1 -ManualEncrypted
```

仅在可运行专用测试代理的环境使用。脚本同样拒绝已有代理，创建一次性加密 PPK，并显示原生 Pageant passphrase 对话框。由操作者在该测试对话框中输入固定假 passphrase：`fixture passphrase 2026`。不得使用真实私钥或真实 passphrase。对话框最多等待两分钟；成功加载后，驱动继续上述实际 Hub 与 WinSCP 的代理认证检查。

只有实际完成这一人工流程并通过认证，才会生成 `evidence/app-agent-encrypted-integration.json`。2026-10-23 由操作者实际完成一次性加密 PPK 的原生口令解锁，随后真实 Hub/WinSCP 公钥认证五项检查通过，证据为 app-agent-encrypted-integration.json；此结果来自人工解锁模式，未用未加密模式替代。测试用 PPK v2 生成器仅用于假数据，不进入产品的私钥处理路径。

人工步骤的原因：当前 computer-use 技能规则明确禁止自动操作认证对话框。产品继续依赖原生 Pageant 处理解锁，并未用脚本填充或绕过这个窗口。

固定版本依据：`evidence/putty-source/windows/pageant.c` 中命令行密钥加载与已有实例转发；`windows/agent-client.c` 中代理传输；`sshpubk.c` 中 PPK 测试格式。升级版本需重新验证。
