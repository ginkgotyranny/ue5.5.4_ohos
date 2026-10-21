import hilog from "@ohos.hilog"
import display from "@ohos.display";
import window from '@ohos.window';
import { BusinessError } from '@kit.BasicServicesKit';
import { GlobalContext, GlobalContextConstants } from '../common/GlobalConstants';
import nativeRender from "libNativeGlue.so";

export class Display {
	static GetRefreshRate(): number {
		let displayObj = null;
		try {
			displayObj = display.getDefaultDisplaySync();
			let rate = displayObj.refreshRate;
			hilog.info(0x0000, 'ohos display', 'refreshRate is %{public}d', rate);
			return rate;
		} catch (exception) {
			console.error('Failed to obtain the default display. Code: ' + JSON.stringify(exception));
		}
		return 0;
	}

	static registerDisplayChangeEvent() {
		// 鸿蒙对粘性事件支持暂不完善，先调一下同步接口获取初始值。
		let orientation = display.getDefaultDisplaySync().orientation.valueOf();
		nativeRender.Napi_nativeSetDeviceOrientation(orientation);

		display.on("change", (deviceId) => {
			display.getAllDisplays((err, displays) => {
				displays.filter((display) => display.id == deviceId).forEach((selectedDisplay) => {
					nativeRender.Napi_nativeSetDeviceOrientation(selectedDisplay.orientation.valueOf());
				})
			});
		});
	}
	
	static GetDevicePhysicalSizeInInch(): number {
		let displayObj = null;
		try {
			displayObj = display.getDefaultDisplaySync();
			let x = Math.pow(displayObj.width / displayObj.xDPI, 2);
			let y = Math.pow(displayObj.height / displayObj.yDPI, 2);
			let screenInches = Math.sqrt(x + y);
			hilog.info(0x0000, 'ohos display', 'screenInches is %{public}d', screenInches);
			return screenInches;
		} catch (exception) {
			console.error('Failed to obtain the default display. Code: ' + JSON.stringify(exception));
		}
		return 0;
	}

	static SetOrientation(value: number) {
		let context = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT);
		let windowClass: window.Window | undefined = undefined;
		try {
			window.getLastWindow(context, (err: BusinessError, data) => {
				const errCode: number = err.code;
				if (errCode) {
					console.error(`Failed to obtain the top window. Cause code: ${err.code}, message: ${err.message}`);
					return;
				}
				windowClass = data;
				let orientation: window.Orientation = value;
				try {
					let promise = windowClass.setPreferredOrientation(orientation);
					promise.then(() => {
						console.info(`Succeeded in setting the window orientation to ${orientation}.`);
					}).catch((err: BusinessError) => {
						console.error(`Failed to set the window orientation. Cause code: ${err.code}, message: ${err.message}`);
					});
				} catch (exception) {
					console.error(`Failed to set window orientation. Cause code: ${exception.code}, message: ${exception.message}`);
				}
			});
		} catch (exception) {
			console.error(`Failed to obtain the top window. Cause code: ${exception.code}, message: ${exception.message}`);
		}
	}
}
