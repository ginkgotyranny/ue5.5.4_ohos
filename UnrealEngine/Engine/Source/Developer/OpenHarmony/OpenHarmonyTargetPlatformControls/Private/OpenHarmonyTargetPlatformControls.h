#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "OpenHarmony/OpenHarmonyPlatformProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformControlsBase.h"
#include "Interfaces/IOpenHarmonyDeviceDetection.h"
#include "Interfaces/IOpenHarmonyDeviceDetectionModule.h"
#include "OpenHarmonyTargetDevice.h"
#include "OpenHarmonyTargetPlatformSettings.h"

#if WITH_ENGINE
#include "Engine/Texture.h"
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE


class FTargetDeviceId;
class IOpenHarmonyDeviceDetection;
class ITargetPlatform;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;

template< typename InElementType, typename KeyFuncs, typename Allocator > class TSet;
template<typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs > class TMap;
template<typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs > class TMultiMap;
template<typename TPlatformProperties> class TTargetPlatformBase;

/**
 * FOpenHarmonyTargetPlatformControls, abstraction for cooking OpenHarmony platforms
 */
class OPENHARMONYTARGETPLATFORMCONTROLS_API FOpenHarmonyTargetPlatformControls
	: public TNonDesktopTargetPlatformControlsBase<FOpenHarmonyPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FOpenHarmonyTargetPlatformControls(bool bInIsClient, ITargetPlatformSettings* TargetPlatformSettings, const TCHAR* FlavorName, const TCHAR* OverrideIniPlatformName = nullptr);

	/**
	 * Destructor
	 */
	virtual ~FOpenHarmonyTargetPlatformControls();

public:

	/**
	 * Gets the name of the OpenHarmony platform variant, i.e. ASTC, ETC2, DXT, etc.
	 *
	 * @param Variant name.
	 */
	FString GetOpenHarmonyVariantName() const
	{
		return PlatformInfo->PlatformFlavor.ToString();
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override
	{
		return false;
	}

	virtual void GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice() const override;

	virtual ITargetDevicePtr GetDevice(const FTargetDeviceId& DeviceId) override;

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

	virtual void GetPlatformSpecificProjectAnalytics(TArray<struct FAnalyticsEventAttribute>& AnalyticsParamArray) const override;

	virtual bool SupportsCompressedNonPOT() const
	{
		// most formats do support non-POT compressed textures
		return true;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override;
	virtual FName FinalizeVirtualTextureLayerFormat(FName Format) const override;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override;

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings");
		InBoolKeys.Add(TEXT("bBuildForArm64"));	InBoolKeys.Add(TEXT("bBuildForX8664"));
		InBoolKeys.Add(TEXT("bBuildForES31")); InBoolKeys.Add(TEXT("bBuildWithHiddenSymbolVisibility"));
		InBoolKeys.Add(TEXT("bSaveSymbols")); InStringKeys.Add(TEXT("NDKAPILevel"));
	}

	virtual bool ShouldExpandTo32Bit(const uint16* Indices, const int32 NumIndices) const override;
	//~ End ITargetPlatform Interface

	virtual void InitializeDeviceDetection();

protected:

	virtual FOpenHarmonyTargetDevicePtr CreateTargetDevice(const ITargetPlatformControls& InTargetPlatform, const FString& InSerialNumber, const FString& InOpenHarmonyVariant) const;

protected:
	/**
	 * Return true if this device has a supported set of extensions for this platform.
	 *
	 * @param Extensions - The GL extensions string.
	 * @param GLESVersion - The GLES version reported by this device.
	 */
	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const
	{
		return true;
	}

	// Handles when the ticker fires.
	bool HandleTicker(float DeltaTime);

	virtual FOpenHarmonyTargetDeviceRef CreateNewDevice(const FOpenHarmonyDeviceInfo& DeviceInfo);

	// Holds a map of valid devices.
	TMap<FString, FOpenHarmonyTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;

	// Pointer to the device detection handler that grabs device ids in another thread
	IOpenHarmonyDeviceDetection* DeviceDetection;

#if WITH_ENGINE

	ITargetDevicePtr DefaultDevice;
#endif //WITH_ENGINE

	FOpenHarmonyTargetPlatformSettings* OpenHarmonyTargetPlatformSettings;
};

