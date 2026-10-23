# 终端前端验证与兼容范围

当前验证环境：Windows x64，系统报告版本 `10.0.26300.0`。以下结论只覆盖该环境与固定组件版本。

| 路线 | 当前状态 | 证据 |
| --- | --- | --- |
| Windows Terminal 1.24.12741.0 原生控件 + ConPTY + Plink 0.85 | 已接入主应用；SSH、输入、远端尺寸、双标签共享与 WinSCP 联动通过 | `evidence/pty-results.json`、`evidence/app-integration.json` |
| ConEmu 230724 `-insidewnd` + Plink 0.85 | 已接入主应用；手动选择及缺失现代终端 DLL 时自动回退通过 | `evidence/conemu-results.json`、`evidence/app-conemu-integration.json`、`evidence/app-fallback-integration.json` |
| 未安装 Windows Terminal 应用 | 控件路线使用分发的固定 DLL，不通过检测 `wt.exe` 决定；ConEmu 原型不创建该控件 | 主应用缺失 DLL 时的自动回退已验证；损坏及加载失败分支仍待补充 |
| 较老 Windows | 未验证 | ConPTY 已改为运行时探测；其他系统 API 和旧系统行为仍未验证，不宣称旧系统支持 |

## ConEmu 原型

固定官方资产：`https://github.com/ConEmu/ConEmu/releases/download/v23.07.24/ConEmuPack.230724.7z`。

归档 SHA-256：`2a9b98ebecaede62665ef427b05b3a5ccdac7bd3202414fc0f4c10825b4f4ea2`。完整解压文件清单在 `evidence/conemu-manifest.json`。原包许可保留在 `components/conemu-230724/ConEmu/License.txt`。

宿主提供自己的子窗口，通过官方 `-insidewnd 0xHWND` 启动，不在 Hub 中调用 `SetParent` 捕获外部窗口。测试使用 `-basic -nosingle -noupdate -nodefterm -noregfonts -nokeyhooks -nocloseconfirm`，不保存设置，不注册为默认终端。

ConEmu 在分配的作业中启动，Plink 必须同时满足后代关系、作业归属、预期可执行文件路径和固定哈希，才会被绑定密码通道及加载尺寸适配器。绑定目标是实际 Plink PID，不能使用 ConEmu GUI PID。

测试通过 ConEmu 官方 GuiMacro 发送固定假输入。GuiMacro 参数是原生宏语言尾串，不能照搬将所有参数逐一做 Windows argv 转义的策略。本原型只处理固定测试文本，不提供任意宏或用户命令的拼接接口。

原型服务端只开放本机、固定假密码和假 SFTP 文件。`-batch -no-antispoof` 仅用于这个固定且显式校验主机密钥的测试，主应用正常连接继续保留原生确认与反欺骗交互。

```powershell
./build.ps1 -ConEmuProbe
./tests/run-conemu-tests.ps1
```

验收实际观察到：ConEmu 所有的嵌入子窗口、正确 Plink 的密码管道交付、服务端认证、远端 shell、宏输入、远端 PTY 尺寸更新。该原型没有证明中文 IME、选区、完整鼠标/全屏程序、高 DPI 或旧 Windows 兼容。

## 主应用接入与剩余验收

2026-10-21 输入修复：固定 Plink 的受控适配器设置其控制台 UTF-8 代码页，并仅对标准输入控制台的同步 `ReadFile` 调用使用 `ReadConsoleW` 转 UTF-8，跨读取保留 UTF-16 高代理项；其他句柄与异步读取原样透传。原因是本机默认代码页损坏中文，而仅改 UTF-8 代码页仍会把非 BMP 字符拆成替代字符。`evidence/pty-results.json` 已在真实回环 SSH 服务端观察到 Tab、中文和 emoji 的完整 UTF-8 字节，同时验证远端尺寸更新。该检查不代表系统 IME 或所有交互验收完成。

外围界面开始按 Everything 方向收紧：采用系统消息字体，缩小边距、工具栏及会话侧栏，移除强制深色标题栏；布局尺寸随窗口 DPI 缩放。具体视觉与跨显示器字体仍需验收。

主应用通过组合式 `ConEmuFrontend` 管理启动、焦点、作业归属、目标 Plink 发现、密码绑定及关闭。焦点宏异步执行，避免跨进程窗口消息与同步等待互相阻塞；辅助进程同样归属该终端作业。菜单支持自动、现代终端及 ConEmu，选择只对新标签生效并保存在 `data/ui.json`。自动模式在 ConPTY 或固定终端 DLL 不可用时选择 ConEmu，控件创建失败时也尝试兼容前端。组件管理已支持下载固定 ConEmu 归档、用系统 tar 解包、校验 GUI/助手/服务端/钩子，并保留许可。

最新主应用测试覆盖手动 ConEmu 选择、缺少终端 DLL 的真实隔离目录自动回退，以及两条路线的共享身份、关闭生命周期和 WinSCP 联动。`evidence/native-imports.txt` 确认没有 ConPTY 启动期导入。尚需损坏 DLL、加载失败、故障注入与完整交互验收；旧 Windows 未验证。

官方依据：[嵌入接口](https://conemu.github.io/en/InsideParent.html)、[启动参数](https://conemu.github.io/en/ConEmuArgs.html)、[GuiMacro](https://conemu.github.io/en/GuiMacro.html)。
