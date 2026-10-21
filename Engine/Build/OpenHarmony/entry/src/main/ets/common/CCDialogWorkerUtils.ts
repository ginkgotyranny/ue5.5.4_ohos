export default class CCDialogUtils {
	static MODULE_NAME : string = 'Dialog';
	static parentPort;
	static init(parentPort) {
		CCDialogUtils.parentPort = parentPort;
	}
	
	// static showTextInputDialogPostToMainThread(type: number, msg: string) {
	// 	CCDialogUtils.parentPort.postMessage({'module': CCDialogUtils.MODULE_NAME, 'function': 'showTextInputDialogPostToMainThread', 'type': type, 'msg': msg });
	// }

	// static hideTextInputDialogPostToMainThread() {
	// 	CCDialogUtils.parentPort.postMessage({'module': CCDialogUtils.MODULE_NAME, 'function': 'hideTextInputDialogPostToMainThread' });
	// }
}
globalThis.ccDialogUtils={};
// globalThis.ccDialogUtils.showTextInputDialog = CCDialogUtils.showTextInputDialogPostToMainThread;
// globalThis.ccDialogUtils.hideTextInputDialog = CCDialogUtils.hideTextInputDialogPostToMainThread;