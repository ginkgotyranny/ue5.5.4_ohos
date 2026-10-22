import { bundleManager } from '@kit.AbilityKit'
import { BusinessError } from '@ohos.base';
//从MetaData中读取数据
export default class EtsBundleManager {
	static bundleFlags: number =
		bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION |
        bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_METADATA;
	
	static  GetMetaDataString(key: string): string {
		let res = EtsBundleManager.GetMetaDataObject(key);
		if (res == undefined) {
			return "";
		} else {
			return res.value;
		}
	}

	static ContainKey(key: string): boolean {
		return EtsBundleManager.GetMetaDataObject(key) != undefined;
	}

	static GetMetaDataObject(key: string) {
		return bundleManager.getBundleInfoForSelfSync(EtsBundleManager.bundleFlags).appInfo.metadataArray
			.find(obj => obj.moduleName == "entry")?.metadata.find(obj => obj.name == key)
	}

	static tokenId: number = -1;
	static GetAppTokenId(): number {
		// 获取应用程序的accessTokenID
		if (EtsBundleManager.tokenId != -1) {
			return EtsBundleManager.tokenId;
		}
		try {
			let bundleInfo: bundleManager.BundleInfo =
				bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
			let appInfo: bundleManager.ApplicationInfo = bundleInfo.appInfo;
			EtsBundleManager.tokenId = appInfo.accessTokenId;
		} catch (error) {
			const err: BusinessError = error as BusinessError;
			console.error(`Failed to get bundle info for self. Code is ${err.code}, message is ${err.message}`);
		}
		return EtsBundleManager.tokenId;
	}
}