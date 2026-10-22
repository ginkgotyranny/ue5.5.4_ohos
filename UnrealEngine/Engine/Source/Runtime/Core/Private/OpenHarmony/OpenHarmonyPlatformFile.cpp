// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmony/OpenHarmonyPlatformFile.h"

#include <cerrno>

#include "Misc/Paths.h"
#include <dirent.h>

#include "OpenHarmony/OpenHarmonyPlatformTLS.h"

#define USE_UTIME 0
#define LOG_OPENHARMONY_FILE 1
#define UNUSE_RAW_PATH 1

// Internal File Direcory Path (for application) - setup during load
FString GInternalFilePath;
// External File Direcory Path (for application) - setup during load
FString GExternalFilePath;
// Init NativeResourceManager.
NativeResourceManager* GNativeResourceManager = nullptr;

// Directory for log file on OpenHarmony
bool GOverrideOpenHarmonyLogDir = false;

FString GFilePathBase;
FString OpenHarmonyLogDir;
FString GOpenHarmonyProjectName;
// External font path base - setup during load
FString GFontPathBase;

#define FILEBASE_DIRECTORY "/UE4Game/"

napi_value Napi_nativeResourceManagerInit(napi_env Env, napi_callback_info Info)
{
	size_t Argc = 1;
	napi_value Args[1];
	NAPI_CALL(Env, napi_get_cb_info(Env, Info, &Argc, Args, nullptr, nullptr));
	GNativeResourceManager = OH_ResourceManager_InitNativeResourceManager(Env, Args[0]);

	if(GNativeResourceManager != nullptr)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyPlathformFile::Napi_nativeResourceManagerInit is ok!"));
	}
	else{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyPlathformFile::Napi_nativeResourceManagerInit is failed!"));
	}

	return nullptr;
}

napi_value Napi_nativeSetOverrideLogFileDir(napi_env Env, napi_callback_info Info)
{
	size_t Argc = 1;
	napi_value Args[1];
	NAPI_CALL(Env, napi_get_cb_info(Env, Info, &Argc, Args, nullptr, nullptr));

	size_t TypeLen = 0;
	napi_get_value_string_utf8(Env, Args[0], nullptr, 0, &TypeLen);
	char* Buffer = new char[TypeLen + 1];
	napi_get_value_string_utf8(Env, Args[0], Buffer, TypeLen + 1, &TypeLen);
	FString FileBaseDir(Buffer);
	delete[] Buffer;

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Override log dir: %s"), *FileBaseDir);

	GFilePathBase = FileBaseDir;
	GInternalFilePath = GFilePathBase;
	GExternalFilePath = GFilePathBase;
	GOverrideOpenHarmonyLogDir = true;

	if (GOverrideOpenHarmonyLogDir)
	{
		FString LogBaseDir = FileBaseDir + FString("/UE4Game/");
		mkdir(TCHAR_TO_UTF8(*LogBaseDir), 0766);
		mkdir(TCHAR_TO_UTF8(*(LogBaseDir + TEXT("/Saved"))), 0766);
		mkdir(TCHAR_TO_UTF8(*(LogBaseDir + TEXT("/Saved/Logs"))), 0766);

		OpenHarmonyLogDir = LogBaseDir + TEXT("/Saved/Logs/");
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Override log dir: %s"), *OpenHarmonyLogDir);
	}

	return nullptr;
}

napi_value Napi_nativeGetBundleInfoForSelfSync(napi_env Env, napi_callback_info Info)
{
	size_t Argc = 1;
	napi_value Args[1];
	NAPI_CALL(Env, napi_get_cb_info(Env, Info, &Argc, Args, nullptr, nullptr));

	size_t TypeLen = 0;
	napi_get_value_string_utf8(Env, Args[0], nullptr, 0, &TypeLen);
	char* BundleInfoName = new char[TypeLen + 1];
	napi_get_value_string_utf8(Env, Args[0], BundleInfoName, TypeLen + 1, &TypeLen);
	int start = TypeLen;
	//字符串截取OpenHarmonyProjectName
	while(BundleInfoName[start] != '.') {
		start--;
	}
	start++;
	//包名从BundleInfoName最后一个.之后的部分算起，进行拷贝到ProjectName
	char* ProjectName = new char[TypeLen + 2 - start];
	for (int end = start; end < TypeLen + 1; end++) {
		ProjectName[end - start] = BundleInfoName[end];
	}

	FString OpenHarmonyProjectName(ProjectName);
	GOpenHarmonyProjectName = OpenHarmonyProjectName;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("BundleInfoName is : %s"), *GOpenHarmonyProjectName);

	delete[] BundleInfoName;
	delete[] ProjectName;

	return nullptr;
}

