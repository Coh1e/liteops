# LiteOps 交接

日期：2026-10-24。状态：原 PuTTY Hub 的 C++ 迁移目标未完成，保持暂停。本轮整理为 LiteOps 并初始化 Git，没有提交或推送。

## 方向与实现

C++23 / Win32 外壳组合 PuTTY 生态：Plink 后端、HwndTerminal 主前端、ConEmu 兼容前端，独立双栏 WinSCP 从活动 SSH 快照启动。保持 Everything 式紧凑系统控件；不扩展为新的 SSH 库或文件管理器。

内部二进制名仍为 PuTTYHub.exe，本轮不更改业务名称、协议或运行逻辑。构建入口 build.ps1；测试入口 tests/*.ps1；原有源码包验证与打包入口为 verify-source.ps1、package.ps1。

## 最新停点

RESUME-PLAN.md 是暂停检查点；MIGRATION-STATUS.md 的旧记录需结合其顶部说明解读。CPP-MIGRATION-PLAN.md 保留原始目标，FRONTENDS.md、PAGEANT.md、SHARING.md、MIGRATION-CONTRACT.md 记录专项约定。

核心检查历史为 58 项。加密 PPK 经人工 Pageant 解锁后的 Hub/WinSCP 代理认证已有历史通过证据。本地 Vim 经 ConPTY 的显示、尺寸、保存及退出已检查，不等于完整 SSH/HwndTerminal 验收。

暂停前已新增 tests/vim-fixture.py 并修改 SSH fixture、应用测试及 src/app.cpp；历史编译与 Python 语法检查通过，真实 Vim 经 SSH/Plink/Hub 的完整链路尚未运行。旧 source-proof-currentness.json 为 stale，完整新版 23 报告验证未重跑，不能用旧预览包代表当前源码。

## 恢复计划

人类恢复后，先在项目内准备受控组件、测试依赖和 Vim，再运行完整 SSH Vim 链路。随后完善 ConEmu 全屏程序、IME、鼠标、剪贴板、跨屏 DPI、对话框、旧 Windows 和网络超时验收；人工交互保持人工验收。最后按原迁移计划审计，重新进行独立源码验证，证据与二进制一致后才打包。

## 迁移影响与归档

源码、vendor/json.hpp 及许可、licenses/、tests/legacy-source/ 保留。bin、components、data、runtime、evidence、测试依赖/工具和历史测试现场移出项目；构建/测试前需在本项目重新准备依赖。本轮已把验证及打包的输出目录收进项目 publish/，不再写入父目录。

原 C# integrator/、integrator-native/ 完整现场、设计与历史发布包保存在本机 D:/Projects/rea/archive/legacy-workspace。归档不随 GitHub 上传；涉及旧版数据、源样本或历史证据时，由人类提供所需副本。不要读取其他项目或把归档私有 data/ 提交 Git。

OpenPi/DeepSeek/rea/桥接/技术沙箱均未配置。本轮不恢复构建、测试或发布，也不把历史检查写成本轮通过。原始构建入口仍含本机工具路径；迁移后实际可构建性尚未运行验证。