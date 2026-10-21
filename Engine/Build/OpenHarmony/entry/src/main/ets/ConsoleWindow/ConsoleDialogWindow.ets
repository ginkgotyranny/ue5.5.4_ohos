import { setMessageANDselectedItem } from './setMessageANDselectedItem';
import { emitter } from '@kit.BasicServicesKit';
import { GlobalContextConstants } from '../common/GlobalConstants';
import nativeRender from "libNativeGlue.so";

export class ConsoleDialogWindow {
  public static controller: CustomDialogController;
  public static messageANDCallItemEntity: setMessageANDselectedItem;

  constructor(_controller: CustomDialogController, _messageANDCallItemEntity: setMessageANDselectedItem) {
    ConsoleDialogWindow.controller = _controller;
    ConsoleDialogWindow.messageANDCallItemEntity = _messageANDCallItemEntity
  }

  //拉起consolewindow
  static ShowConsoleWindow(Formats: string) {
    emitter.emit(GlobalContextConstants.UE_MESSAGE_AND_CALLITEMENTITY, {
      data: {
        Message: "[Available texture formats: " + Formats + "]"
      }
    });
    console.log(`ConsoleWindow open success`);
  }

  static ClickButtonOK(Command: string): void {
    nativeRender.Napi_nativeConsoleCommand(Command);
    ConsoleDialogWindow.messageANDCallItemEntity.setMessage = "";
    ConsoleDialogWindow.controller.close();
    console.log("ClickButtonOK finished");
  }

  static ClickButtonCancel(): void {
    ConsoleDialogWindow.messageANDCallItemEntity.setMessage = "";
    ConsoleDialogWindow.controller.close();
    console.log("ClickButtonCancel finished");
  }

  listen() {
    //四指弹出命令窗
    emitter.on(GlobalContextConstants.UE_MESSAGE_AND_CALLITEMENTITY, (eventData: emitter.EventData) => {
      ConsoleDialogWindow.messageANDCallItemEntity.setMessage = eventData.data?.Message;
      ConsoleDialogWindow.controller.open();
    })
  }
}