/**
	OpenHarmony file handle implementation for partial (i.e. parcels) files.
	This can handle all the variations of accessing data for resource and files.
**/
class CORE_API FFileHandleOpenHarmony : public IFileHandle
{
	enum { READWRITE_SIZE = 1024 * 1024 };

	void LogInfo()
	{
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("FFileHandleOpenHarmony => Asset = %p, Handle = %d, Bounds = [%d,%d)"),
			File->RawFileHandle, File->Handle, int32(Start), int32(Start + Length));
#endif
	}

public:

	struct FileReference
	{
		FString Path;
		RawFile64* RawFileHandle;
		int32 Handle;

		FileReference()
			: RawFileHandle(nullptr), Handle(-1)
		{
		}

		FileReference(const FString& Path, RawFile64* Rfile)
			: Path(Path), RawFileHandle(Rfile), Handle(0)
		{
		}

		FileReference(const FString& Path, int32 Fhandle)
			: Path(Path), RawFileHandle(nullptr), Handle(Fhandle)
		{
		}

		~FileReference()
		{
			if (Handle != -1)
			{
				close(Handle);
				Handle = -1;
			}
			if (RawFileHandle != nullptr) {
				OH_ResourceManager_CloseRawFile64(RawFileHandle);
				RawFileHandle = nullptr;
			}
		}
	};

	TSharedPtr<FileReference> File;
	int64 Start;
	int64 Length;
	int64 CurrentOffset;

	FORCEINLINE void CheckValid()
	{
		check(File.IsValid() && File->Handle != -1);
	}

	// Invalid handle.
	FFileHandleOpenHarmony()
		: File(MakeShareable(new FileReference()))
		, Start(0), Length(0), CurrentOffset(0)
	{
	}

	// Handle that covers a subsegment of another handle.
	FFileHandleOpenHarmony(const FFileHandleOpenHarmony& Base,
		int64 Fstart, int64 Flength)
		: File(Base.File)
		, Start(Base.Start + Fstart), Length(Flength)
		, CurrentOffset(Base.Start + Fstart)
	{
		CheckValid();
		LogInfo();
	}

	// Handle that covers a subsegment of provided file.
	FFileHandleOpenHarmony(const FString& Path, int32 Filehandle, int64 Filestart, int64 Filelength)
		: File(MakeShareable(new FileReference(Path, Filehandle)))
		, Start(Filestart), Length(Filelength), CurrentOffset(0)
	{
		CheckValid();
		lseek(File->Handle, Filestart, SEEK_SET);
		LogInfo();
	}

	// Handle that covers the entire file content.
	FFileHandleOpenHarmony(const FString& Path, int32 Filehandle)
		: File(MakeShareable(new FileReference(Path, Filehandle)))
		, Start(0), Length(0), CurrentOffset(0)
	{
		CheckValid();
		Length = lseek(File->Handle, 0, SEEK_END);
		lseek(File->Handle, 0, SEEK_SET);
		LogInfo();
	}

	// Handle that covers the entire content of an asset.
	FFileHandleOpenHarmony(const FString& Path, RawFile64* Rfile)
		: File(MakeShareable(new FileReference(Path, Rfile)))
		, Start(0), Length(0), CurrentOffset(0)
	{
		RawFileDescriptor64 FileDescriptor;
		bool bResult = OH_ResourceManager_GetRawFileDescriptor64(Rfile, &FileDescriptor);
		if (bResult == false)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FFileHandleOpenHarmony::GetRawFileDescriptor64 failed"));
		}
		else
		{
			File->Handle = FileDescriptor.fd;
			Start = FileDescriptor.start;
			Length = FileDescriptor.length;
			CurrentOffset = Start;
		}
		CheckValid();
		LogInfo();
	}

	virtual ~FFileHandleOpenHarmony()
	{
	}

	virtual int64 Tell() override
	{
		CheckValid();
		int64 Pos = CurrentOffset;
		check(Pos != -1);
		return Pos - Start; // We are treating 'tell' as a virtual location from file Start
	}

	virtual bool Seek(int64 NewPosition) override
	{
		CheckValid();
		// we need to offset all positions by the Start offset
		NewPosition += Start;
		CurrentOffset = NewPosition;
		check(NewPosition >= 0);

		return true;
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		CheckValid();
		check(NewPositionRelativeToEnd <= 0);
		// We need to convert this to a virtual offset inside the file we are interested in
		CurrentOffset = Start + (Length - NewPositionRelativeToEnd);
		return true;
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		CheckValid();
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("(%d) FFileHandleOpenHarmony:Read => Path = %s, BytesToRead = %d"), File->Handle,
			*(File->Path), int32(BytesToRead));
