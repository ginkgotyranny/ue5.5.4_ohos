#include "OpenHarmonyTargetPlatformControls.h"
#include "OpenHarmonyTargetPlatformSettings.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Serialization/Archive.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IOpenHarmonyDeviceDetectionModule.h"
#include "Interfaces/IOpenHarmonyDeviceDetection.h"
#include "Modules/ModuleManager.h"
#include "Misc/SecureHash.h"
#include "AnalyticsEventAttribute.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#include "Sound/SoundWave.h"
#include "TextureResource.h"
#endif

#define LOCTEXT_NAMESPACE "FOpenHarmonyTargetPlatformControls"

class Error;
class FOpenHarmonyTargetDevice;
class FConfigCacheIni;
class FModuleManager;
class FScopeLock;
class FStaticMeshLODSettings;
class FTargetDeviceId;
class FTSTicker;
class IOpenHarmonyDeviceDetectionModule;
class UTexture;
class UTextureLODSettings;
struct FOpenHarmonyDeviceInfo;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;

static FString GetLicensePath()
{
	auto &OpenHarmonyDeviceDetection = FModuleManager::LoadModuleChecked<IOpenHarmonyDeviceDetectionModule>("OpenHarmonyDeviceDetection");
	IOpenHarmonyDeviceDetection* DeviceDetection = OpenHarmonyDeviceDetection.GetOpenHarmonyDeviceDetection();
	FString HDCPath = DeviceDetection->GetHDCPath();

	if (!FPaths::FileExists(HDCPath))
	{
		return TEXT("");
	}

	// strip off the hdc.exe part
	FString PlatformToolsPath;
	FString Filename;
	FString Extension;
	FPaths::Split(HDCPath, PlatformToolsPath, Filename, Extension);

	// remove the platform-tools part and point to licenses
	FPaths::NormalizeDirectoryName(PlatformToolsPath);
	FString LicensePath = PlatformToolsPath + "/../licenses";
	FPaths::CollapseRelativeDirectories(LicensePath);

	return LicensePath;
}

#if WITH_ENGINE
static bool GetLicenseHash(FSHAHash& LicenseHash)
{
	return true;
}
#endif

static bool HasLicense()
{
	// doesn't match
	return false;
}

FOpenHarmonyTargetPlatformControls::FOpenHarmonyTargetPlatformControls(bool bInIsClient, ITargetPlatformSettings* InTargetPlatformSettings, const TCHAR* FlavorName, const TCHAR* OverrideIniPlatformName)
	: TNonDesktopTargetPlatformControlsBase(bInIsClient, InTargetPlatformSettings, FlavorName, OverrideIniPlatformName)
	, DeviceDetection(nullptr)

{
	TickDelegate = FTickerDelegate::CreateRaw(this, &FOpenHarmonyTargetPlatformControls::HandleTicker);
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 4.0f);

	OpenHarmonyTargetPlatformSettings = (FOpenHarmonyTargetPlatformSettings*)TargetPlatformSettings;
}


FOpenHarmonyTargetPlatformControls::~FOpenHarmonyTargetPlatformControls()
{
	 FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ITargetPlatform overrides
 *****************************************************************************/

void FOpenHarmonyTargetPlatformControls::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}

ITargetDevicePtr FOpenHarmonyTargetPlatformControls::GetDefaultDevice( ) const
{
	// return the first device in the list
	if (Devices.Num() > 0)
	{
		auto Iter = Devices.CreateConstIterator();
		if (Iter)
		{
			return Iter.Value();
		}
	}

	return nullptr;
}

ITargetDevicePtr FOpenHarmonyTargetPlatformControls::GetDevice( const FTargetDeviceId& DeviceId )
{
	if (DeviceId.GetPlatformName() == PlatformName())
	{
		return Devices.FindRef(DeviceId.GetDeviceName());
	}

	return nullptr;
}


bool FOpenHarmonyTargetPlatformControls::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	OutDocumentationPath = FString("Shared/Tutorials/SettingUpOpenHarmonyTutorial");
	return true;
}

int32 FOpenHarmonyTargetPlatformControls::CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/OpenHarmony/GettingStarted");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready;
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}

/*
	// need to check license was accepted
	if (!HasLicense())
	{
		OutTutorialPath.Empty();
		CustomizedLogMessage = LOCTEXT("OpenHarmonyLicenseNotAcceptedMessageDetail", "SDK License must be accepted in the OpenHarmony project settings to deploy your app to the device.");
		bReadyToBuild |= ETargetPlatformReadyStatus::LicenseNotAccepted;
	}
*/

	return bReadyToBuild;
}

