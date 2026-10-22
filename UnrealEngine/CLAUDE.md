# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Repo Is

Unreal Engine 5.5.4 source, forked to add **OpenHarmony (OHOS/HarmonyOS)** as a target platform. The OpenHarmony port closely mirrors the Android port's architecture (module layout, deploy flow, UPL, texture-format platform variants), so the Android implementation is the best reference when extending or debugging platform code. The platform name in build commands and code is `OpenHarmony` (enum `UnrealTargetPlatform.OpenHarmony`, platform group `UnrealPlatformGroup.OpenHarmony`, Mobile group).

## Common Commands

Run from repo root on Windows (requires VS 2022 17.4+; the OHOS build additionally requires the OpenHarmony SDK/NDK and DevEco Studio, see next section).

```bat
:: One-time setup: download binary dependencies, install prereqs, register engine
Setup.bat

:: Generate IDE project files (creates UE5.sln)
GenerateProjectFiles.bat

:: Build the editor
Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development

:: Build a game for OpenHarmony (HAP assembly + signing runs as the deploy stage of the build)
Engine\Build\BatchFiles\Build.bat <TargetName> OpenHarmony Development -project=<Path\To\Game.uproject>

:: Invoke UnrealBuildTool directly
Engine\Build\BatchFiles\RunUBT.bat <args>

:: Full cook/package/deploy via UAT
Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=<Path\To\Game.uproject> -platform=OpenHarmony -clientconfig=Development -cook -build -stage -pak -package -deploy
```

OpenHarmony build output lands in `<Project>\Binaries\OpenHarmony\` as `<TargetName>-arm64-v8a.hap` plus a generated `Install_<TargetName>-arm64-v8a.bat` that installs/launches via `hdc`.

### Tests

- Automation framework: run `UnrealEditor-Cmd.exe <Project.uproject> -ExecCmds="Automation RunTests <Namespace.Filter>" -TestExit="Automation Test Queue Empty"` (a project or the engine's own tests).
- Gauntlet: `Engine\Build\BatchFiles\RunUAT.bat Gauntlet ...`; CQTest framework lives in `Engine/Source/Developer/CQTest`.
- UBT/UAT are C# — rebuild them with `Engine\Build\BatchFiles\BuildUBT.bat` / `BuildUAT.bat` if tooling changes don't take effect.

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
- **Platform abstraction**: each platform implements backends inside generic modules (e.g. `Core/Private/OpenHarmony/OpenHarmonyPlatformMisc.cpp`, `Launch/Private/OpenHarmony/LaunchOpenHarmony.cpp`) plus dedicated platform modules under `Engine/Source/Runtime/OpenHarmony/`: `AudioMixerOpenHarmony`, `OpenHarmonyNativeGlue`, `OpenHarmonyLocalNotification`, `OpenHarmonyRuntimeSettings`.
- **UBT OpenHarmony platform**: `Engine/Source/Programs/UnrealBuildTool/Platform/OpenHarmony/` — `UEBuildOpenHarmony` (target rules incl. sanitizer flags `-EnableASan` etc.), `OpenHarmonyToolChain` (clang), `UEDeployOpenHarmony` (stages libs, processes UPL, invokes `hvigorw assembleHap` + `ohpm install`, signs with `hap-sign-tool.jar`, writes install scripts), `OpenHarmonyPlatformSDK` (SDK discovery via the env vars above).
- **UAT OpenHarmony platform**: `Engine/Source/Programs/AutomationTool/OpenHarmony/OpenHarmonyPlatform.Automation.cs` — `Platform` subclass plus texture-format variants (`OpenHarmonyPlatformDXT/ETC2/ASTC/Multi`), and `hdc`-based device process management.
- **HAP app shell**: `Engine/Build/OpenHarmony/` is the ArkTS entry project template (module.json5, EntryAbility, pages, virtual keyboard, console window, webview, sensors, workers). It is copied to the project's Intermediate dir and templated by UPL at deploy time.
- **UPL (Unreal Plugin Language)**: `Engine/Source/Programs/UnrealBuildTool/System/UnrealPluginLanguage.cs` supports OpenHarmony nodes — plugins can inject ArkTS code, permissions, resource copies, and `.har` package dependencies into the HAP shell via their `.uplugin` XML (`IOpenHarmonyDeploy`).
- **Rendering**: Vulkan RHI (ES3.1 and SM5 shader formats) and OpenGL ES 3.1, with OpenHarmony-specific shader formats (`VULKAN_ES3_1_OPENHARMONY`, `VULKAN_SM5_OPENHARMONY`, `GLSL_ES3_1_OPENHARMONY`); platform facts and previewer configs live in `Engine/Config/OpenHarmony/DataDrivenPlatformInfo.ini`.
- **OpenHarmony plugins**: `Engine/Plugins/Media/OpenHarmonyMedia`, `Engine/Plugins/Runtime/OpenHarmonyDeviceProfileSelector`, `Engine/Plugins/Runtime/OpenHarmonyMoviePlayer`, `Engine/Plugins/Runtime/WebBrowserNativeProxy`, `Engine/Plugins/Runtime/WebBrowserWidget`.
- **Native/ArkTS bridge**: the engine `.so` (`OpenHarmonyNativeGlue`) talks to the ArkTS shell through NAPI/AKI — see `Launch/Private/OpenHarmony/OpenHarmonyNAPI.cpp` and `OpenHarmonyAKI.cpp` and the workers under `Engine/Build/OpenHarmony/entry/src/main/ets/`.

## Conventions

- Epic C++ coding standard (https://docs.unrealengine.com/epic-cplusplus-coding-standard-for-unreal-engine/); formatting per the checked-in `.editorconfig` and `Engine/Extras/clang-format` rules. Build tooling follows the surrounding C# style.
- This is a huge tree — locate code by platform/module path patterns rather than repo-wide greps when possible.
