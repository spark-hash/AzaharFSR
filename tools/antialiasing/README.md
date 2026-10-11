# Presentation anti-aliasing validation

From the repository root, compile the standalone GPU harness with a C++20 compiler:

```sh
c++ -std=c++20 -O2 -Iexternals/vulkan-headers/include -Iexternals/smaa tools/antialiasing/gpu_validation.cpp -o gpu_validation
python tools/antialiasing/validate.py --validator glslangValidator --gpu-test ./gpu_validation
```

Linux may require `-ldl`; Windows MinGW can use `-static` to avoid runtime DLLs.
The harness dynamically loads the installed Vulkan driver. It dispatches the actual
five presentation shaders on an odd-size image (127x93), reads back FXAA and SMAA
results, and requires intermediate edge pixels. It then reuses the images for a
flat frame, requiring exact color preservation and no stale SMAA edges. The test
does not create a window or access any game or connected Android device.

Omit `--gpu-test` to validate GLSL/SPIR-V compilation only. This checks the algorithm
and GPU shaders; emulator integration, layouts and Android GPU behavior require
separate runtime tests.
