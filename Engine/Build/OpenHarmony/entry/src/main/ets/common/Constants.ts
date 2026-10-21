import { Permissions } from '@ohos.abilityAccessCtrl';

export enum ContextType {
	APP_LIFECYCLE = 0,
	JSPAGE_LIFECYCLE,
	RAW_FILE_UTILS,
	WORKER_INIT,
	NATIVE_RENDER_API,
	INPUT_NAPI,
	MISC
}

export class Constants {
	static readonly APP_KEY_WORKER_MANAGER = "app_key_worker_manager";
	static readonly REQUEST_PERMISSIONS: Array<Permissions> = ['ohos.permission.LOCATION',
		'ohos.permission.APPROXIMATELY_LOCATION',
		'ohos.permission.LOCATION_IN_BACKGROUND'];
}

export class AppPermissionConsts {
	static readonly REQUEST_CODE_REQUIRED: number = 1000;

	static readonly REQUEST_CODE_CUSTOM: number = 1001;
}