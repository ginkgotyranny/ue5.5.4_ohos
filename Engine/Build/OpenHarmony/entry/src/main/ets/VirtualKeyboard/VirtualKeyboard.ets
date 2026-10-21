import { emitter } from '@kit.BasicServicesKit';
import { GlobalContextConstants } from '../common/GlobalConstants';
import { TextInputDialogEntity } from './TextInputDialogEntity';
export class VirtualKeyboard {
  public static controller: CustomDialogController;
  public static showMessageANDType: TextInputDialogEntity;
  constructor(_controller: CustomDialogController, _messageANDCallItemEntity: TextInputDialogEntity) {
    VirtualKeyboard.controller = _controller;
    VirtualKeyboard.showMessageANDType = _messageANDCallItemEntity
  }
  static OpenHarmonyShowVirtualKeyboardInputDialog(type: number, msg: string) {
    emitter.emit(GlobalContextConstants.UE_SHOW_MESSAGE_AND_TYPE, {
      data: {
        type: type,
        message: msg
      }
    });
  }
  static OpenHarmonyHideVirtualKeyboardInputDialog() {
    emitter.emit(GlobalContextConstants.UE_UN_SHOW_MESSAGE_AND_TYPE, {});
  }
  listen() {
    emitter.on(GlobalContextConstants.UE_SHOW_MESSAGE_AND_TYPE, (eventData: emitter.EventData) => {
     VirtualKeyboard.showMessageANDType.message = eventData.data?.message;
     VirtualKeyboard.showMessageANDType.type = eventData.data?.type;
     VirtualKeyboard.controller.open();
    })
    emitter.on(GlobalContextConstants.UE_UN_SHOW_MESSAGE_AND_TYPE, () => {
      VirtualKeyboard.showMessageANDType.message = '';
      VirtualKeyboard.controller.close();
    })
  }
}