#endif
		check(BytesToRead >= 0);
		check(Destination);

		while (BytesToRead > 0)
		{
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			ThisSize = pread(File->Handle, Destination, ThisSize, CurrentOffset);
#if LOG_OPENHARMONY_FILE		
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("(%d) FFileHandleOpenHarmony:Read => Path = %s, ThisSize = %d, destination = %X"), File->Handle,
				*(File->Path), int64(ThisSize), Destination);
#endif		
			if (ThisSize < 0)
			{
				return false;
			}
			else if (ThisSize == 0)
			{
				break;
			}
			CurrentOffset += ThisSize;
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}

		return BytesToRead == 0;
	}

	virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override
	{
		int64 TrueOffset = Start + Offset;
		return ReadInternal(Destination, BytesToRead, TrueOffset);
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("(%d) FFileHandleOpenHarmony:Write => Path = %s, BytesToWrite = %d"), File->Handle,
			*(File->Path), int32(BytesToWrite));
#endif
		CheckValid();
		if (nullptr != File->RawFileHandle)
		{
			// Can't write to assets.
			return false;
		}

		bool bSuccess = true;
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			if (pwrite(File->Handle, Source, ThisSize, CurrentOffset) != ThisSize)
			{
				bSuccess = false;
				break;
			}
			CurrentOffset += ThisSize;
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}

		// Update the cached file length
		Length = FMath::Max(Length, CurrentOffset);

		return bSuccess;
	}

	virtual bool Flush(const bool bFullFlush = false) override
	{
		CheckValid();
		if (nullptr != File->RawFileHandle)
		{
			// Can't write to assets.
			return false;
		}

		return bFullFlush
			? fsync(File->Handle) == 0
			: fdatasync(File->Handle) == 0;
	}

	virtual bool Truncate(int64 NewSize) override
	{
		CheckValid();
		if (nullptr != File->RawFileHandle)
		{
			// Can't write to assets.
			return false;
		}

		int Result = 0;
		do { Result = ftruncate(File->Handle, NewSize); } while (Result < 0 && errno == EINTR);
		return Result == 0;
	}

	virtual int64 Size() override
	{
		return Length;
	}
private:
	bool ReadInternal(uint8* Destination, int64 BytesToRead, int64 Offset)
	{
		CheckValid();
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("(%d/%d) FFileHandleOpenHarmony:Read => Path = %s, BytesToRead = %d"),
			FOpenHarmonyTLS::GetCurrentThreadId(), File->Handle,
			*(File->Path), int32(BytesToRead));
#endif
		if (BytesToRead < 0 || Offset - Start < 0 || (BytesToRead + Offset - Start) > Size())
		{
			return false;
		}

		if (BytesToRead == 0)
		{
			return true;
		}

		check(Destination);

		while (BytesToRead > 0)
		{
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);

			ThisSize = pread(File->Handle, Destination, ThisSize, Offset);
#if LOG_OPENHARMONY_FILE
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("(%d/%d) FFileHandleOpenHarmony:Read => Path = %s, ThisSize = %d, destination = %X"),
				FOpenHarmonyTLS::GetCurrentThreadId(), File->Handle,
				*(File->Path), int32(ThisSize), Destination);
#endif
			if (ThisSize < 0)
			{
				return false;
			}
			else if (ThisSize == 0)
			{
				break;
			}
			Offset += ThisSize;
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}

		return BytesToRead == 0;
	}
};

/**
 * OpenHarmony File I/O implementation
**/
class CORE_API FOpenHarmonyPlatformFile : public IOpenHarmonyPlatformFile
{
protected:
	FString NormalizePath(const TCHAR* Path);
	void PathToOpenHarmonyPaths(FString& LocalPath, FString& AssetPath, const TCHAR* Path, bool AllowLocal);
	bool IsLocal(const FString& LocalPath);
	virtual bool IsAsset(const FString& AssetPath) override;
	FDateTime GetLastModifiedTimeStamp(const FString& LocalPath);

public:
#if USE_OPENHARMONY_NAPI
	virtual NativeResourceManager* GetAssetManager() override
	{
		return GNativeResourceManager;
	}
#endif
	//~ For visibility of overloads we don't override
	using IPhysicalPlatformFile::IterateDirectory;
	using IPhysicalPlatformFile::IterateDirectoryStat;

