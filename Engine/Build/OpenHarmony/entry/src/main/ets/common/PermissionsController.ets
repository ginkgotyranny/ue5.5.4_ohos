import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { GlobalContext, GlobalContextConstants } from './GlobalConstants';
import EtsBundleManager from './EtsBundleManager';

//UI 线程
type ReqCallbackFunc = (arg1: string, arg2: string) => void;
type SettingCallbackFunc = (arg1: string) => void;

export default class PermissionsController {
	//请求权限
	static reqPermissionsFromUser(permissionStrings: Array<string>, callback: ReqCallbackFunc) {
		let permissions: Array<Permissions> = permissionStrings.map((v) => v as Permissions)
		let context: common.UIAbilityContext = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT);
		let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
		atManager.requestPermissionsFromUser(context, permissions).then((data) => {
			if (data.dialogShownResults) {
			callback(data.authResults.map((v) => v == 0 ? 0 : 1).join(""), 
				data.dialogShownResults.map((v) => v ? 0 : 1).join(""))	//0表示成功
			} else {
				callback(data.authResults.map((v) => v == 0 ? 0 : 1).join(""),
					data.permissions.map(() => 0).join(""))
			}
			// 授权成功
		}).catch((err: BusinessError) => {
			console.error(`Failed to request permissions from user. Code is ${err.code}, message is ${err.message}`);
			callback(permissions.map(() => 0).join(""), permissions.map(() => 0).join(""))
		})
	}
	static checkPermissionsFromUser(permissionString: string): boolean {
		let permissions: Permissions = permissionString as Permissions
		let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
		let grantStatus: abilityAccessCtrl.GrantStatus =
			atManager.checkAccessTokenSync(EtsBundleManager.GetAppTokenId(), permissions);
		return grantStatus == abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED;
	}
	
	// The permission setting dialog box is displayed.
	static openPermissionsSetting(permissionStrings: Array<string>, callback: SettingCallbackFunc): void {
		let permissions: Array<Permissions> = permissionStrings.map((v) => v as Permissions)
		let context: common.UIAbilityContext = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT);
		let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
		atManager.requestPermissionOnSetting(context, permissions).then((data: Array<abilityAccessCtrl.GrantStatus>) => {
			callback(data.map((v) => v == abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED ? 0 : 1).join(""))	//0表示成功
		}).catch((err: BusinessError) => {
			console.error(`Failed to open permission setting dialog. Code is ${err.code}, message is ${err.message}`);
			callback(permissions.map(() => 0).join(""))
		});
	}
}