void FOpenHarmonyTargetPlatformControls::GetPlatformSpecificProjectAnalytics(TArray<FAnalyticsEventAttribute>& AnalyticsParamArray) const
{
	TTargetPlatformControlsBase<FOpenHarmonyPlatformProperties>::GetPlatformSpecificProjectAnalytics(AnalyticsParamArray);

	AppendAnalyticsEventAttributeArray(AnalyticsParamArray,
		TEXT("OpenHarmonyVariant"), GetOpenHarmonyVariantName(),
		TEXT("SupportsVulkan"), OpenHarmonyTargetPlatformSettings->SupportsVulkan(),
		TEXT("SupportsVulkanSM5"), OpenHarmonyTargetPlatformSettings->SupportsVulkanSM5(),
		TEXT("SupportsES31"), OpenHarmonyTargetPlatformSettings->SupportsES31()
	);

	AppendAnalyticsEventConfigBool(AnalyticsParamArray, TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bPackageForMetaQuest"), GEngineIni);
}

#if WITH_ENGINE

FName FOpenHarmonyTargetPlatformControls::FinalizeVirtualTextureLayerFormat(FName Format) const
{
#if WITH_EDITOR
	// Remap non-ETC variants to ETC

	// VirtualTexture Format was already run through the ordinary texture remaps to change AutoDXT to ASTC or ETC
	// this then runs again
	// currently it forces all ASTC to ETC
	// this is needed because the runtime virtual texture encoder only supports ETC

	// code dupe with IOSTargetPlatform

	const static FName VTRemap[][2] =
	{
		{ { FName(TEXT("ASTC_RGB")) },			{ OpenHarmonyTexFormat::NameETC2_RGB } },
		{ { FName(TEXT("ASTC_RGBA")) },			{ OpenHarmonyTexFormat::NameETC2_RGBA } },
		{ { FName(TEXT("ASTC_RGBA_HQ")) },		{ OpenHarmonyTexFormat::NameETC2_RGBA } },
//		{ { FName(TEXT("ASTC_RGB_HDR")) },		{ NameRGBA16F } }, // ?
		{ { FName(TEXT("ASTC_RGBAuto")) },		{ OpenHarmonyTexFormat::NameAutoETC2 } },
		{ { FName(TEXT("ASTC_NormalAG")) },		{ OpenHarmonyTexFormat::NameETC2_RGB } },
		{ { OpenHarmonyTexFormat::NameASTC_NormalRG },	{ OpenHarmonyTexFormat::NameETC2_RG11 } },
		{ { OpenHarmonyTexFormat::NameASTC_NormalLA	},	{ OpenHarmonyTexFormat::NameETC2_RG11 } },
		{ { OpenHarmonyTexFormat::NameDXT1 },		{ OpenHarmonyTexFormat::NameETC2_RGB } },
		{ { OpenHarmonyTexFormat::NameDXT5 },		{ OpenHarmonyTexFormat::NameETC2_RGBA } },
		{ { OpenHarmonyTexFormat::NameAutoDXT },	{ OpenHarmonyTexFormat::NameAutoETC2 } }
	};

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(VTRemap); RemapIndex++)
	{
		if (VTRemap[RemapIndex][0] == Format)
		{
			return VTRemap[RemapIndex][1];
		}
	}
#endif
	return Format;
}
#endif //WITH_ENGINE

bool FOpenHarmonyTargetPlatformControls::SupportsVariants() const
{
	return true;
}


/* FOpenHarmonyTargetPlatformControls implementation
 *****************************************************************************/
void FOpenHarmonyTargetPlatformControls::InitializeDeviceDetection()
{
	DeviceDetection = FModuleManager::LoadModuleChecked<IOpenHarmonyDeviceDetectionModule>("OpenHarmonyDeviceDetection").GetOpenHarmonyDeviceDetection();
	DeviceDetection->Initialize(TEXT("OPENHARMONY_HOME"),
#if PLATFORM_WINDOWS
		TEXT("base\\toolchains\\hdc.exe"),
#else
		TEXT("base/toolchains/hdc"),
#endif
		TEXT("shell param get"), true);
}