	static const FString* GetOverrideLogDirectory();
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;

	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual void SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime) override;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;

public:
	static FOpenHarmonyPlatformFile& GetPlatformPhysical();

	// Offset within file or asset where its data starts.
	// Note, offsets for assets is relative to complete APK file
	// and matches what is returned by AssetFileDescriptor.getStartOffset().
	virtual int64 FileStartOffset(const TCHAR* Filename) override;

	// Get the root, i.e. underlying, path for the file. This
	// can be any of: a resolved file path, an OBB path, an
	// asset path.
	virtual FString FileRootPath(const TCHAR* Filename) override;

protected:
	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(const FString&, struct dirent*)>& Visitor, const TFunctionRef<bool(const char*)>& AssetVisitor, bool AllowLocal, bool AllowAsset);

	/** We're logging an error message. */
	bool bLoggingError = false;
};

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime OpenHarmonyEpoch(1970, 1, 1);

// Constructs the base path for any files which are not in pak data
const FString& GetFileBasePath()
{
	static FString BasePath = GFilePathBase + FString(FILEBASE_DIRECTORY) + FApp::GetProjectName() + FString("/");
	return BasePath;
}

namespace
{
	FFileStatData OpenHarmonyStatToUEFileData(struct stat& FileInfo)
	{
		const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = FileInfo.st_size;
		}

		return FFileStatData(
			OpenHarmonyEpoch + FTimespan::FromSeconds(FileInfo.st_ctime),
			OpenHarmonyEpoch + FTimespan::FromSeconds(FileInfo.st_atime),
			OpenHarmonyEpoch + FTimespan::FromSeconds(FileInfo.st_mtime),
			FileSize,
			bIsDirectory,
			!(FileInfo.st_mode & S_IWUSR)
		);
	}
}


const FString* FOpenHarmonyPlatformFile::GetOverrideLogDirectory()
{
	return GOverrideOpenHarmonyLogDir ? &OpenHarmonyLogDir : nullptr;
}

bool FOpenHarmonyPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::Initialize(..)"));
#endif
	if (!IPhysicalPlatformFile::Initialize(Inner, CmdLine))
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::Initialize failed"));
		return false;
	}
	return true;
}

bool FOpenHarmonyPlatformFile::FileExists(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileExists('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	bool bResult = false;
	struct stat FileInfo;
	if (!LocalPath.IsEmpty() && (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == 0))
	{
		// For local files we need to check if it's a plain
		// file, as opposed to directories.
		bResult = S_ISREG(FileInfo.st_mode);
	}
	else
	{
		// For everything else we only check existence.
		bResult = IsAsset(AssetPath);
	}
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileExists('%s') => %s\nResolved as %s"), Filename, bResult ? TEXT("TRUE") : TEXT("FALSE"), *LocalPath);
#endif

	return bResult;
}

int64 FOpenHarmonyPlatformFile::FileSize(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileSize('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	struct stat FileInfo;
	FileInfo.st_size = -1;
	if (!LocalPath.IsEmpty() && (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == 0))
	{
		// make sure to return -1 for directories
		if (S_ISDIR(FileInfo.st_mode))
		{
			FileInfo.st_size = -1;
		}
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileSize('%s') is local size is %d"),
			Filename, FileInfo.st_size);
#endif
	}
	else
	{
		RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		if (nullptr != Rfile)
		{
			FileInfo.st_size = OH_ResourceManager_GetRawFileSize64(Rfile);
			OH_ResourceManager_CloseRawFile64(Rfile);
			Rfile = nullptr;
		}
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileSize('%s') is rawfile size is %d"),
			Filename, FileInfo.st_size);
#endif
	}
	return FileInfo.st_size;
}

