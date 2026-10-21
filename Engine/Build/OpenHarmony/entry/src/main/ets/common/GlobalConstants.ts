import emitter from '@ohos.events.emitter';
export class GlobalContext {
	public static loadGlobalThis(name: string) {
		return globalThis[name]
	}

	public static storeGlobalThis(name: string, obj: Object) {
		globalThis[name] = obj
	}
}


export class GlobalContextConstants {
	static readonly UE_DIALOG_CONTROLLER = "dialogController";
	static readonly UE_SHOW_MESSAGE_AND_TYPE = "showMessageANDType";
	static readonly UE_UN_SHOW_MESSAGE_AND_TYPE = "unShowMessageANDType"
	static readonly UE_MESSAGE_BOX_DIALOG_ENTITY = "MessageBoxDialogEntity";
	static readonly UE_MESSAGE_BOX_ENTITY = "MessageBoxEntity";
	static readonly UE_MESSAGE_AND_CALLITEMENTITY = "MessageANDCallItemEntity";
	static readonly UE_CONSOLE_WINDOW_DIALOG_ENTITY = "ConsoleWindowDialogEntity";
	static readonly UE_WEB_VIEW_ARRAY = "webViewArray";
	static readonly UE_WEB_VIEW_INDEX_MAP = "webViewIndexMap";
	static readonly UE_MAIN_PAGE_URL = "pages/Index";
	static readonly UE_GLOBAL_THIS = "globalThis";
	static readonly UE_ABILITY_CONTEXT = "abilityContext";
	static readonly UE_RESUME_SENSOR = "resumeSensor";
	static readonly UE_PAUSE_SENSOR = "pauseSensor";

	//event
	static readonly UE_SPLASH_EVENT : emitter.InnerEvent = { eventId: 1 };
}