bool FOpenHarmonyTargetPlatformControls::ShouldExpandTo32Bit(const uint16* Indices, const int32 NumIndices) const
{
	bool bIsMaliBugIndex = false;
	const uint16 MaliBugIndexMaxDiff = 16;
	uint16 LastIndex = Indices[0];
	for (int32 i = 1; i < NumIndices; ++i)
	{
		uint16 CurrentIndex = Indices[i];
		if ((FMath::Abs(LastIndex - CurrentIndex) > MaliBugIndexMaxDiff))
		{
			bIsMaliBugIndex = true;
			break;
		}
		else
		{
			LastIndex = CurrentIndex;
		}
	}
	return bIsMaliBugIndex;
}

/* FOpenHarmonyTargetPlatformControls callbacks
 *****************************************************************************/

bool FOpenHarmonyTargetPlatformControls::HandleTicker( float DeltaTime )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenHarmonyTargetPlatform_HandleTicker);

	if (DeviceDetection == nullptr)
	{
		InitializeDeviceDetection();
		checkf(DeviceDetection != nullptr, TEXT("A target platform didn't create a device detection object in InitializeDeviceDetection()!"));
	}

	TArray<FString> ConnectedDeviceIds;

	{
		FScopeLock ScopeLock(DeviceDetection->GetDeviceMapLock());

		auto DeviceIt = DeviceDetection->GetDeviceMap().CreateConstIterator();

		for (; DeviceIt; ++DeviceIt)
		{
			ConnectedDeviceIds.Add(DeviceIt.Key());

			const FOpenHarmonyDeviceInfo& DeviceInfo = DeviceIt.Value();

			// see if this device is already known
			if (Devices.Contains(DeviceIt.Key()))
			{
				FOpenHarmonyTargetDevicePtr TestDevice = Devices[DeviceIt.Key()];

				// ignore if authorization didn't change
				if (DeviceInfo.bAuthorizedDevice == TestDevice->IsAuthorized())
				{
					continue;
				}

				// remove it to add again
				TestDevice->SetConnected(false);
				Devices.Remove(DeviceIt.Key());

				OnDeviceLost().Broadcast(TestDevice.ToSharedRef());
			}

			// check if this platform is supported by the extensions and version
			if (!SupportedByExtensionsString(DeviceInfo.GLESExtensions, DeviceInfo.GLESVersion))
			{
				continue;
			}

			// create target device
			FOpenHarmonyTargetDevicePtr& Device = Devices.Add(DeviceInfo.SerialNumber);

			Device = CreateTargetDevice(*this, DeviceInfo.SerialNumber, GetOpenHarmonyVariantName());

			Device->SetConnected(true);
			Device->SetModel(DeviceInfo.Model);
			Device->SetDeviceName(DeviceInfo.DeviceName);
			Device->SetAuthorized(DeviceInfo.bAuthorizedDevice);
			Device->SetVersions(DeviceInfo.SDKVersion, DeviceInfo.HumanOpenHarmonyVersion);

			ITargetPlatformControls::OnDeviceDiscovered().Broadcast(Device.ToSharedRef());
		}
	}

	// remove disconnected devices
	for (auto Iter = Devices.CreateIterator(); Iter; ++Iter)
	{
		if (!ConnectedDeviceIds.Contains(Iter.Key()))
		{
			FOpenHarmonyTargetDevicePtr RemovedDevice = Iter.Value();
			RemovedDevice->SetConnected(false);

			Iter.RemoveCurrent();

			OnDeviceLost().Broadcast(RemovedDevice.ToSharedRef());
		}
	}

	return true;
}

FOpenHarmonyTargetDeviceRef FOpenHarmonyTargetPlatformControls::CreateNewDevice(const FOpenHarmonyDeviceInfo &DeviceInfo)
{
	return MakeShareable(new FOpenHarmonyTargetDevice(*this, DeviceInfo.SerialNumber, GetOpenHarmonyVariantName()));
}

FOpenHarmonyTargetDevicePtr FOpenHarmonyTargetPlatformControls::CreateTargetDevice(const ITargetPlatformControls& InTargetPlatform, const FString& InSerialNumber, const FString& InOpenHarmonyVariant) const
{
	return MakeShareable(new FOpenHarmonyTargetDevice(InTargetPlatform, InSerialNumber, InOpenHarmonyVariant));
}

