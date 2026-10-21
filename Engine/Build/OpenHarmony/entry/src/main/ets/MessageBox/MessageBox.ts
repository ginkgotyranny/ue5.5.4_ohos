import nativeRender from "libNativeGlue.so";

export class MessageBox {
	Caption: string;
	Text: string;
	ButtonText: Array<string> = new Array<string>();   //C++发送过来的按钮数量
	ButtonPressed: number = -1;

	static MessageBox01: MessageBox;

	static init(MessageBox02: MessageBox): void {
		MessageBox.MessageBox01 = MessageBox02;
	}

	static setCaption(text: string): void {
		MessageBox.MessageBox01.Caption = text;
		console.log("setCaption finished");
	}

	static setText(text: string): void {
		MessageBox.MessageBox01.Text = text;
		console.log("setText finished");
	}

	static addButton(text: string): void {
		MessageBox.MessageBox01.ButtonText.push(text);
		console.log("addButton finished");
	}

	static clear(): void {
		MessageBox.MessageBox01.Caption = "";
		MessageBox.MessageBox01.Text = "";
		MessageBox.MessageBox01.ButtonText = [];
		MessageBox.MessageBox01.ButtonPressed = -1;
		console.log("clear finished");
	}

	static show(callback?: Function): number {
		globalThis.result = callback;
		globalThis.MessageBoxDialogEntity.open();
		console.log(`show finished, ButtonPressed = ${MessageBox.MessageBox01.ButtonPressed}`);
		return MessageBox.MessageBox01.ButtonPressed;   //未点击按钮的默认值-1返回
	}

	static ClickButton(index: number): void {
		MessageBox.MessageBox01.ButtonPressed = index;
		globalThis.result(index);     //点击按钮的实际值返回
		globalThis.result = undefined;
		globalThis.MessageBoxDialogEntity.close();
		MessageBox.clear();
		console.log("ClickButton finished");
	}

}
