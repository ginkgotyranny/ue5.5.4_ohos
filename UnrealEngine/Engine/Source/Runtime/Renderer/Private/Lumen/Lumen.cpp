// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lumen.h"
#include "RenderCore.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "LumenSceneData.h"
#include "RenderUtils.h"

int32 GLumenSupported = 1;
FAutoConsoleVariableRef CVarLumenSupported(
	TEXT("r.Lumen.Supported"),
	GLumenSupported,
	TEXT("Whether Lumen is supported at all for the project, regardless of platform.  This can be used to avoid compiling shaders and other load time overhead."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenAsyncCompute(
	TEXT("r.Lumen.AsyncCompute"),
	1,
	TEXT("Whether Lumen should use async compute if supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenWaveOps(
	TEXT("r.Lumen.WaveOps"),
	1,
	TEXT("Whether Lumen should use wave ops if supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenThreadGroupSize32(
	TEXT("r.Lumen.ThreadGroupSize32"),
	1,
	TEXT("Whether to prefer dispatches in groups of 32 threads on HW which supports it (instead of standard 64)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenLightingDataFormat(
	TEXT("r.Lumen.LightingDataFormat"),
	0,
	TEXT("Data format for surfaces storing lighting information (e.g. radiance, irradiance). 0=PF_FloatR11G11B10 (default), 1=PF_FloatRGBA (64 bit)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool DoesRuntimePlatformSupportLumen()
{
	return UE::PixelFormat::HasCapabilities(PF_R16_UINT, EPixelFormatCapabilities::TypedUAVLoad);
}

bool Lumen::UseAsyncCompute(const FViewFamilyInfo& ViewFamily)
{
	bool bUseAsync = GSupportsEfficientAsyncCompute
		&& CVarLumenAsyncCompute.GetValueOnRenderThread() != 0;
		
	if (Lumen::UseHardwareRayTracing(ViewFamily))
	{
		// Async for Lumen HWRT path can only be used by inline ray tracing
		bUseAsync &= CVarLumenAsyncCompute.GetValueOnRenderThread() != 0 && Lumen::UseHardwareInlineRayTracing(ViewFamily);
	}

	return bUseAsync;
}

bool Lumen::UseWaveOps(EShaderPlatform ShaderPlatform)
{
	return CVarLumenWaveOps.GetValueOnRenderThread() != 0
		&& GRHISupportsWaveOperations
		&& RHISupportsWaveOperations(ShaderPlatform);
}

bool Lumen::UseThreadGroupSize32()
{
	return GRHISupportsWaveOperations && GRHIMinimumWaveSize <= 32 && CVarLumenThreadGroupSize32.GetValueOnAnyThread() != 0;
}

EPixelFormat Lumen::GetLightingDataFormat()
{
	return CVarLumenLightingDataFormat.GetValueOnRenderThread() == 0 ? PF_FloatR11G11B10 : PF_FloatRGBA;
}

extern FVector3f ComputePixelFormatQuantizationError(EPixelFormat PixelFormat);

FVector3f Lumen::GetLightingQuantizationError()
{
	return ComputePixelFormatQuantizationError(GetLightingDataFormat());
}

namespace Lumen
{
	bool AnyLumenHardwareRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
#if RHI_RAYTRACING

		const bool bLumenGI = ShouldRenderLumenDiffuseGI(Scene, View);
		const bool bLumenReflections = ShouldRenderLumenReflections(View);

		const bool bGIHWRT = bLumenGI
			&& (UseHardwareRayTracedScreenProbeGather(*View.Family)
				|| UseHardwareRayTracedRadianceCache(*View.Family)
				|| UseHardwareRayTracedDirectLighting(*View.Family)
				|| UseHardwareRayTracedTranslucencyVolume(*View.Family));

		const bool bReflectionsHWRT = bLumenReflections
			&& UseHardwareRayTracedReflections(*View.Family);

		const bool bVisualizeHWRT = (bLumenGI || bLumenReflections) && Lumen::ShouldVisualizeHardwareRayTracing(*View.Family);
		const bool bRadiosityHWRT = (bLumenGI || bLumenReflections) && Lumen::ShouldRenderRadiosityHardwareRayTracing(*View.Family);
		const bool bResult = bGIHWRT || bReflectionsHWRT || bVisualizeHWRT || bRadiosityHWRT;

		// RTDIAG: temporary bring-up logging, remove once ray tracing pipeline is verified on device
		static uint32 GDiagCounter = 0;
		if (GDiagCounter < 3 || (GDiagCounter % 600) == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("RTDIAG AnyLumenHWRT: LumenGI=%d LumenRefl=%d GI_HWRT=%d Refl_HWRT=%d => %d"),
				(int32)bLumenGI, (int32)bLumenReflections, (int32)bGIHWRT, (int32)bReflectionsHWRT, (int32)bResult);
		}
		GDiagCounter++;

		return bResult;
#else
		return false;
#endif
	}

	bool AnyLumenHardwareInlineRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
		if (!AnyLumenHardwareRayTracingPassEnabled(Scene, View))
		{
			return false;
		}

		return Lumen::UseHardwareInlineRayTracing(*View.Family);
	}
}

bool Lumen::ShouldHandleSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& (Scene->SkyLight->ProcessedTexture || Scene->SkyLight->bRealTimeCaptureEnabled)
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& DoesPlatformSupportLumenGI(Scene->GetShaderPlatform(), /*bSkipProjectCheck*/ false)
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool ShouldRenderLumenForViewFamily(const FScene* Scene, const FSceneViewFamily& ViewFamily, bool bSkipProjectCheck)
{
	return Scene
		&& Scene->DefaultLumenSceneData != nullptr
		&& (ViewFamily.Views.Num() <= LUMEN_MAX_VIEWS || ViewFamily.Views[0]->bIsSceneCaptureCube)
		&& DoesPlatformSupportLumenGI(Scene->GetShaderPlatform(), bSkipProjectCheck);
}

bool Lumen::IsSoftwareRayTracingSupported()
{
	return DoesProjectSupportDistanceFields();
}

bool Lumen::IsLumenFeatureAllowedForView(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck, bool bSkipProjectCheck)
{
	const bool bHasFamily = View.Family != nullptr;
	const bool bRuntimePlatformSupportsLumen = DoesRuntimePlatformSupportLumen();
	const bool bShouldRenderForFamily = bHasFamily && Scene != nullptr && ShouldRenderLumenForViewFamily(Scene, *View.Family, bSkipProjectCheck);
	// Don't update scene lighting for secondary views
	const bool bIsSecondaryView = View.bIsPlanarReflection || View.bIsReflectionCapture;
	const bool bHasViewState = View.State != nullptr;
	const bool bTracingDataAvailable = bSkipTracingDataCheck || (bHasFamily && (Lumen::UseHardwareRayTracing(*View.Family) || IsSoftwareRayTracingSupported()));
	const bool bResult = bHasFamily && bRuntimePlatformSupportsLumen && bShouldRenderForFamily && !bIsSecondaryView && bHasViewState && bTracingDataAvailable;

	// RTDIAG: temporary bring-up logging, remove once ray tracing pipeline is verified on device
	static uint32 GDiagCounter = 0;
	if (GDiagCounter < 3 || (GDiagCounter % 600) == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("RTDIAG IsLumenFeatureAllowedForView: HasFamily=%d RuntimeLumen(R16UAVLoad)=%d FamilyAllowed=%d SecondaryView=%d HasState=%d TracingData=%d => %d"),
			(int32)bHasFamily, (int32)bRuntimePlatformSupportsLumen, (int32)bShouldRenderForFamily,
			(int32)bIsSecondaryView, (int32)bHasViewState, (int32)bTracingDataAvailable, (int32)bResult);
	}
	GDiagCounter++;

	return bResult;
}

bool Lumen::UseGlobalSDFObjectGrid(const FSceneViewFamily& ViewFamily)
{
	if (!Lumen::IsSoftwareRayTracingSupported())
	{
		return false;
	}

	// All features use Hardware RayTracing, no need to update voxel lighting
	if (Lumen::UseHardwareRayTracedSceneLighting(ViewFamily)
		&& Lumen::UseHardwareRayTracedScreenProbeGather(ViewFamily)
		&& Lumen::UseHardwareRayTracedReflections(ViewFamily)
		&& Lumen::UseHardwareRayTracedRadianceCache(ViewFamily)
		&& Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily)
		&& Lumen::UseHardwareRayTracedVisualize(ViewFamily))
	{
		return false;
	}

	return true;
}

uint32 Lumen::GetMeshCardDistanceBin(float Distance)
{
	uint32 OffsetDistance = FMath::Max(1, (int32)(Distance - 1000));
	uint32 Bin = FMath::Min(FMath::FloorLog2(OffsetDistance), Lumen::NumDistanceBuckets - 1);
	return Bin;
}
