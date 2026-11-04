# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Repo Is

Unreal Engine 5.5.4 source, forked to add **OpenHarmony (OHOS/HarmonyOS)** as a target platform, with the engine tree living in the `UnrealEngine/` subfolder. Current focus: bringing up the **hardware ray tracing pipeline (Lumen HWRT) on Vulkan SM5 for OpenHarmony**, verified on a Huawei Maleoon 955 GPU.

Repo-root layout (everything outside `UnrealEngine/` is untracked scratch/support material):

- `UnrealEngine/` — the engine. All build commands run from here.
- `ue5_ohos_rtp/` — the test game project used for RT bring-up (`ue5_ohos_rtp.uproject`, C++ module `ue5_ohos_rtp`). Its `Config/DefaultEngine.ini` carries the required runtime cvars (see RT section).
- `rt-pipeline-patches/` — the upstream Android/Huawei RT patches (`0001-*`, `0002-*`, `combined-all-changes.patch`) plus `README-移植说明.txt` describing what must be synced by hand outside the patches. These are already applied to this fork (commit `9baa1ad5b`); keep them as reference, don't re-apply.
- `logs/`, `hilog_outall.txt`, `ue5_ohos_rtp.log` — device hilog captures from RT runs.

The OpenHarmony port closely mirrors the Android port's architecture (module layout, deploy flow, UPL, texture-format platform variants), so the Android implementation is the best reference when extending or debugging platform code. The platform name in build commands and code is `OpenHarmony` (enum `UnrealTargetPlatform.OpenHarmony`, platform group `UnrealPlatformGroup.OpenHarmony`, Mobile group).

## Common Commands

Run from `UnrealEngine\` on Windows (requires VS 2022 17.4+; the OHOS build additionally requires the OpenHarmony SDK/NDK and DevEco Studio, see next section).

```bat
:: One-time setup: download binary dependencies, install prereqs, register engine
Setup.bat

:: Generate IDE project files (creates UE5.sln)
GenerateProjectFiles.bat

:: Build the editor
Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development

:: Build a game for OpenHarmony (HAP assembly + signing runs as the deploy stage of the build)
Engine\Build\BatchFiles\Build.bat ue5_ohos_rtp OpenHarmony Development -project=E:\UE\ue5.5.4_ohos\ue5_ohos_rtp\ue5_ohos_rtp.uproject

:: Invoke UnrealBuildTool directly
Engine\Build\BatchFiles\RunUBT.bat <args>