#if WITH_ENGINE
void FOpenHarmonyTargetPlatformControls::GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	// FOpenHarmonyTargetPlatformControls aside from being the base class for all the concrete openharmony target platforms
	//	it is also usable on its own as "flavorless" OpenHarmony
	//	but I don't understand how that's supposed to work or what that's supposed to mean
	//	and no information has been forthcoming
	check(Texture);

	// Supported in ES3.2 with ASTC
	const bool bSupportCompressedVolumeTexture = OpenHarmonyTargetPlatformSettings->SupportsTextureFormatCategory(EOpenHarmonyTextureFormatCategory::ASTC);
	// OpenGL ES has F32 textures but doesn't allow linear filtering unless OES_texture_float_linear
	const bool bSupportFilteredFloat32Textures = false;

	// optionaly compress landscape weightmaps for a mobile rendering
	static const auto CompressLandscapeWeightMapsVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.CompressLandscapeWeightMaps"));
	static const bool bCompressLandscapeWeightMaps = (CompressLandscapeWeightMapsVar && CompressLandscapeWeightMapsVar->GetValueOnAnyThread() != 0);

	TArray<FName>& LayerFormats = OutFormats.AddDefaulted_GetRef();
	int32 BlockSize = 1; // this looks wrong? should be 4 for FOpenHarmony_DXTTargetPlatform ? - it is wrong, but BlockSize is ignored

	GetDefaultTextureFormatNamePerLayer(LayerFormats, this->GetTargetPlatformSettings(), this, Texture, bSupportCompressedVolumeTexture, BlockSize, bSupportFilteredFloat32Textures);

	for (FName& TextureFormatName : LayerFormats)
	{
		// @todo Oodle: this should not be here
		//	should be in GetDefaultTextureFormatNamePerLayer
		//	so that 4x4 checks can be applied correctly, etc.
		if (Texture->LODGroup == TEXTUREGROUP_Terrain_Weightmap && bCompressLandscapeWeightMaps)
		{
			TextureFormatName = OpenHarmonyTexFormat::NameAutoDXT;
		}

		if (Texture->GetTextureClass() == ETextureClass::Cube)
		{
			FTextureFormatSettings FormatSettings;
			Texture->GetDefaultFormatSettings(FormatSettings);
			// TC_EncodedReflectionCapture is no longer used and could be deleted
			if (FormatSettings.CompressionSettings == TC_EncodedReflectionCapture && !FormatSettings.CompressionNone)
			{
				TextureFormatName = FName(TEXT("ETC2_RGBA"));
			}
		}

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::GenericRemap); ++RemapIndex)
		{
			if (TextureFormatName == OpenHarmonyTexFormat::GenericRemap[RemapIndex][0])
			{
				TextureFormatName = OpenHarmonyTexFormat::GenericRemap[RemapIndex][1];
			}
		}
	}
}

void FOpenHarmonyTargetPlatformControls::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	GetAllDefaultTextureFormats(this->GetTargetPlatformSettings(), OutFormats);

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::GenericRemap); ++RemapIndex)
	{
		OutFormats.Remove(OpenHarmonyTexFormat::GenericRemap[RemapIndex][0]);
	}

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::GenericRemap); ++RemapIndex)
	{
		OutFormats.AddUnique(OpenHarmonyTexFormat::GenericRemap[RemapIndex][1]);
	}
}

void FOpenHarmony_ASTCTargetPlatformControls::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	FOpenHarmonyTargetPlatformControls::GetAllTextureFormats(OutFormats);

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::ASTCRemap); ++RemapIndex)
	{
		OutFormats.Remove(OpenHarmonyTexFormat::ASTCRemap[RemapIndex][0]);
	}

	// ASTC for compressed textures
	OutFormats.Add(OpenHarmonyTexFormat::NameAutoASTC);
	// ETC for ETC2_R11
	OutFormats.Add(OpenHarmonyTexFormat::NameAutoETC2);
}

