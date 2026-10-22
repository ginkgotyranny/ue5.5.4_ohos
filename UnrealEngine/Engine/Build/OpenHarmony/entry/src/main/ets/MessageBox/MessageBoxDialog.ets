import worker from '@ohos.worker';
import { MessageBox } from './MessageBox';
import nativeRender from "libNativeGlue.so";

@CustomDialog
export struct MessageBoxDialog {
	@State MessageBoxEntity: MessageBox = new MessageBox();
	controller: CustomDialogController

	build() {
		Column() {
			Column() {
				Text(this.MessageBoxEntity.Caption).width('100%').textAlign(TextAlign.Start).fontColor('#fff')
				Divider().width('100%').strokeWidth(1).color('#33ffffff')
				Text(this.MessageBoxEntity.Text).fontColor('#fff')
			}.padding(12).backgroundColor('#000')

			Row() {
				ForEach(this.MessageBoxEntity.ButtonText, (item: string, index: number) => {
					Button(item).width('46%')
						.onClick(() => {
							MessageBox.ClickButton(index);
							console.log("onClick!!");
						}).fontColor(0x000000).backgroundColor('#ccc')
				}, (item: string) => item)
			}.width('100%').justifyContent(FlexAlign.SpaceAround).backgroundColor('#ccc').padding(6)
		}.width('50%').height('70%')
	}
}
