import { identifier } from '@kit.AdsKit';
import { abilityAccessCtrl, common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { GlobalContext, GlobalContextConstants } from '../common/GlobalConstants';

export class Information {
	static requestOAIDTrackingConsentPermissions(callback: Function): void {
		try{
			hilog.info(0x0000, 'UEOH', '%{public}s', 'succeeded in requesting permission');
			identifier.getOAID((err: BusinessError, data: string) => {
				if (err.code) {
					hilog.error(0x0000, 'UEOH', '%{public}s', `get oaid failed, error: ${err.code} ${err.message}`);
					callback('');
				} else{
					const oaid: string = data;
					callback(oaid);
					hilog.info(0x0000, 'UEOH', '%{public}s', `succeeded in getting oaid by callback , oaid: ${oaid}`);
				}
			});
		} catch(err){
			hilog.error(0x0000, 'UEOH', '%{public}s', `catch err->${err.code}, ${err.message}`);
			callback('');
		}
	}
}