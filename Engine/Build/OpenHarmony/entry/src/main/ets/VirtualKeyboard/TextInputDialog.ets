import { TextInputDialogEntity } from './TextInputDialogEntity'
import nativeRender from "libNativeGlue.so";
import { emitter } from '@kit.BasicServicesKit';
import { GlobalContextConstants } from '../common/GlobalConstants';
@CustomDialog
export struct TextInputDialog {
  @State showMessageANDType: TextInputDialogEntity = new TextInputDialogEntity(0,'');
  controller: CustomDialogController

  build() {
    Column() {
      Row() {
        TextInput({ text: this.showMessageANDType.message })
          .backgroundColor('#ffffff')
          .layoutWeight(1)
          .caretColor(Color.Blue)
          .type(this.showMessageANDType.getInputType())
          .onChange((value) => {
            nativeRender.Napi_nativeVirtualKeyboardChanged(value);
          })
          .onSubmit((enterKey) => {
            nativeRender.Napi_nativeVirtualKeyboardSendKey(enterKey);
            emitter.emit(GlobalContextConstants.UE_UN_SHOW_MESSAGE_AND_TYPE, {});
          })
          .defaultFocus(true) // 页面首次打开时，该TextInput获取焦点
          .enableKeyboardOnFocus(true) // TextInput通过点击以外的方式获焦时，绑定输入法
          .onBlur(()=>{
            //关闭输入法时，自动隐藏键盘
            emitter.emit(GlobalContextConstants.UE_UN_SHOW_MESSAGE_AND_TYPE, {});
          })
      }
      .backgroundColor(Color.Transparent)
    }
    .width('100%')

    .justifyContent(FlexAlign.End)
  }
}
