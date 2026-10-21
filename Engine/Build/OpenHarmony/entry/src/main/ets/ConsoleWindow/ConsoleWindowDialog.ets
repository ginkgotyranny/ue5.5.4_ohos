import { ConsoleDialogWindow } from './ConsoleDialogWindow';
import { setMessageANDselectedItem } from './setMessageANDselectedItem';

@CustomDialog
export struct ConsoleWindowDialog {
	@State MessageANDCallItemEntity: setMessageANDselectedItem = new setMessageANDselectedItem("")
	@State SelectdItem: string = ""
	data: Array<SelectOption> = [{ value: "Common Console Commands" }, { value: "stat FPS" }, { value: "stat Anim" }, {
		value: "stat OpenGLRHI"
	}, { value: "stat VulkanRHI" }, { value: "stat DumpEvents" }, { value: "stat DumpFrame" },
		{ value: "stat DumpHitches" }, { value: "stat Engine" }, { value: "stat Game" }, { value: "stat Grouped" }, {
			value: "stat Hitches"
		}, { value: "stat InitViews" }, { value: "stat LightRendering" },
		{ value: "stat Memory" }, { value: "stat Particles" }, { value: "stat SceneRendering" }, {
			value: "stat SceneUpdate"
		}, { value: "stat ShadowRendering" }, { value: "stat Slow" },
		{ value: "stat Streaming" }, { value: "stat StreamingDetails" }, { value: "stat Unit" }, {
			value: "stat UnitGraph"
		}, { value: "stat StartFile" }, { value: "stat StopFile" }, { value: "GameVer" }, {
			value: "show PostProcessing"
		}, { value: "stat AndroidCPU" }]

	dialogController?: CustomDialogController

	build() {
		Column() {
			Column() {
				Text('Console Window - Enter Command').width('100%').textAlign(TextAlign.Start).fontColor('#fff')
				Divider().width('100%').strokeWidth(1).color('#33ffffff')
				Text(this.MessageANDCallItemEntity.setMessage).fontColor('#fff')
				Select(this.data).margin({ top: 6 }).width('100%').backgroundColor('#ccc')//select组件
					.selected(0)
					.value("Common Console Commands")
					.onSelect((index: number, Item: string) => {
						this.SelectdItem = Item
						console.info('Select:' + Item)
					})
				TextInput({ text: this.SelectdItem })
					.margin({ top: 6 })
					.backgroundColor('#fff')
					.borderRadius(0)
					.onChange((value) => {
						this.SelectdItem = value;
					})
			}.padding(12).backgroundColor('#000')
			Row() {
				Button('Ok').width('46%').fontColor(0x000000).backgroundColor('#ccc')
					.onClick(() => {
						ConsoleDialogWindow.ClickButtonOK(this.SelectdItem)
						console.log('ButtonType.OK')
					})
				Button('Cancel').width('46%').fontColor(0x000000).backgroundColor('#ccc')
					.onClick(() => {
						ConsoleDialogWindow.ClickButtonCancel()
						console.log('ButtonType.Cancel')
					})
			}.width('100%').justifyContent(FlexAlign.SpaceAround).backgroundColor('#ccc').padding(6)
		}.width('50%').height('70%')
	}
}
