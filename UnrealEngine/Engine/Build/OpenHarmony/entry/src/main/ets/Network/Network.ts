import hilog from "@ohos.hilog";
import connection from '@ohos.net.connection';

export class Network {
	static GetNetworkConnectionType(): number {
		let netHandle = connection.getDefaultNetSync();
		if (netHandle && netHandle.netId != 0) {
			let res = connection.getNetCapabilitiesSync(netHandle);
			hilog.info(0x0000,'ohos Network', 'NetworkConnectionType is %{public}d', res.bearerTypes[0]);
			return res.bearerTypes[0];
		}
        return -1;
	}
}