bool FOpenHarmonyPlatformFile::DeleteFile(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DeleteFile('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	// Only delete if we have a local file.
	if (IsLocal(LocalPath))
	{
		return unlink(TCHAR_TO_UTF8(*LocalPath)) == 0;
	}
	return false;
}

bool FOpenHarmonyPlatformFile::IsReadOnly(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::IsReadOnly('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	if (IsLocal(LocalPath))
	{
		if (access(TCHAR_TO_UTF8(*LocalPath), W_OK) == -1)
		{
			return errno == EACCES;
		}
	}
	else
	{
		// Anything other than local files are from read-only sources.
		return IsAsset(AssetPath);
	}
	return false;
}

bool FOpenHarmonyPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::MoveFile('%s', '%s')"), To, From);
#endif
	// Can only move local files.
	FString ToLocalPath;
	FString ToAssetPath;
	PathToOpenHarmonyPaths(ToLocalPath, ToAssetPath, To, false);
	FString FromLocalPath;
	FString FromAssetPath;
	PathToOpenHarmonyPaths(FromLocalPath, FromAssetPath, From, false);

	if (IsLocal(FromLocalPath))
	{
		return rename(TCHAR_TO_UTF8(*FromLocalPath), TCHAR_TO_UTF8(*ToLocalPath)) != -1;
	}
	return false;
}

bool FOpenHarmonyPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::SetReadOnly('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	if (IsLocal(LocalPath))
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
		{
			if (bNewReadOnlyValue)
			{
				FileInfo.st_mode &= ~S_IWUSR;
			}
			else
			{
				FileInfo.st_mode |= S_IWUSR;
			}
			return chmod(TCHAR_TO_UTF8(*LocalPath), FileInfo.st_mode) == 0;
		}
	}
	return false;
}

FDateTime FOpenHarmonyPlatformFile::GetLastModifiedTimeStamp(const FString& LocalPath)
{
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
	{
		return FDateTime::MinValue();
	}
	// convert _stat time to FDateTime
	FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
	return OpenHarmonyEpoch + TimeSinceEpoch;
}

FDateTime FOpenHarmonyPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::GetTimeStamp('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	if (IsLocal(LocalPath))
	{
#if USE_UTIME
		return GetLastModifiedTimeStamp(LocalPath);
#else
		// pak file outside of obb may not be in manifest so check if it exists
		if (LocalPath.EndsWith(".hap"))
		{
			// return local file access timestamp (if exists)
			return GetAccessTimeStamp(Filename);
		}
		else if (LocalPath.EndsWith(".log"))
		{
			return GetLastModifiedTimeStamp(LocalPath);
		}

		return FDateTime::MinValue();
#endif
	}
	else
	{
		// No TimeStamp for assets, so just return a default timespan for now.
		return FDateTime::MinValue();
	}
}

void FOpenHarmonyPlatformFile::SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::SetTimeStamp('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	// Can only set time stamp on local files
	if (IsLocal(LocalPath))
	{
#if USE_UTIME
		// get file times
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
		{
			return;
		}
		// change the modification time only
		struct utimbuf Times;
		Times.actime = FileInfo.st_atime;
		Times.modtime = (DateTime - OpenHarmonyEpoch).GetTotalSeconds();
		utime(TCHAR_TO_UTF8(*LocalPath), &Times);
#endif
	}
}

FDateTime FOpenHarmonyPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::GetAccessTimeStamp('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, true);

	if (IsLocal(LocalPath))
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
		{
			return FDateTime::MinValue();
		}
		// convert _stat time to FDateTime
		FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
		return OpenHarmonyEpoch + TimeSinceEpoch;
	}
	else
	{
		// No TimeStamp for resources nor assets, so just return a default timespan for now.
		return FDateTime::MinValue();
	}
}

FFileStatData FOpenHarmonyPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::GetStatData('%s')"), FilenameOrDirectory);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, FilenameOrDirectory, false);

	if (IsLocal(LocalPath))
	{
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
		{
			return OpenHarmonyStatToUEFileData(FileInfo);
		}
	}
	else
	{
		RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		if (nullptr != Rfile)
		{
			bool bIsDirectory = false;
			RawDir* Subdir = OH_ResourceManager_OpenRawDir(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
			if (nullptr != Subdir)
			{
				bIsDirectory = true;
				OH_ResourceManager_CloseRawDir(Subdir);
			}

			int64 FileSize = -1;
			if (!bIsDirectory)
			{
				FileSize = OH_ResourceManager_GetRawFileSize64(Rfile);
			}

			FFileStatData StatData(
				FDateTime::MinValue(),				// CreationTime
				FDateTime::MinValue(),				// AccessTime
				FDateTime::MinValue(),				// ModificationTime
				FileSize,							// FileSize
				bIsDirectory,						// bIsDirectory
				true								// bIsReadOnly
			);

			OH_ResourceManager_CloseRawFile64(Rfile);

			return StatData;
		}
	}

	return FFileStatData();
}

