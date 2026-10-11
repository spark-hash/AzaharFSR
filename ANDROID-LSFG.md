# Android LSFG

在图形设置中进入「LSFG 着色器」，选择你购买的 Lossless Scaling 中的 Lossless.dll。
程序仅解析 DLL 并在本地提取、转换着色器，不运行 DLL。APK 和仓库不包含此 DLL 或专有着色器。

使用 Vulkan，并选择「LSFG 2× (30 → 60)」。支持全局及每游戏覆盖；可以在游戏设置返回时生效。
着色器导入是设备共享资源。首次启动需要预热帧历史与创建计算管线，可能出现短暂停顿。
切换显示尺寸、暂停较久后重建历史。稳定 30 FPS 输入会在相邻真实帧之间显示一张中间帧。
原生接近 60 FPS 输入自动绕过插帧。该功能增加显示帧，不修改游戏速度、逻辑帧率或动画。

打开性能诊断可以查看 LSFG 真实/输出 FPS、计算提交至完成耗时和状态。
耗时包含 CPU 提交及 GPU 等待，不是纯 GPU 时间。输出 FPS 统计成功提交的呈现，不能证明面板实际扫描帧率。
缺少着色器或驱动不兼容时回退真实帧，诊断显示原因状态，详细错误在日志中。

要求 Vulkan 1.3、Vulkan memory model、FP32 storage images 和至少 1024 compute workgroup invocations。
第一版为 LSFG 3.1 Performance FP32，使用同步 GPU 等待，可能增加延迟和降低真实帧率。
暂不支持 FP16、多倍插帧或 OpenGL。不同 Lossless Scaling 版本的资源布局可能改变，导入不兼容时报告失败。
已验证提供的 DLL 可提取 26 个着色器，全部通过 Vulkan 1.3 SPIR-V 校验。
Android 构建和设置单元测试通过；尚未在 Android 设备 GPU 上验证画面、稳定性或性能。

导入器还验证了无效 PE 文件和缺少着色器资源的 PE 文件会失败，且不会写入输出目录。
