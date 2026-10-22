import hilog from "@ohos.hilog"
import batteryInfo from '@ohos.batteryInfo';
import settings from '@ohos.settings';
import connection from '@ohos.net.connection';

export class BatteryInfo {
	static GetBatteryTemperature(): number {
		let batteryTemperature = batteryInfo.batteryTemperature * 0.1;  // batteryTemperature表示当前设备电池的温度，单位0.1摄氏度。
		hilog.info(0x0000,'ohos battery info', 'BatteryTemperature is %{public}d', batteryTemperature);
        return batteryTemperature;
	}
	static GetBatteryChargeState(): number {
		let batteryState = batteryInfo.chargingStatus;
		hilog.info(0x0000,'ohos battery info', 'BatteryChargeState is %{public}d', batteryState);
		return batteryState;
	}
	static GetBatterySoC(): number {
		let batteryCapacityLevel = batteryInfo.batterySOC;
		hilog.info(0x0000,'ohos battery info', 'batteryCapacityLevel is %{public}d', batteryCapacityLevel);
		return batteryCapacityLevel;
	}
}