FString FOpenHarmonyPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	return Filename;
}

IFileHandle* FOpenHarmonyPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::OpenRead('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	if (IsLocal(LocalPath))
	{
		int32 Handle = open(TCHAR_TO_UTF8(*LocalPath), O_RDONLY);
		if (Handle != -1)
		{
			return new FFileHandleOpenHarmony(LocalPath, Handle);
		}
	}
	else
	{
		RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		if (nullptr != Rfile)
		{
			return new FFileHandleOpenHarmony(AssetPath, Rfile);
		}
	}
	return nullptr;
}

IFileHandle* FOpenHarmonyPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::OpenWrite('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, false);

	int Flags = O_CREAT;
	if (!bAppend)
	{
		Flags |= O_TRUNC;
	}
	if (bAllowRead)
	{
		Flags |= O_RDWR;
	}
	else
	{
		Flags |= O_WRONLY;
	}

	int32 Handle = open(TCHAR_TO_UTF8(*LocalPath), Flags, S_IRUSR | S_IWUSR);
	if (Handle != -1)
	{
		FFileHandleOpenHarmony* FileHandleOpenHarmony = new FFileHandleOpenHarmony(LocalPath, Handle);
		if (bAppend)
		{
			FileHandleOpenHarmony->SeekFromEnd(0);
		}
		return FileHandleOpenHarmony;
	}
	return nullptr;
}

bool FOpenHarmonyPlatformFile::DirectoryExists(const TCHAR* Directory)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DirectoryExists('%s')"), Directory);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Directory, false);

	bool Found = false;
	if (IsLocal(LocalPath))
	{
#if LOG_OPENHARMONY_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DirectoryExists('%s') => Check IsLocal: '%s'"), Directory, *(LocalPath + "/"));
#endif
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
		{
			Found = S_ISDIR(FileInfo.st_mode);
		}
	}
	else
	{
		// This can be very slow on some devices so only do it if we enabled
		RawDir* Dir = OH_ResourceManager_OpenRawDir(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		Found = OH_ResourceManager_GetRawFileCount(Dir) > 0 ? true : false;
		OH_ResourceManager_CloseRawDir(Dir);
#if LOG_OPENHARMONY_FILE
		if (Found)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DirectoryExists('%s') => Found as asset: '%s'"), Directory, *(AssetPath));
		}
#endif
	}
#if LOG_OPENHARMONY_FILE
	if (Found)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DirectoryExists('%s') => FOUND"), Directory);
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DirectoryExists('%s') => NOT"), Directory);
	}
#endif
	return Found;
}

bool FOpenHarmonyPlatformFile::CreateDirectory(const TCHAR* Directory)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::CreateDirectory('%s')"), Directory);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Directory, false);

	return (mkdir(TCHAR_TO_UTF8(*LocalPath), 0755) == 0) || (errno == EEXIST);
}

bool FOpenHarmonyPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::DeleteDirectory('%s')"), Directory);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Directory, false);

	return rmdir(TCHAR_TO_UTF8(*LocalPath));
}

bool FOpenHarmonyPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[hxp] FOpenHarmonyPlatformFile::IterateDirectory('%s')"), Directory);
	const FString DirectoryStr = Directory;

	auto InternalVisitor = [&](const FString& InLocalPath, struct dirent* InEntry) -> bool
	{
		const FString DirPath = DirectoryStr / UTF8_TO_TCHAR(InEntry->d_name);
		return Visitor.Visit(*DirPath, InEntry->d_type == DT_DIR);
	};

	auto InternalAssetVisitor = [&](const char* InAssetPath) -> bool
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[hxp] FOpenHarmonyPlatformFile::IterateDirectory::InternalAssetVisitor('%s')"), UTF8_TO_TCHAR(InAssetPath));
		bool bIsDirectory = false;
		RawDir* Dir = OH_ResourceManager_OpenRawDir(GNativeResourceManager, InAssetPath);
		if (nullptr != Dir && OH_ResourceManager_GetRawFileCount(Dir) > 0)
		{
			bIsDirectory = true;
			OH_ResourceManager_CloseRawDir(Dir);
		}

		return Visitor.Visit(UTF8_TO_TCHAR(InAssetPath), bIsDirectory);
	};

	return IterateDirectoryCommon(Directory, InternalVisitor, InternalAssetVisitor, false, true);
}

