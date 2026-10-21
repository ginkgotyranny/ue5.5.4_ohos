import geoLocationManager from '@ohos.geoLocationManager';
import hilog from '@ohos.hilog';
import { BusinessError } from '@ohos.base';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import { GlobalContext, GlobalContextConstants } from './GlobalConstants';
import { Constants } from './Constants';
import intl from '@ohos.intl';
import common from '@ohos.app.ability.common';
import bundleManager from '@ohos.bundle.bundleManager';
class LocationService {
	'Timestamp'?: number;
	'Longitude': number;
	'Latitude': number;
	'HorizontalAccuracy'?: number;
	'VerticalAccuracy'?: number;
	'Altitude'?: number;
}

export default class LocationUtils {
	static initLocationServices(): boolean {
		globalThis.bundleManager = bundleManager;
		let atManager = abilityAccessCtrl.createAtManager();
		const contextThis = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_GLOBAL_THIS);
		const context: common.UIAbilityContext = contextThis as common.UIAbilityContext;
		try {
			atManager.requestPermissionsFromUser(context, Constants.REQUEST_PERMISSIONS).then((data) => {
				if (data.authResults[0] !== 0 || data.authResults[1] !== 0) {
					hilog.info(0x0000, 'LocationUtils', '%{public}s', 'permissionRequestResult failed');
					return false;
				}
				hilog.info(0x0000, 'LocationUtils', '%{public}s', 'permissionRequestResult success');
				let location: LocationService = {
					Longitude: 30, Latitude: 130
				};
				try {
					geoLocationManager.getCurrentLocation().then(locationResult => {
						location.Timestamp = locationResult.timeStamp;
						location.Longitude = locationResult.longitude;
						location.Latitude = locationResult.latitude;
						location.HorizontalAccuracy = locationResult.accuracy;
						location.VerticalAccuracy = locationResult.accuracy;
						location.Altitude = locationResult.altitude;
						hilog.info(0x0000, 'LocationUtils', '%{public}s', 'getCurrentLocation.', locationResult.latitude);
						globalThis.location = location;
					})
				} catch (err) {
					hilog.error(0x0000, 'LocationUtils', 'getLastLocation err, errCode:', (err as BusinessError).code, ",errMessage:", (err as BusinessError).message);
				}
			}).catch((err: Error) => {
				hilog.error(0x0000, 'LocationUtils', 'requestPermissionsFromUser err', JSON.stringify(err) ?? '');
			})
		} catch (err) {
			hilog.error(0x0000, 'LocationUtils', 'requestPermissionsFromUser err', JSON.stringify(err) ?? '');
		}
		return true;
	}

	static getGeolocation: (location: geoLocationManager.Location) => void = (location: geoLocationManager.Location) => {
		let currentLocation: string = '';
		let latitude: number = 0;
		let longitude: number = 0;
		let locale: string = new intl.Locale().language;

		if (latitude === location.latitude && longitude === location.longitude) {
			return;
		}
		latitude = location.latitude;
		longitude = location.longitude;
		let reverseGeocodeRequest: geoLocationManager.ReverseGeoCodeRequest = {
			'locale': locale.toString().includes('zh') ? 'zh' : 'en',
			'latitude': latitude,
			'longitude': longitude,
			'maxItems': 1
		};
		geoLocationManager.getAddressesFromLocation(reverseGeocodeRequest).then(data => {
			if (data[0].placeName) {
				currentLocation = data[0].placeName;
			}
		}).catch((err: Error) => {
			hilog.error(0x0000, 'LocationUtils', 'GetAddressesFromLocation err', JSON.stringify(err) ?? '');
		});
	}

	static geolocationOn(): boolean {
		if (!canIUse('SystemCapability.Location.Location.Core')) {
			return false;
		}
		let requestInfo: geoLocationManager.LocationRequest = {
			'priority': 0x203,
			'scenario': 0x300,
			'timeInterval': 0,
			'distanceInterval': 0,
			'maxAccuracy': 0
		}
		try {
			geoLocationManager.on('locationChange', requestInfo, this.getGeolocation);
			hilog.info(0x0000, 'LocationUtils', '%{public}s', 'geolocationOn');
		} catch (err) {
			hilog.error(0x0000, 'LocationUtils', 'LocationChange err, errCode:', (err as BusinessError).code, ",errMessage:", (err as BusinessError).message);
		}
		return true;
	}

	static geolocationOff(): boolean {
		if (!canIUse('SystemCapability.Location.Location.Core')) {
			return false;
		}
		geoLocationManager.off('locationChange');
		hilog.info(0x0000, 'LocationUtils', '%{public}s', 'geolocationOff');
		return true;
	}

	static getLastLocation(): LocationService {
		hilog.info(0x0000, 'LocationUtils', '%{public}s', 'getLastLocation start');
		let location: LocationService = {
			Longitude: 30, Latitude: 130
		};
		try {
			let locationResult = geoLocationManager.getLastLocation();
			location.Timestamp = locationResult.timeStamp;
			location.Longitude = locationResult.longitude;
			location.Latitude = locationResult.latitude;
			location.HorizontalAccuracy = locationResult.accuracy;
			location.VerticalAccuracy = locationResult.accuracy;
			location.Altitude = locationResult.altitude;
			hilog.info(0x0000, 'LocationUtils', '%{public}s', 'getLastLocation.', JSON.stringify(location));
			globalThis.location = location;
		} catch (err) {
			hilog.error(0x0000, 'LocationUtils', 'getLastLocation err, errCode:', (err as BusinessError).code, ",errMessage:", (err as BusinessError).message);
		}
		return location;
	}

	static isLocationAccuracyAvailable(): boolean {
		if (!canIUse('SystemCapability.Location.Location.Core')) {
			return false;
		}
		hilog.info(0x0000, 'LocationUtils', '%{public}s', 'location accuracy default false');
		return false;
	}

	static isLocationEnabled(): boolean {
		hilog.info(0x0000, 'LocationUtils', '%{public}s', 'location enable start:');
		if (!canIUse('SystemCapability.Location.Location.Core')) {
			return false;
		}

		let locationEnabled: boolean = false;
		try {
			locationEnabled = geoLocationManager.isLocationEnabled();
			hilog.info(0x0000, 'LocationUtils', '%{public}s', 'location enable state:', locationEnabled);
		} catch (err) {
			hilog.error(0x0000, 'LocationUtils', 'isLocationEnabled err, errCode:', (err as BusinessError).code, ",errMessage:", (err as BusinessError).message);
		}
		return locationEnabled;
	}
}