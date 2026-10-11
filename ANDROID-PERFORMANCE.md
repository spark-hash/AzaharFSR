# Android performance diagnostics

Enable Performance diagnostics in the in-game overlay options menu. Enabling it also enables the performance overlay. Disable diagnostics to restore the configurable compact display; hide the performance overlay to hide either mode. The mode is saved on this device.

The panel refreshes once per second, uses multiple lines and is bounded by screen width. Existing overlay position settings apply. No phone installation or device testing was performed.

- Game FPS counts GSP frame submissions; system Hz counts emulated LCD VBlanks. A 30 FPS game can run at normal speed with a roughly 60 Hz system rate.
- Speed is emulated time divided by elapsed wall time. 100% means normal emulation speed.
- Active frame is mean processing time per system frame, excluding frame limiter/vsync outside the active frame. It is not display latency or an individual frame percentile.
- GPU commands measures CPU wall time handling guest GPU commands, not host GPU execution time.
- Present / wait measures SwapBuffers, including waits for the host GPU. It cannot alone identify a GPU bottleneck.
- IPC, SVC and Other show mean processing time per system frame. Nested GPU and IPC time is excluded from parent categories.
- Process RSS uses the actual OS page size (including 16 KB devices), in MiB. Failed reads display a dash. Free is Android available memory, including reclaimable memory.
- Battery temperature is not CPU or GPU temperature.

AI-assisted implementation in this personal fork.