bool FOpenHarmonyPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[hxp] FOpenHarmonyPlatformFile::IterateDirectoryStat ('Directory is %s')"), Directory);
	const FString DirectoryStr = Directory;

	auto InternalVisitor = [&](const FString& InLocalPath, struct dirent* InEntry) -> bool
	{
		const FString DirPath = DirectoryStr / UTF8_TO_TCHAR(InEntry->d_name);

		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*(InLocalPath / UTF8_TO_TCHAR(InEntry->d_name))), &FileInfo) != -1)
		{
			return Visitor.Visit(*DirPath, OpenHarmonyStatToUEFileData(FileInfo));
		}

		return true;
	};

	auto InternalAssetVisitor = [&](const char* InAssetPath) -> bool
	{
		bool bIsDirectory = false;
		RawDir* Subdir = OH_ResourceManager_OpenRawDir(GNativeResourceManager, InAssetPath);
		if (nullptr != Subdir)
		{
			bIsDirectory = true;
			OH_ResourceManager_CloseRawDir(Subdir);
		}

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, InAssetPath);
			FileSize = OH_ResourceManager_GetRawFileSize64(Rfile);
			OH_ResourceManager_CloseRawDir(Subdir);
		}

		return Visitor.Visit(
			UTF8_TO_TCHAR(InAssetPath),
			FFileStatData(
				FDateTime::MinValue(),	// CreationTime
				FDateTime::MinValue(),	// AccessTime
				FDateTime::MinValue(),	// ModificationTime
				FileSize,				// FileSize
				bIsDirectory,			// bIsDirectory
				true					// bIsReadOnly
			)
		);
	};

	return IterateDirectoryCommon(Directory, InternalVisitor, InternalAssetVisitor, false, false);
}

FOpenHarmonyPlatformFile& FOpenHarmonyPlatformFile::GetPlatformPhysical()
{
	static FOpenHarmonyPlatformFile OpenHarmonyPlatformSingleton;
	return OpenHarmonyPlatformSingleton;
}

int64 FOpenHarmonyPlatformFile::FileStartOffset(const TCHAR* Filename)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileStartOffset is asset('%s')"), Filename);
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileStartOffset('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, true);

	if (IsLocal(LocalPath))
	{
		return 0;
	}
	else if (IsAsset(AssetPath))
	{
		RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		if (nullptr != Rfile)
		{
			off_t start = -1;
			RawFileDescriptor64 FileDescriptor;
			bool bResult = OH_ResourceManager_GetRawFileDescriptor64(Rfile, &FileDescriptor);
			if (bResult == true)
			{
				start = FileDescriptor.start;
				OH_ResourceManager_CloseRawFile64(Rfile);
			}
			return start;
		}
	}
	return -1;
}

FString FOpenHarmonyPlatformFile::FileRootPath(const TCHAR* Filename)
{
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::FileRootPath('%s')"), Filename);
#endif
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Filename, true);

	if (IsLocal(LocalPath))
	{
		return LocalPath;
	}
	else if (IsAsset(AssetPath))
	{
		return AssetPath;
	}
	return "";
}


bool FOpenHarmonyPlatformFile::IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(const FString&, struct dirent*)>& Visitor, const TFunctionRef<bool(const char*)>& AssetVisitor, bool AllowLocal, bool AllowAsset)
{
	FString LocalPath;
	FString AssetPath;
	PathToOpenHarmonyPaths(LocalPath, AssetPath, Directory, AllowLocal);

	if (IsLocal(LocalPath))
	{
		DIR* Handle = opendir(TCHAR_TO_UTF8(*LocalPath));
		if (Handle)
		{
			bool bResult = true;
			struct dirent* Entry;
			while ((Entry = readdir(Handle)) != nullptr && bResult == true)
			{
				if (FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT("..")))
				{
					bResult = Visitor(LocalPath, Entry);
				}
			}
			closedir(Handle);
			return bResult;
		}
	}
	else if (AllowAsset)
	{
		RawDir* Dir = OH_ResourceManager_OpenRawDir(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
		if (nullptr != Dir)
		{
			bool bResult = true;
			const char* FileName = nullptr;
			int FileCount = OH_ResourceManager_GetRawFileCount(Dir);
			for (int Index = 0; Index < FileCount; ++Index)
			{
				FileName = OH_ResourceManager_GetRawFileName(Dir, Index);
				FString FilePath = AssetPath + FileName;

				bResult = AssetVisitor(TCHAR_TO_UTF8(*FilePath));
				if (bResult == false)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::IterateDirectoryCommon, ASSET Visit failed!"));
					break;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::IterateDirectoryCommon, ASSET Visit is ok!"));
				}
			}
			OH_ResourceManager_CloseRawDir(Dir);
			return bResult;
		}
		else
		{
#if LOG_OPENHARMONY_FILE
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::IterateDirectoryCommon('%s'), ASSET Visit, dir is null"), *AssetPath);
#endif
		}
	}
	return false;
}