:: Full cook/package/deploy via UAT
Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=<Path\To\Game.uproject> -platform=OpenHarmony -clientconfig=Development -cook -build -stage -pak -package -deploy
```

OpenHarmony build output lands in `<Project>\Binaries\OpenHarmony\` as `<TargetName>arm64.hap` (plus `<TargetName>-arm64.so`) and a generated `Install_<TargetName>arm64.bat` that installs/launches via `hdc`.

### Tests

- Automation framework: run `UnrealEditor-Cmd.exe <Project.uproject> -ExecCmds="Automation RunTests <Namespace.Filter>" -TestExit="Automation Test Queue Empty"` (a project or the engine's own tests).
- Gauntlet: `Engine\Build\BatchFiles\RunUAT.bat Gauntlet ...`; CQTest framework lives in `Engine/Source/Developer/CQTest`.
- UBT/UAT are C# — rebuild them with `Engine\Build\BatchFiles\BuildUBT.bat` / `BuildUAT.bat` if tooling changes don't take effect.

### Working in this tree

- This is a huge tree. Repo-wide `grep -r` over `UnrealEngine/Engine/Source` takes minutes; scope searches to a module/platform directory (`Runtime/VulkanRHI`, `Runtime/Renderer/Private/Lumen`, `*/OpenHarmony/*`) or use `git grep`.

## OpenHarmony Build Requirements

Environment variables (no AutoSDK scripts exist for OpenHarmony; paths are user-provided):

- `OHSDKROOT` — OpenHarmony SDK root (must contain `openharmony\<ver>\toolchains\hdc`)
- `OHNDKROOT` — OpenHarmony NDK root (clang toolchain; UE builds with a ClangToolChain derived from it, target triple `aarch64-linux-ohos`)
- `DEVECO_PATH` — DevEco Studio install; the HAP build locates `jbr`, `tools\hvigor`, `tools\ohpm`, `tools\node`, and `sdk` under it

SDK/NDK/tool versions are pinned in `Engine/Config/OpenHarmony/OpenHarmony_SDK.json` (API level 12 era). Architectures: Arm64 (`arm64-v8a`, folder name `a`) and X64 (folder `x`).

HAP signing reads `CertPath`/`StoreFile`/`Profile`/`KeyAlias`/passwords from `[/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings]`; when those aren't configured it falls back to auto-signing with keys bundled in `Engine/Build/OpenHarmony/temp/tools` (hap-sign-tool.jar, `.p12`/`.pem`/provision templates).

## Engine Architecture (big picture)

- **Build system**: `Engine/Source/Programs/UnrealBuildTool` (C#) resolves modules (`*.Build.cs`), targets (`*.Target.cs`), and platform toolchains. `Engine/Source/Programs/AutomationTool` (UAT) handles cook/package/deploy orchestration. Program build data is checked in as `.csproj` files under `Engine/Intermediate/ProjectFiles`.
- **Modules**: engine code is `Engine/Source/{Runtime,Editor,Developer,ThirdParty,Programs}`; plugins in `Engine/Plugins`; config in `Engine/Config` (per-platform ini layers, e.g. `Engine/Config/OpenHarmony/`).
- **Platform abstraction**: each platform implements backends inside generic modules (e.g. `Core/Private/OpenHarmony/OpenHarmonyPlatformMisc.cpp`, `Launch/Private/OpenHarmony/LaunchOpenHarmony.cpp`) plus dedicated platform modules under `Engine/Source/Runtime/OpenHarmony/` (`AudioMixerOpenHarmony`, `OpenHarmonyNativeGlue`, `OpenHarmonyLocalNotification`, `OpenHarmonyRuntimeSettings`) and editor/target-platform modules under `Engine/Source/Developer/OpenHarmony/` (`OpenHarmonyTargetPlatform*`, `OpenHarmonyPlatformEditor`, `OpenHarmonyDeviceDetection`).
- **UBT OpenHarmony platform**: `Engine/Source/Programs/UnrealBuildTool/Platform/OpenHarmony/` — `UEBuildOpenHarmony` (target rules incl. sanitizer flags `-EnableASan` etc.), `OpenHarmonyToolChain` (clang), `UEDeployOpenHarmony` (stages libs, processes UPL, invokes `hvigorw assembleHap` + `ohpm install`, signs with `hap-sign-tool.jar`, writes install scripts), `OpenHarmonyPlatformSDK` (SDK discovery via the env vars above).
- **UAT OpenHarmony platform**: `Engine/Source/Programs/AutomationTool/OpenHarmony/OpenHarmonyPlatform.Automation.cs` — `Platform` subclass plus texture-format variants (`OpenHarmonyPlatformDXT/ETC2/ASTC/Multi`), and `hdc`-based device process management.
- **HAP app shell**: `Engine/Build/OpenHarmony/` is the ArkTS entry project template (module.json5, EntryAbility, pages, virtual keyboard, console window, webview, sensors, workers). It is copied to the project's Intermediate dir and templated by UPL at deploy time.
- **UPL (Unreal Plugin Language)**: `Engine/Source/Programs/UnrealBuildTool/System/UnrealPluginLanguage.cs` supports OpenHarmony nodes — plugins can inject ArkTS code, permissions, resource copies, and `.har` package dependencies into the HAP shell via their `.uplugin` XML (`IOpenHarmonyDeploy`).
- **Rendering**: Vulkan RHI (ES3.1 and SM5 shader formats) and OpenGL ES 3.1, with OpenHarmony-specific shader formats (`VULKAN_ES3_1_OPENHARMONY`, `VULKAN_SM5_OPENHARMONY`, `GLSL_ES3_1_OPENHARMONY`); platform facts and previewer configs live in `Engine/Config/OpenHarmony/DataDrivenPlatformInfo.ini`.
- **OpenHarmony plugins**: `Engine/Plugins/Media/OpenHarmonyMedia`, `Engine/Plugins/Runtime/OpenHarmonyDeviceProfileSelector`, `Engine/Plugins/Runtime/OpenHarmonyMoviePlayer`, `Engine/Plugins/Runtime/WebBrowserNativeProxy`, `Engine/Plugins/Runtime/WebBrowserWidget`.
- **Native/ArkTS bridge**: the engine `.so` (`OpenHarmonyNativeGlue`) talks to the ArkTS shell through NAPI/AKI — see `Launch/Private/OpenHarmony/OpenHarmonyNAPI.cpp` and `OpenHarmonyAKI.cpp` and the workers under `Engine/Build/OpenHarmony/entry/src/main/ets/`.

## Hardware Ray Tracing on OpenHarmony (current work)

The RT path is a port of the Android/Huawei Maleoon bring-up patches. It spans several layers that must stay consistent; a change in one usually needs a matching change elsewhere:

- **Platform gating**: `bSupportsRayTracingShaders` + `BindlessSupport` in `Engine/Config/OpenHarmony/DataDrivenPlatformInfo.ini` (and the Android one); `GetRayTracingMode()` in `OpenHarmonyTargetPlatformSettings.h`; RT compile gates in `Engine/Source/Runtime/Engine/Private/ShaderCompiler/ShaderCompiler.cpp`; Lumen allowed on `VULKAN_SM5_OPENHARMONY` in the Lumen renderer code (`Lumen.cpp`, `LumenScreenProbeGather.cpp`, `LumenSceneLighting.cpp`).
- **Bindless + classic-UAV hybrid descriptor layout**: `[SF_VULKAN_SM5_OPENHARMONY]`/`[SF_VULKAN_SM5_ANDROID]` sections with `BindlessResources/BindlessSamplers=RayTracingShaders` in `Base*Engine.ini`/`*Engine.ini`; `ShaderParameterParser` hook in RenderCore; `Engine/Shaders/Public/Platform/Vulkan/VulkanCommon.ush`; `SpirVShaderCompiler.inl` reflection and `VulkanShaderCompiler.cpp` (UAVs kept classic for OHOS formats); `VulkanDescriptorSets.cpp` (bindless for RayTracingShaders only); `VulkanDevice.cpp`/`VulkanExtensions.cpp`/`VulkanRayTracing.cpp` (vendor handling — Kirin/Huawei vendor id `0x19E5`, inline SBT fix, RT probes).
- **Vulkan SM5 profiles**: OpenHarmony ES3.1/SM5/SM5-RT profiles registered in the Vulkan profile list (commit `f9d89f6c4`).
- **Project-side runtime cvars** (the `[SF_*]` ini sections are not shipped, so bindless must be forced via cvars in the project's `DefaultEngine.ini` `[SystemSettings]`): `r.Lumen.HardwareRayTracing.Inline=0`, `rhi.Bindless.Resources=RayTracingOnly`, `rhi.Bindless.Samplers=RayTracingOnly`, `r.Vulkan.RayTracing.AllowDeferredOperation=-1` (synchronous RT PSO compile; deferred ops crash on a `bCompleted` assert). `ue5_ohos_rtp/Config/DefaultEngine.ini` is the reference.
- **Device profile**: Maleoon 955 profile and the global Vulkan SM5 enable were hand-ported into `Engine/Config/BaseDeviceProfiles.ini` (this fork had already modified that file, so the patch didn't apply cleanly — re-diff by hand if touching it).

Verification on device (grep hilog): `RT probe:` lines, `Ray tracing shaders are enabled.`, `runtime bindless resources=2 samplers=2, Device->SupportsBindless=1`, and `vkCmdTraceRaysIndirectKHR` reaching the driver. Cook logs must be free of shader errors (global shaders expose port omissions first).

**RTDIAG diagnostics** (commit `03b283a54`, marked temporary): per-frame logging tagged `RTDIAG ` in `DeferredShadingRenderer.cpp`, `Lumen/Lumen.cpp`, `Lumen/LumenHardwareRayTracingCommon.cpp`, `Lumen/LumenReflections.cpp`. Still used for the open GPU page-fault / device-lost-after-~420-frames investigation on Maleoon; remove once root-caused, don't build on top of it.

Toolchain cautions from the patch notes: keep the bundled dxc (ShaderConductor/Win64, 1.8-dev) — moving to 1.9 cascades into RawBufferLoad2 removal and by-value resource-parameter store errors.

## Conventions

- Epic C++ coding standard (https://docs.unrealengine.com/epic-cplusplus-coding-standard-for-unreal-engine/); formatting per the checked-in `UnrealEngine/.editorconfig` and `Engine/Extras/clang-format` rules. Build tooling follows the surrounding C# style.
- Commit messages in this fork describe the OHOS adaptation and reference the Android counterpart when the change mirrors Android.
