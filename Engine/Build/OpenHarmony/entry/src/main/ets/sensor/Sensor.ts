import sensor from '@ohos.sensor'
import BusinessError from '@ohos.base';
import nativeRender from "libNativeGlue.so";
import { emitter } from '@kit.BasicServicesKit';
import { GlobalContextConstants } from '../common/GlobalConstants';

export class SensorManager {
	static sensor_mgr: SensorManager = new SensorManager();
	//安卓中适用于游戏模式的传感器就是设置的20_000ns
	//由于现在不支持多线程，所以暂时调大传感器的值，防止频繁调用
	static SENSOR_DELAY_GAME: number = 100_000_000

	bExistGyroscope: boolean = false;
	bGyroUpdate: boolean = false;
	first_acceleration_sample: boolean = true;

	rotationMatrix: number[] = [0, 0, 0, 0, 0, 0, 0, 0, 0];
	orientationAngles: number[] = [0, 0, 0];

	current_accelerometer: number[] = [0, 0, 0];
	current_gyroscope: number[] = [0, 0, 0];
	current_magnetometer: number[] = [0, 0, 0];

	filtered_gravity: number[] = [0, 0, 0];
	last_gravity: number[] = [0, 0, 0];
	last_tilt: number[] = [0, 0, 0];

	static current_tilt: number[] = [0, 0, 0];
	static current_rotation_rate: number[] = [0, 0, 0];
	static current_gravity: number[] = [0, 0, 0];
	static current_acceleration: number[] = [0, 0, 0];

	static listen(): void {
		emitter.on(GlobalContextConstants.UE_RESUME_SENSOR,()=>{
			SensorManager.sensor_mgr.listenAccelerometer();
			SensorManager.sensor_mgr.listenGyroscope();
			SensorManager.sensor_mgr.listenMagnetometer();
		})
		emitter.on(GlobalContextConstants.UE_PAUSE_SENSOR,()=>{
			sensor.off(sensor.SensorId.ACCELEROMETER);
			sensor.off(sensor.SensorId.GYROSCOPE);
			sensor.off(sensor.SensorId.MAGNETIC_FIELD);
		})
		//由于UI线程和worker是异步的，所以初始化时，需要由worker启动传感器
		emitter.emit(GlobalContextConstants.UE_RESUME_SENSOR,{});
	}

	// static flag = 0;

	static updateData(type: sensor.SensorId): void {
		if (type == sensor.SensorId.ACCELEROMETER || type == sensor.SensorId.MAGNETIC_FIELD) {
			try {
				sensor.getRotationMatrix(SensorManager.sensor_mgr.current_accelerometer, SensorManager.sensor_mgr.current_magnetometer, (err: BusinessError.BusinessError, data: sensor.RotationMatrixResponse) => {
					if (err) {
						console.error(`Failed to get rotationMatrix. Code: ${err.code}, message: ${err.message}`);
						return;
					}
					SensorManager.sensor_mgr.rotationMatrix = data.rotation;
				});
			} catch (error) {
				let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
				console.error(`Failed to get rotationMatrix. Code: ${e.code}, message: ${e.message}`);
			}
		}
		try {
			sensor.getOrientation(SensorManager.sensor_mgr.rotationMatrix, (err: BusinessError.BusinessError, data: Array<number>) => {
				if (err) {
					console.error(`Failed to get orientation. Code: ${err.code}, message: ${err.message}`);
					return;
				}
				if (data.length < 3) {
					console.error('Failed to get orientation length' + data.length);
				}
				SensorManager.sensor_mgr.orientationAngles[0] = data[0];
				SensorManager.sensor_mgr.orientationAngles[1] = data[1];
				SensorManager.sensor_mgr.orientationAngles[2] = data[2];
			});
		} catch (error) {
			let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
			console.error(`Failed to get orientation. Code: ${e.code}, message: %{e.message}`);
		}

		if (type == sensor.SensorId.ACCELEROMETER) {
			SensorManager.current_tilt[0] = SensorManager.sensor_mgr.orientationAngles[1];
			SensorManager.current_tilt[1] = SensorManager.sensor_mgr.orientationAngles[2];
			SensorManager.current_tilt[2] = SensorManager.sensor_mgr.orientationAngles[0];
			// this.flag = this.flag | 1;
		} else if (type == sensor.SensorId.GYROSCOPE) {
			SensorManager.current_gravity = SensorManager.sensor_mgr.filtered_gravity;
			SensorManager.current_acceleration[0] = SensorManager.sensor_mgr.current_accelerometer[0] - SensorManager.current_gravity[0];
			SensorManager.current_acceleration[1] = SensorManager.sensor_mgr.current_accelerometer[1] - SensorManager.current_gravity[1];
			SensorManager.current_acceleration[2] = SensorManager.sensor_mgr.current_accelerometer[2] - SensorManager.current_gravity[2];

			if (SensorManager.sensor_mgr.bGyroUpdate) {
				SensorManager.current_rotation_rate = SensorManager.sensor_mgr.current_gyroscope;
				SensorManager.sensor_mgr.bGyroUpdate = false;
			} else if (!SensorManager.sensor_mgr.bExistGyroscope) {
				SensorManager.current_rotation_rate[0] = SensorManager.current_tilt[0] - SensorManager.sensor_mgr.last_tilt[0];
				SensorManager.current_rotation_rate[1] = SensorManager.current_tilt[1] - SensorManager.sensor_mgr.last_tilt[1];
				SensorManager.current_rotation_rate[2] = SensorManager.current_tilt[2] - SensorManager.sensor_mgr.last_tilt[2];
			}
			// this.flag = this.flag | 2;
		} else if (type == sensor.SensorId.MAGNETIC_FIELD) {
			// this.flag = this.flag | 4;
		}
		SensorManager.sensor_mgr.last_tilt = SensorManager.current_tilt;
	}
	
