export function handleDialogMsg(eventData: any) {
	switch (eventData.function) {
		case "showTextInputDialogPostToMainThread": {
			globalThis.showMessageANDType["type"] = eventData.type;
			globalThis.showMessageANDType["message"] = eventData.msg;
			globalThis.dialogController.open();
			break;
		}
		case "hideTextInputDialogPostToMainThread": {
			globalThis.showMessageANDType["message"] = '';
			globalThis.dialogController.close();
			break;
		}
	}
}