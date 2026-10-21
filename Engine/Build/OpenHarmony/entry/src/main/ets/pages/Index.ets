import { ContextType } from '../common/Constants';
import nativerender from 'libNativeGlue.so';
import { WorkerManager } from '../workers/WorkerManager';
import { TextInputDialogEntity } from '../VirtualKeyboard/TextInputDialogEntity';
import { TextInputDialog } from '../VirtualKeyboard/TextInputDialog';
import { GlobalContext, GlobalContextConstants } from '../common/GlobalConstants';
import { AkiJSBind } from '../common/AkiJSBind';
import { AffinityInfo } from '../Affinity/AffinityInfo';
import { MessageBoxDialog } from '../MessageBox/MessageBoxDialog';
import { MessageBox } from '../MessageBox/MessageBox';
import { ConsoleWindowDialog } from '../ConsoleWindow/ConsoleWindowDialog';
import { setMessageANDselectedItem } from '../ConsoleWindow/setMessageANDselectedItem';
import { WebViewInfo } from '../webview/WebView';
import { UeWebView } from '../webview/UeWebView';
import emitter from '@ohos.events.emitter';
import { ConsoleDialogWindow } from '../ConsoleWindow/ConsoleDialogWindow';
import { VirtualKeyboard } from '../VirtualKeyboard/VirtualKeyboard';
import { Display } from '../Display/Display';
import { VideoSplash } from './VideoSplashIndex';
import EtsBundleManager from '../common/EtsBundleManager'

//$${indexImportAdditions}$$

const nativePageLifecycle: ESObject = nativerender.Napi_getContext(ContextType.JSPAGE_LIFECYCLE);
const rawFileUtils: ESObject = nativerender.Napi_getContext(ContextType.RAW_FILE_UTILS);
const inputNapi: ESObject = nativerender.Napi_getContext(ContextType.INPUT_NAPI);

@Entry
@Component
struct Index {
  static {
    //$${indexStaticInitializationAdditions}$$
  }

	@State showSplashScreen: Visibility = Visibility.Hidden;
          ueWorker = WorkerManager.getInstance().getWorker();
	  akiWorker = WorkerManager.getInstance().getAkiWorker();
          //软键盘
          showMessageANDType: TextInputDialogEntity = new TextInputDialogEntity(0, '');
          virtualKeyBoardDialog: VirtualKeyboard = new VirtualKeyboard(new CustomDialogController({
            builder: TextInputDialog({
              showMessageANDType: this.showMessageANDType
            }),
            autoCancel: true,
            alignment: DialogAlignment.Bottom,
            customStyle: true,
          }),this.showMessageANDType)

	// MessageBox
	MessageBoxEntity: MessageBox = new MessageBox();
	MessageBoxDialogEntity: CustomDialogController = new CustomDialogController({
		builder: MessageBoxDialog({
			MessageBoxEntity: this.MessageBoxEntity
		}),
		autoCancel: false,
		customStyle: true,
		alignment: DialogAlignment.Center,
		maskColor: '#99000000'
	})

	//命令窗口
	MessageANDCallItemEntity: setMessageANDselectedItem = new setMessageANDselectedItem("");
	consoleDialogWindow: ConsoleDialogWindow = new ConsoleDialogWindow(new CustomDialogController({
	  builder: ConsoleWindowDialog({
		MessageANDCallItemEntity: this.MessageANDCallItemEntity
	  }),
	  customStyle: true,
	  alignment: DialogAlignment.Top,
	  maskColor: '#99000000',
	  keyboardAvoidMode: KeyboardAvoidMode.NONE
	}), this.MessageANDCallItemEntity)
	
	// WebView
	@State webViewArray: WebViewInfo[] = [];
	private webViewIndexMap: Map<number, number> = new Map;
	
	// PanGesture
  private panOption: PanGestureOptions = new PanGestureOptions({ direction: PanDirection.Up | PanDirection.Down });
  private ueXComponentController: XComponentController = new XComponentController();
  private splashVideo: VideoSplash | null = null
  private bUseNativeSplashVideo: boolean = false;
	onMouseWheel(eventType: string, event: PanGestureEvent) {
		// only for inactive mouse
		if (event.source !== SourceType.Mouse) {
			return;
		}
		const validFinger = event.fingerList?.find(item => item.globalX && item.globalY);
		if (!validFinger || validFinger.id > 1000) {
			return;
		}
		inputNapi.Napi_onMouseWheel(eventType, validFinger?.localX, validFinger?.localY, event?.offsetY);
	}
	
