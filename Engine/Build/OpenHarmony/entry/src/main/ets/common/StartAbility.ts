import common from '@ohos.app.ability.common';
import { GlobalContext, GlobalContextConstants } from './GlobalConstants';
import bundleManager from '@ohos.bundle.bundleManager';
import { Want } from '@kit.AbilityKit';

export function LaunchURL(URL: string): void {
 	let context = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT);
	let want = {
		action: 'ohos.want.action.viewData',
		entities: ['entity.system.browsable'],
		uri: URL
	};
	context.startAbility(want)
	.then(() => {
		// ...
	})
	.catch((err) => {
		console.error(`Failed to startAbility. Code: ${err.code}, Message: ${err.message}`);
	});
}

export function ForceQuit(): void {
	let context = GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT);
	context.terminateSelf();
}
export function ForceRestart(): void {
	let context: common.UIAbilityContext = getContext(GlobalContext.loadGlobalThis(GlobalContextConstants.UE_ABILITY_CONTEXT)) as common.UIAbilityContext;
	let want: Want = {
		bundleName: bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT).name,
		abilityName: 'EntryAbility'
	};
	try {
		context.getApplicationContext().restartApp(want);
	} catch (error) {
		console.error(`restartApp fail`);
	}
}