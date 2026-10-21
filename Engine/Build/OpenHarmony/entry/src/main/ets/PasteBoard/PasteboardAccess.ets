import pasteboard from '@ohos.pasteboard';
import hilog from "@ohos.hilog"
import { BusinessError } from '@kit.BasicServicesKit';

export class PasteboardAccess {
	static OpenHarmonyGetPasteboardData(callback: Function) {
		let systemPasteBoard: pasteboard.SystemPasteboard = pasteboard.getSystemPasteboard();
		systemPasteBoard.getData().then((pasteData: pasteboard.PasteData) => {
			let primaryText: string = pasteData.getPrimaryText();
			if (typeof primaryText === 'string') {
				hilog.info(0x0001, 'UEOH', '%{public}s', `Getting pastedata.`);
				callback(primaryText)
			} else {
				hilog.info(0x0001, 'UEOH', '%{public}s', `Pastedata is not a valid string.`);
				callback('')
			}
		}).catch((err: BusinessError) => {
			hilog.error(0x0001, 'UEOH', '%{public}s', `Failed to get pastedata. Code: ${err.code}, message: ${err.message}.`);
			callback('')
		});
	}

	static OpenHarmonySetPasteboardData(text: string): void {
		let pasteData: pasteboard.PasteData = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
		let systemPasteBoard: pasteboard.SystemPasteboard = pasteboard.getSystemPasteboard();
		try {
			systemPasteBoard.setData(pasteData);
			hilog.info(0x0001, 'UEOH', '%{public}s', `Setting pastedata.`);
		} catch(err) {
			hilog.error(0x0001, 'UEOH', '%{public}s', `Failed to set pastedata. Code: ${err.code}, message: ${err.message}.`);
		}
	}
}