	aboutToAppear() {
		console.log('[LIFECYCLE-Page] onPageShow');
		
		nativePageLifecycle.Napi_onPageShow();
		//这里是类对象，传showMessageANDType 和 dialogController 的地址给globalthis.showMessageANDType和globalthis.dialogController，简单数据类型才会传值
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_MESSAGE_BOX_DIALOG_ENTITY, this.MessageBoxDialogEntity);
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_MESSAGE_BOX_ENTITY, this.MessageBoxEntity);
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_WEB_VIEW_ARRAY, this.webViewArray);
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_WEB_VIEW_INDEX_MAP, this.webViewIndexMap);
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_WEB_VIEW_INDEX_MAP, this.webViewIndexMap);
		GlobalContext.storeGlobalThis(GlobalContextConstants.UE_GLOBAL_THIS, getContext(this));

		MessageBox.init(this.MessageBoxEntity);

		//往MainThread线程绑定AKI，绑定成功后，C++调用UI线程(MainThread)的JS函数
		AkiJSBind.MainThreadBindAki();
		//往C++传递核心掩码方法
		AffinityInfo.SetAffinityInfo();
		//弹出软键盘
		this.virtualKeyBoardDialog.listen()
		//四指弹出命令窗
		this.consoleDialogWindow.listen()
		
		Display.registerDisplayChangeEvent();
    if (EtsBundleManager.ContainKey("bUseNativeSplashVideo")) {
      this.bUseNativeSplashVideo = EtsBundleManager.GetMetaDataString("bUseNativeSplashVideo") == "true";
    }
    //$${indexAboutToAppearAdditions}$$
  }
	onPageShow() {
    		console.log('[LIFECYCLE-Page] onPageShow');
    		emitter.on(GlobalContextConstants.UE_SPLASH_EVENT, () => {
    			this.showSplashScreen = Visibility.Hidden;
    		});
    		if(AppStorage.get('showSplashScreen')) {
    			this.showSplashScreen = Visibility.Visible;
    			AppStorage.setOrCreate('showSplashScreen', false);
    		}
    		emitter.emit(GlobalContextConstants.UE_RESUME_SENSOR, {});
    }
    	
	onPageHide() {
		console.log('[LIFECYCLE-Page] onPageHide');
		emitter.emit(GlobalContextConstants.UE_PAUSE_SENSOR, {});
		nativePageLifecycle.Napi_onPageHide();
	}

  //侧滑返回功能
  onBackPress(): boolean | void {
    nativerender.Napi_DispatchOnBackPress();
    return true;
  }

	build() {
		Stack(){
			Flex({ direction: FlexDirection.Column, alignItems: ItemAlign.Center, justifyContent: FlexAlign.Center }) {
	//XComponent({ id: 'xcomponentId', type: 'surface', libraryname: 'NativeGlue'})
        XComponent({
          id: 'xcomponentId',
          type: XComponentType.SURFACE,
          libraryname: 'NativeGlue',
          controller: this.ueXComponentController
        })
					.onLoad((context) => {
						console.log('XComponent.onLoad Callback');
            if (this.bUseNativeSplashVideo) {
              let surfaceId = this.ueXComponentController.getXComponentSurfaceId();
              this.splashVideo = new VideoSplash(surfaceId);
              this.splashVideo.showSplashVideo();
              this.showSplashScreen = Visibility.Hidden;
            }
					})
					.onDestroy(() => {
						console.log('XComponent.onDestroy Callback');
					})
					.focusable(true)
					.focusOnTouch(true)
					.gesture(
						PanGesture(this.panOption)
							.onActionStart((event: GestureEvent) => {
								this.onMouseWheel("actionStart", event);
							})
							.onActionUpdate((event: GestureEvent) => {
								this.onMouseWheel("actionUpdate", event);
							})
							.onActionEnd((event: GestureEvent) => {
								this.onMouseWheel("actionEnd", event);
							})
					)

				ForEach(this.webViewArray, (item: WebViewInfo) => {
					UeWebView({ viewInfo: item })
				}, (item:WebViewInfo) => (item.viewTag + item.x + item.y + item.w + item.h).toString())
			}
			Image($r('app.media.splashscreen_landscape'))
				.objectFit(ImageFit.Contain)
				.width('100%')
				.height('100%')
				.visibility(this.showSplashScreen)
        .draggable(false)
		}
		.width('100%')
		.height('100%')

	}
}
