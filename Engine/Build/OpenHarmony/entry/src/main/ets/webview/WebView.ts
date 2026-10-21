import { webview } from '@kit.ArkWeb'
import { BusinessError } from '@ohos.base';

export class WebViewInfo {
	// position
	public x: number = 0;
	public y: number = 0;
	// size
	public w: number = 0;
	public h: number = 0;
	// url
	public url: string = '';
	public isUrl: boolean = true;

	public viewTag: number = 0
	public zoomAccess: boolean = true
	public visible: boolean = true
	public opacity: number = 1
	public backgroundColor: number = 0xffffff;

	public controller:webview.WebviewController = new webview.WebviewController()

	constructor(x: number, y: number, w: number, h: number, viewTag: number) {
		this.x = x;
		this.y = y;
		this.w = w;
		this.h = h;
		this.viewTag = viewTag
	}
}

function copyWebViewInfo(viewInfo: WebViewInfo) : WebViewInfo {
	let newViewInfo: WebViewInfo = new WebViewInfo(viewInfo.x, viewInfo.y, viewInfo.w, viewInfo.h, viewInfo.viewTag);
	newViewInfo.url = viewInfo.url;
	newViewInfo.isUrl = viewInfo.isUrl;
	newViewInfo.zoomAccess = viewInfo.zoomAccess;
	newViewInfo.visible = viewInfo.visible;
	newViewInfo.controller = viewInfo.controller;
	newViewInfo.opacity = viewInfo.opacity;
	newViewInfo.backgroundColor = viewInfo.backgroundColor;
	return newViewInfo;
}

function waitUtilControllerAttached() : Promise<number> {
  return new Promise((resolve, reject) => {
      resolve(1);
  })
}

export class WebView {
    static createWebView(viewTag: number) : void {
		let view = new WebViewInfo(0, 0, 0, 0, viewTag);
		globalThis.webViewArray.push(view);
		globalThis.webViewIndexMap.set(viewTag, globalThis.webViewArray.length - 1);
    }

    static removeWebView(viewTag: number) : void {
		if (globalThis.webViewArray.length > 0) {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray.splice(index, 1);
		// Do not assume the invoking time of removeWebView. After an element is deleted, the following elements need to be advanced by one bit.
		globalThis.webViewIndexMap.forEach((value: number, key: number, map: Map<number, number>) => {
		if (value > index) {
				globalThis.webViewIndexMap.set(key, value - 1);
			}
		})
		// Delete the subscript mapping of the removed webView.
		globalThis.webViewIndexMap.delete(viewTag);
		}
    }

	static loadData(viewTag: number, data: string, mimeType: string, encoding: string, baseURL: string) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		let webViewInfo: WebViewInfo = globalThis.webViewArray[index];
		waitUtilControllerAttached().then(() => {
			webViewInfo.controller.loadData(data, mimeType, encoding, baseURL);
		});
	}

    static loadURL(viewTag: number, url: string) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].url = url;
		globalThis.webViewArray[index].isUrl = true;
		let webViewInfo: WebViewInfo = globalThis.webViewArray[index];
		waitUtilControllerAttached().then(() => {
			webViewInfo.controller.loadUrl(url);
		});
    }

    static loadFile(viewTag: number, filePath: string) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].url = filePath;
		globalThis.webViewArray[index].isUrl = false;
		let webViewInfo: WebViewInfo = globalThis.webViewArray[index];
		waitUtilControllerAttached().then(() => {
			webViewInfo.controller.loadUrl($rawfile(filePath));
		});
    }

    static stopLoading(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].controller.stop();
    }

    static reload(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].controller.refresh();
    }

    static canGoBack(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		let result: number = globalThis.webViewArray[index].controller.accessBackward();
    }

    static canGoForward(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		let result: number = globalThis.webViewArray[index].controller.accessForward();
    }

    static goBack(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].controller.backward();
    }

    static goForward(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].controller.forward();
    }

    static setWebViewRect(viewTag: number, x: number, y: number, w: number, h: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		let tmpWebInfo: WebViewInfo = globalThis.webViewArray[index];
		tmpWebInfo.x = px2vp(x);
		tmpWebInfo.y = px2vp(y);
		tmpWebInfo.w = px2vp(w);
		tmpWebInfo.h = px2vp(h);
		let newViewInfo: WebViewInfo = copyWebViewInfo(tmpWebInfo);
		globalThis.webViewArray[index] = newViewInfo;
    }

    static setVisible(viewTag: number, visible: boolean) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		if (globalThis.webViewArray[index].visible == visible) {
			return;
		}
		let tmpWebInfo: WebViewInfo = globalThis.webViewArray[index];
		tmpWebInfo.visible = visible;
		let newViewInfo: WebViewInfo = copyWebViewInfo(tmpWebInfo);
		globalThis.webViewArray[index] = newViewInfo;
    }

    static setOpacityWebView(viewTag: number, opacity: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		if (globalThis.webViewArray[index].opacity == opacity) {
			return;
		}
		let tmpWebInfo: WebViewInfo = globalThis.webViewArray[index];
		tmpWebInfo.opacity = opacity;
		let newViewInfo: WebViewInfo = copyWebViewInfo(tmpWebInfo);
		globalThis.webViewArray[index] = newViewInfo;
    }

    static setBackgroundTransparent(viewTag: number) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		if (globalThis.webViewArray[index].backgroundColor == Color.Transparent) {
			return;
		}
		globalThis.webViewArray[index].backgroundColor = Color.Transparent;
		let newViewInfo: WebViewInfo = copyWebViewInfo(globalThis.webViewArray[index]);
		globalThis.webViewArray[index] = newViewInfo;
    }

    static evaluateJS(viewTag: number, js: string) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		globalThis.webViewArray[index].controller.runJavaScript(js, (error:BusinessError, result:string) => {
		if (error) {
			// log.warn("webView run JavaScript error:%{public}s", JSON.stringify(error));
			return;
		}
		if (result) {
			globalThis.webResult = result;
			// log.debug("webView run JavaScript result is %{public}s:", result);
		 }
		})
    }

    static setScalesPageToFit(viewTag: number, scalesPageToFit: boolean) : void {
		let index = globalThis.webViewIndexMap.get(viewTag);
		if (globalThis.webViewArray[index].zoomAccess == scalesPageToFit) {
			return;
		}
		let tmpWebInfo: WebViewInfo = globalThis.webViewArray[index];
		tmpWebInfo.zoomAccess = scalesPageToFit
		let newViewInfo: WebViewInfo = copyWebViewInfo(tmpWebInfo);
		globalThis.webViewArray[index] = newViewInfo;
    }
}