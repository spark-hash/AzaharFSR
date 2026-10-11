# FSR 1 for Azahar

This experimental fork adds AMD FidelityFX Super Resolution 1 to the shared
Vulkan presentation path used by Windows and Android. It applies EASU upscaling
to each guest screen and RCAS sharpening before screen composition. AMD's
unmodified v1.0.2 headers and their MIT notices are in `externals/fsr1`.

## Usage

Choose Vulkan and an internal resolution lower than the displayed screen size.
Enable FSR 1 in the graphics enhancement settings. Sharpening defaults to 50%;
0% disables RCAS while keeping EASU. Native-size and downscaled output bypass FSR.
OpenGL and software rendering do not use FSR. This does not implement FSR 2/3 or
frame generation, and performance gains are not guaranteed.

Windows settings are in Emulation → Configure → Graphics → Enhancements.
Android exposes an FSR switch and a 0–100% sharpening slider in graphics settings.

## Build and validation

Base commit: `5f3306d13cab40bbd5cf38f2cf3ae8b6e6eedca6`.
Clone with submodules and follow Azahar's normal platform build instructions.
Android's optimized test variant is `:app:assembleVanillaRelWithDebInfoLite`.
For an APK installed through the device's normal package installer, build with
`-Pandroid.injected.testOnly=false` and verify the final manifest has no
`testOnly=true` flag. Do not distribute an IDE-only installation artifact.

Windows Release was built and tested with Metroid: Samus Returns on an RTX 5090.
The gameplay scene, display orientation, window resizing and standalone Qt
runtime launch were checked. FSR-off and zero-sharpening configurations launched
successfully. Android ARM64 Debug and RelWithDebInfoLite builds passed; the
installable APK's signature, ABI and manifest were checked. Android GPU gameplay
has not been tested on hardware. Stereo, hybrid layouts and allocation-failure
fallbacks need further device testing.

The change includes a Windows MinGW crypt32 linkage fix for httplib.
Generated binaries, local build tools, ROMs, saves and device logs are excluded.

## Provenance

This implementation was written with substantial assistance from OpenAI Codex.
It is an experimental personal fork, not an upstream contribution. Azahar's
AI-POLICY.md restricts substantial AI-written upstream contributions; no upstream
pull request or issue was created.
