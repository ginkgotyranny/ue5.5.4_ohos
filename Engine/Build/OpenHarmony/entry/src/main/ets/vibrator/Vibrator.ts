import vibrator from '@ohos.vibrator'
import BusinessError from '@ohos.base';

export class Vibrator {
	static Vibrate(duration: number): void {
		try {
			if (duration < 1) {
				vibrator.stopVibration(vibrator.VibratorStopMode.VIBRATOR_STOP_MODE_TIME, (err: BusinessError.BusinessError) => {
					if (err) {
						console.error(`Failed to stopVibration. code: ${err.code}, message: ${err.message}`);
						return;
					}
				});
			} else {
				vibrator.startVibration({
					type: 'time',
					duration: duration,
				}, {
					id: 0,
					usage: 'alarm'
				}, (err: BusinessError.BusinessError) => {
					if (err) {
						console.error(`Failed to startVibration. code: ${err.code}, message: ${err.message}`);
						return;
					}
				})
			}
		} catch (error) {
			let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
			console.error(`Failed to Vibrate. code: ${e.code}, message: ${e.message}`);			
		}
	}
}