	static PushSensorEvents(): void {
		if (SensorManager.sensor_mgr != null) {
			nativeRender.Napi_nativeHandleSensorEvents(SensorManager.current_tilt, SensorManager.current_rotation_rate, SensorManager.current_gravity, SensorManager.current_acceleration);
		}
	}

	listenAccelerometer() {
		try {
			sensor.on(sensor.SensorId.ACCELEROMETER, function (data) {
				SensorManager.sensor_mgr.current_accelerometer[0] = data.x;
				SensorManager.sensor_mgr.current_accelerometer[1] = data.y;
				SensorManager.sensor_mgr.current_accelerometer[2] = data.z;

				if (!SensorManager.sensor_mgr.first_acceleration_sample) {
					SensorManager.sensor_mgr.filtered_gravity[0] = SensorManager.sensor_mgr.last_gravity[0] * 0.85 + SensorManager.sensor_mgr.current_accelerometer[0] * 0.15;
					SensorManager.sensor_mgr.filtered_gravity[1] = SensorManager.sensor_mgr.last_gravity[1] * 0.85 + SensorManager.sensor_mgr.current_accelerometer[1] * 0.15;
					SensorManager.sensor_mgr.filtered_gravity[2] = SensorManager.sensor_mgr.last_gravity[2] * 0.85 + SensorManager.sensor_mgr.current_accelerometer[2] * 0.15;
				}
				SensorManager.sensor_mgr.first_acceleration_sample = false;
				SensorManager.sensor_mgr.last_gravity = SensorManager.sensor_mgr.filtered_gravity;
				SensorManager.updateData(sensor.SensorId.ACCELEROMETER);
			}, {'interval': SensorManager.SENSOR_DELAY_GAME});
		} catch (error) {
			let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
			console.error(`Failed to invoke on. Code: ${e.code}, message: ${e.message}`);
		}
	}

	//GYROSCOPE
	listenGyroscope() {
		try {
			sensor.getSingleSensor(sensor.SensorId.GYROSCOPE, (err: BusinessError.BusinessError, data: sensor.Sensor) => {
				if (err) {
					console.error(`Failed to get sensor. Code: ${err.code}, message: ${err.message}`);
					return;
				}
				SensorManager.sensor_mgr.bExistGyroscope = true;
				console.info('Succeeded in getting sensor: ' + JSON.stringify(data));
			});
			console.log('start listen gyroscope sensor');
			sensor.on(sensor.SensorId.GYROSCOPE, function(data) {
				SensorManager.sensor_mgr.current_gyroscope[0] = data.x;
				SensorManager.sensor_mgr.current_gyroscope[1] = data.y;
				SensorManager.sensor_mgr.current_gyroscope[2] = data.z;
				SensorManager.sensor_mgr.bGyroUpdate = true;
				SensorManager.updateData(sensor.SensorId.GYROSCOPE)
			});
		} catch (error) {
			let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
			console.error(`Failed to invoke on. Code: ${e.code}, message: ${e.message}`);
		}
	}

	listenMagnetometer() {
		try {
			console.log('start listen magnetometer sensor');
			sensor.on(sensor.SensorId.MAGNETIC_FIELD, function(data) {
				SensorManager.sensor_mgr.current_magnetometer[0] = data.x;
				SensorManager.sensor_mgr.current_magnetometer[1] = data.y;
				SensorManager.sensor_mgr.current_magnetometer[2] = data.z;
			});
			SensorManager.updateData(sensor.SensorId.MAGNETIC_FIELD)
		} catch (error) {
			let e: BusinessError.BusinessError = error as BusinessError.BusinessError;
			console.error(`Failed to invoke on. Code: ${e.code}, message: ${e.message}`);
		}
	}

	static getSensorEvents(): Array<number[]> {
		return [this.current_tilt, this.current_rotation_rate, this.current_gravity, this.current_acceleration];
	}
}
