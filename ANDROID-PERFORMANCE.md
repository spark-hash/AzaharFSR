# Android performance diagnostics

Enable Performance diagnostics in the in-game overlay options menu. Enabling it also enables the performance overlay. Disable diagnostics to restore the configurable compact display; hide the performance overlay to hide either mode. The mode is saved on this device.

The panel refreshes once per second, uses multiple lines and is bounded by screen width. Existing overlay position settings apply. No phone installation or device testing was performed. Independent Vulkan secondary windows show GSP source submissions and output presentations separately, plus that window's LSFG state. Source submissions follow the game screens enabled by the window's layout, not window presentation or primary-screen updates. Repeated output frames do not increase source FPS. GSP submissions can still contain identical pixels; this does not measure animation uniqueness. Composite layouts count the larger submission delta per render tick. OpenGL secondary telemetry shows unavailable.

- FPS counts game GSP frame submissions. With active LSFG, the same line shows game FPS → output presentation FPS and the current 2× frame generation ratio. The 60 FPS mode uses 2× for native 30 FPS and bypasses native 60 FPS; removed 120 FPS settings migrate to 60 FPS. The output rate is measured from real and generated presentation counters; it is not assumed to equal a fixed multiple of game FPS or the display refresh rate. When interpolation is inactive, the line shows its status instead. An active independent second screen gets its own FPS line and status, measured from that window's presentation counters.
- Speed is emulated time divided by elapsed wall time. 100% means normal emulation speed.
- The four rows are FPS/frame generation, speed, memory/battery, and GPU driver. CPU stage timings and emulated system Hz are omitted. The configurable compact overlay also uses the merged FPS line, and its frame-time option shows only total processing time.
- Memory uses process RSS and the actual OS page size (including 16 KB devices), displayed in binary megabytes. Failed reads display a dash. Free is Android available memory, including reclaimable memory.
- Battery temperature is not CPU or GPU temperature.
- Driver data comes from the active renderer, copied under a mutex, so selecting a driver that fails to load will not falsely label the system fallback. Qualcomm is abbreviated as QCOM (高通), Turnip as Turnip (萝卜). Vulkan reports its decoded driver version; OpenGL extracts V@ or Mesa driver versions and omits unavailable versions rather than showing the OpenGL API version. Custom archive revision names such as R-series are not inferred from the runtime version.

AI-assisted implementation in this personal fork.
