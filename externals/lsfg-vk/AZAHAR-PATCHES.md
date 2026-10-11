# LSFG integration provenance

Vendored from FrankBarretta/lsfg-vk-android release commit 3e89e5439a98f55d5acb003d20039426ab24e69c (MIT).
Dependencies: PancakeTAS/dxbc 78ab59a8aaeb43cd1b0a5e91ba86722433a10b78;
trailofbits/pe-parse 31ac5966503689d5693cd9fb520bd525a8710e17;
zeux/volk be3dbd49bf77052665e96b6c7484af855e7e5f67.
Their original license files are retained.

Azahar changes borrow the host Vulkan instance/device/queue and ordinary images, use legacy barriers,
initialize the fallback image, and release handle wrapper allocations.
Only the LSFG 3.1 Performance chain is compiled. Shader conversion uses the upstream DXBC translator.
Lossless Scaling binaries and proprietary shaders are not included.
