# 加密 ROM 支持（个人 fork）

本分支在 FSR 1、FXAA、SMAA 的基础上恢复 NCCH 解密读取，适用于 Android 和桌面端共用的加载核心。加密内容按需解密到内存，不改写原 ROM。

## 使用方法

1. 在 Azahar 选择的**用户数据目录**下创建 `sysdata` 文件夹；它不是 ROM 所在的文件夹。
2. 放入你自己提供的 `sysdata/aes_keys.txt`。本次 Android APK 关闭了内置密钥 blob，需要完整、正确的密钥配置。
3. 对使用 seed 加密的游戏，还需提供含该游戏条目的 `sysdata/seeddb.bin`。
4. 完全退出并重新启动应用，再打开游戏。密钥加载器会缓存配置。

`aes_keys.txt` 使用现有加载器的 `:AES` 分区与 `名称=十六进制值` 格式；各值为 16 字节（32 个十六进制字符）。需要 `generatorConstant` 和 `slot0x2CKeyX`；按照游戏的加密版本，可能还需要 `slot0x25KeyX`、`slot0x18KeyX` 或 `slot0x1BKeyX`。系统 fixed-key 内容需要 `slot0x11KeyN`。游戏自身的 KeyY 从 NCCH 头取得，不需逐个配置。本文不提供真实密钥或下载地址。

## 实现范围

- 加密 NCSD 卡带（`.cci` / `.3ds`）及 NCCH 应用容器（`.cxi` / `.app`）。
- NCCH v0/v1/v2 CTR、Secure1/2/3/4、seed crypto、应用 zero fixed-key 和用户提供的系统 fixed-key。
- ExHeader、ExeFS 头与内容，以及 RomFS 随机读取；`icon` / `banner` 使用主密钥，代码与 RomFS 使用次密钥。
- ExHeader 哈希、常规单块 ExeFS 头哈希、ExeFS 内容哈希、RomFS superblock 哈希和 seed 验证。密钥或数据不正确时返回解密错误，不把校验失败的代码交给 CPU。
- 明文 ExeFS / RomFS 的覆盖与现有模组路径。
- RomFS 的解密参数随即时存档保存；旧版本的明文读取器存档仍可载入。不要分享含个人解密数据的即时存档。

**加密 CIA 安装没有在本次开放**；外层 CBC、内容转换与安装校验需单独实现及验证。此版本不代表无需密钥即可加载任何文件，也不支持恢复损坏的 ROM。

## 验证

`src/tests/core/file_sys/ncch_crypto.cpp` 使用自行生成的内容和测试密钥，覆盖 CTR 版本、卡带封装、主/次密钥、seed 缺失与错误、代码损坏、明文覆盖，以及跨缓存页、非对齐、重复和 EOF 读取。运行过滤器为 `[ncch-crypto]`。测试不会启动模拟器或运行游戏，也不包含商业 ROM。

本次核心验证结果：8 个测试、154 条断言全部通过。

Android 构建：`VanillaRelWithDebInfoLite`、ARM64、`ENABLE_BUILTIN_KEYBLOB=OFF`。FSR / FXAA / SMAA 设置保留。APK 签名、ABI 和对齐另行检查；未在掌机安装或验证真实加密游戏。按用户要求不进行 Windows 模拟器运行测试。

解密算法根据本项目删除前的历史实现适配当前 `IOFileBase` / `SubIOFile` 接口；CTR 与主/次密钥选择也参考 [GodMode9 的 NCCH 实现](https://github.com/d0k3/GodMode9/blob/master/arm9/source/game/ncch.c)。本次修改有 AI 协助，仅提交到个人 fork。