void FOpenHarmony_ASTCTargetPlatformControls::GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	FOpenHarmonyTargetPlatformControls::GetTextureFormats(Texture, OutFormats);

	// L+A mode for normal map compression
	const bool bSupportsNormalLA = GetTargetPlatformSettings()->SupportsFeature(ETargetPlatformFeatures::NormalmapLAEncodingMode);

	// perform any remapping away from defaults
	TArray<FName>& LayerFormats = OutFormats.Last();
	for (FName& TextureFormatName : LayerFormats)
	{
		if (bSupportsNormalLA && TextureFormatName == OpenHarmonyTexFormat::NameBC5)
		{
			TextureFormatName = OpenHarmonyTexFormat::NameASTC_NormalLA;
			continue;
		}

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::ASTCRemap); ++RemapIndex)
		{
			if (TextureFormatName == OpenHarmonyTexFormat::ASTCRemap[RemapIndex][0])
			{
				TextureFormatName = OpenHarmonyTexFormat::ASTCRemap[RemapIndex][1];
				break;
			}
		}
	}

	bool bSupportASTCHDR = OpenHarmonyTargetPlatformSettings->UsesASTCHDR();

	if (!bSupportASTCHDR)
	{
		for (FName& TextureFormatName : LayerFormats)
		{
			if (TextureFormatName == OpenHarmonyTexFormat::NameASTC_RGB_HDR)
			{
				TextureFormatName = OpenHarmonyTexFormat::NameRGBA16F;
			}
		}
	}
}

void FOpenHarmony_DXTTargetPlatformControls::GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	FOpenHarmonyTargetPlatformControls::GetTextureFormats(Texture, OutFormats);

	bool bSupportsDX11Formats = false; // assume OpenHarmony DXT does not support BC6/7

	if (!bSupportsDX11Formats)
	{
		TArray<FName>& LayerFormats = OutFormats.Last();

		for (FName& TextureFormatName : LayerFormats)
		{
			if (TextureFormatName == OpenHarmonyTexFormat::NameBC6H)
			{
				TextureFormatName = OpenHarmonyTexFormat::NameRGBA16F;
			}
			else if (TextureFormatName == OpenHarmonyTexFormat::NameBC7)
			{
				TextureFormatName = OpenHarmonyTexFormat::NameDXT5;
			}
		}
	}
}

void FOpenHarmony_ETC2TargetPlatformControls::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	FOpenHarmonyTargetPlatformControls::GetAllTextureFormats(OutFormats);

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::ETCRemap); ++RemapIndex)
	{
		OutFormats.Remove(OpenHarmonyTexFormat::ETCRemap[RemapIndex][0]);
	}

	// support only ETC for compressed textures
	OutFormats.Add(OpenHarmonyTexFormat::NameAutoETC2);
}

void FOpenHarmony_ETC2TargetPlatformControls::GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	FOpenHarmonyTargetPlatformControls::GetTextureFormats(Texture, OutFormats);

	// perform any remapping away from defaults
	TArray<FName>& LayerFormats = OutFormats.Last();
	for (FName& TextureFormatName : LayerFormats)
	{
		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(OpenHarmonyTexFormat::ETCRemap); ++RemapIndex)
		{
			if (TextureFormatName == OpenHarmonyTexFormat::ETCRemap[RemapIndex][0])
			{
				TextureFormatName = OpenHarmonyTexFormat::ETCRemap[RemapIndex][1];
				break;
			}
		}
	}
}

void FOpenHarmony_MultiTargetPlatformControls::GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	// Ask each platform variant to choose texture formats
	for (ITargetPlatformControls* Platform : FormatTargetPlatforms)
	{
		TArray< TArray<FName> > PlatformFormats;
		Platform->GetTextureFormats(Texture, PlatformFormats);
		for (TArray<FName>& FormatPerLayer : PlatformFormats)
		{
			// For multiformat case we have to disable L+A normal map compression as only ASTC textures support it 
			for (FName& TextureFormatName : FormatPerLayer)
			{
				if (TextureFormatName == OpenHarmonyTexFormat::NameASTC_NormalLA)
				{
					TextureFormatName = OpenHarmonyTexFormat::NameASTC_NormalRG;
				}
			}

			OutFormats.AddUnique(FormatPerLayer);
		}
	}
}

void FOpenHarmony_MultiTargetPlatformControls::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	// Ask each platform variant to choose texture formats
	for (ITargetPlatformControls* Platform : FormatTargetPlatforms)
	{
		TArray<FName> PlatformFormats;
		Platform->GetAllTextureFormats(PlatformFormats);
		for (FName Format : PlatformFormats)
		{
			OutFormats.AddUnique(Format);
		}
	}
}
#endif

FText FOpenHarmony_MultiTargetPlatformControls::DisplayName() const
{
	return FText::Format(LOCTEXT("OpenHarmony_Multi", "OpenHarmony (Multi:{0})"), FText::FromString(FormatTargetString));
}

#undef LOCTEXT_NAMESPACE