class FOpenHarmony_DXTTargetPlatformControls : public FOpenHarmonyTargetPlatformControls
{
public:
	FOpenHarmony_DXTTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FOpenHarmonyTargetPlatformControls(bIsClient, TargetPlatformSettings, TEXT("DXT"))
	{
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")));
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("TextureFormatPriority_DXT"), Priority, GEngineIni) ?
			Priority : 0.6f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override;
#endif
};

class FOpenHarmony_ASTCTargetPlatformControls : public FOpenHarmonyTargetPlatformControls
{
public:
	FOpenHarmony_ASTCTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FOpenHarmonyTargetPlatformControls(bIsClient, TargetPlatformSettings, TEXT("ASTC"))
	{
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("TextureFormatPriority_ASTC"), Priority, GEngineIni) ?
			Priority : 0.9f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif
};


class FOpenHarmony_ETC2TargetPlatformControls : public FOpenHarmonyTargetPlatformControls
{
public:

	FOpenHarmony_ETC2TargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FOpenHarmonyTargetPlatformControls(bIsClient, TargetPlatformSettings, TEXT("ETC2"))
	{
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return GLESVersion >= 0x30000;
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("TextureFormatPriority_ETC2"), Priority, GEngineIni) ?
			Priority : 0.2f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif
};

class FOpenHarmony_MultiTargetPlatformControls : public FOpenHarmonyTargetPlatformControls
{
	TArray<FOpenHarmonyTargetPlatformControls*> FormatTargetPlatforms;
	FString FormatTargetString;

public:
	FOpenHarmony_MultiTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FOpenHarmonyTargetPlatformControls(bIsClient, TargetPlatformSettings, TEXT("Multi"))
	{
	}

	// set up all of the multiple formats together into this one
	void LoadFormats(TArray<FOpenHarmonyTargetPlatformControls*> SingleFormatTPs)
	{
		// sort formats by priority so higher priority formats are packaged (and thus used by the device) first
		// note that we passed this by value, not ref, so we can sort it
		SingleFormatTPs.Sort([](const FOpenHarmonyTargetPlatformControls& A, const FOpenHarmonyTargetPlatformControls& B)
			{
				float PriorityA = 0.f;
				float PriorityB = 0.f;
				FString VariantA = A.GetOpenHarmonyVariantName().Replace(TEXT("Client"), TEXT(""));
				FString VariantB = B.GetOpenHarmonyVariantName().Replace(TEXT("Client"), TEXT(""));
				GConfig->GetFloat(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantA), PriorityA, GEngineIni);
				GConfig->GetFloat(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantB), PriorityB, GEngineIni);
				return PriorityA > PriorityB;
			});

		FormatTargetPlatforms.Empty();
		FormatTargetString = TEXT("");

		TSet<FString> SeenFormats;

		// Load the TargetPlatform module for each format
		for (FOpenHarmonyTargetPlatformControls* SingleFormatTP : SingleFormatTPs)
		{
			// only use once each
			if (SeenFormats.Contains(SingleFormatTP->GetOpenHarmonyVariantName()))
			{
				continue;
			}
			SeenFormats.Add(SingleFormatTP->GetOpenHarmonyVariantName());

			bool bEnabled = false;
			FString SettingsName = FString(TEXT("bMultiTargetFormat_")) + *SingleFormatTP->GetOpenHarmonyVariantName();
			GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), *SettingsName, bEnabled, GEngineIni);
			if (bEnabled)
			{
				if (FormatTargetPlatforms.Num())
				{
					FormatTargetString += TEXT(",");
				}
				FormatTargetString += SingleFormatTP->GetOpenHarmonyVariantName();
				FormatTargetPlatforms.Add(SingleFormatTP);
			}
		}

		// @todo do we need this with DisplayName below?
		PlatformInfo::UpdatePlatformDisplayName(TEXT("OpenHarmony_Multi"), DisplayName());
	}

	virtual float GetVariantPriority() const override
	{
		// lowest priority so specific variants are chosen first
		return (IsClientOnly() ? 0.25f : 0.5f);
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif	
	virtual FText DisplayName() const override;
};
