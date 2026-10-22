import hilog from '@ohos.hilog';
import { handleDialogMsg } from './DialogMsg'

let logTag = 'ThreadUtils';

//主线程处理消息
export default class WorkerMsgUtils {
    static recvWorkerThreadMessage(event) {
        let eventData = event.data;
        hilog.debug(0x0000, logTag, 'mainThread receiveMsg, module:%{public}s, function:%{public}s', eventData.module, eventData.function);

        switch (eventData.module) {
            // case 'EditBox':
            //     handleEditBoxMsg(eventData);
            //     break;
            case "Dialog":
                handleDialogMsg(eventData); 
                break;           
            // case 'WebView':
            //     handleWebViewMsg(eventData);
            //     break;
            // case 'VideoPlay':
            //     handleVideoPlayMsg(eventData);
            //     break;
            default:
                hilog.error(0x0000, logTag, '%{public}s has not implement yet', eventData.module);
        }
    } 
}
