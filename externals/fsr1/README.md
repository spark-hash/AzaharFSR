# FidelityFX Super Resolution 1

`ffx_a.h` and `ffx_fsr1.h` are unmodified AMD headers from
[FidelityFX-FSR v1.0.2](https://github.com/GPUOpen-Effects/FidelityFX-FSR/tree/v1.0.2/ffx-fsr).
Their MIT license and copyright notices are included at the top of each header.

The Vulkan presentation shaders use FP32 EASU and RCAS for Windows and Android.
The headers are embedded at build time, so runtime downloads are unnecessary.