bool FOpenHarmonyPlatformFile::IsLocal(const FString& LocalPath)
{
#if UNUSE_RAW_PATH
	return !LocalPath.IsEmpty() && access(TCHAR_TO_UTF8(*LocalPath), F_OK) == 0;
#else
	return false;
#endif
}

bool FOpenHarmonyPlatformFile::IsAsset(const FString& AssetPath)
{
	RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(*AssetPath));
	if (nullptr != Rfile)
	{
		OH_ResourceManager_CloseRawFile64(Rfile);
		Rfile = nullptr;
		return true;
	}
	return false;
}

FString FOpenHarmonyPlatformFile::NormalizePath(const TCHAR* Path)
{
	FString Result(Path);
	Result.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	// This replacement addresses a "bug" where some callers
	// pass in paths that are badly composed with multiple
	// subdir separators.
	Result.ReplaceInline(TEXT("//"), TEXT("/"), ESearchCase::CaseSensitive);
	// Remove redundant current-dir references.
	Result.ReplaceInline(TEXT("/./"), TEXT("/"), ESearchCase::CaseSensitive);
	return Result;
}

void FOpenHarmonyPlatformFile::PathToOpenHarmonyPaths(FString& LocalPath, FString& AssetPath, const TCHAR* Path, bool AllowLocal)
{
	LocalPath.Empty();
	AssetPath.Empty();

#if UNUSE_RAW_PATH
	FString OpenHarmonyPath = NormalizePath(Path);
#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::PathToOpenHarmonyPaths('%s') => OpenHarmonyPath = '%s'"), Path, *OpenHarmonyPath);
#endif

	if (!OpenHarmonyPath.IsEmpty())
	{
		if (OpenHarmonyPath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive) ||	
			OpenHarmonyPath.StartsWith(GFontPathBase) || 
			OpenHarmonyPath.StartsWith(TEXT("/system/etc/")) ||
			OpenHarmonyPath.StartsWith(GInternalFilePath.Left(OpenHarmonyPath.Len())) ||
			OpenHarmonyPath.StartsWith(GExternalFilePath.Left(OpenHarmonyPath.Len())))
		{
			// Absolute paths are only local.
			LocalPath = OpenHarmonyPath;
			AssetPath = OpenHarmonyPath;
		}
		else
		{
			while (OpenHarmonyPath.StartsWith(TEXT("../"), ESearchCase::CaseSensitive))
			{
				OpenHarmonyPath.RightChopInline(3, false);
			}
			OpenHarmonyPath.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
			if (OpenHarmonyPath.Equals(TEXT(".."), ESearchCase::CaseSensitive))
			{
				OpenHarmonyPath = TEXT("");
			}

			// Local filepaths are directly in the deployment directory.
			static FString BasePath = GetFileBasePath();
			LocalPath = BasePath + OpenHarmonyPath;
			                                                                                                                                                                                                                          
			// Asset paths are relative to the base directory.
			AssetPath = OpenHarmonyPath;
		}
	}

#if LOG_OPENHARMONY_FILE
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::PathToOpenHarmonyPaths('%s') => LocalPath = '%s'"), Path, *LocalPath);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyPlatformFile::PathToOpenHarmonyPaths('%s') => AssetPath = '%s'"), Path, *AssetPath);
#endif
#endif
}

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	return FOpenHarmonyPlatformFile::GetPlatformPhysical();
}

IOpenHarmonyPlatformFile& IOpenHarmonyPlatformFile::GetPlatformPhysical()
{
	return FOpenHarmonyPlatformFile::GetPlatformPhysical();
}

const FString* IOpenHarmonyPlatformFile::GetOverrideLogDirectory()
{
	return FOpenHarmonyPlatformFile::GetOverrideLogDirectory();
}


