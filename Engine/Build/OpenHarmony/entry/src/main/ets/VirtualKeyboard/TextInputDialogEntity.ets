export class TextInputDialogEntity {
	message: string;
	type: number;
	inputType: InputType;

	constructor(tp: number, msg: string) {
		this.message = msg;
		this.type = tp;
		this.inputType= InputType.Normal;
	}

	public getInputType(): InputType {
		switch (this.type) {
			case 0:
				this.inputType = InputType.Normal;
				break;
			case 1:
				this.inputType = InputType.Number;
				break;
			case 2:
				this.inputType = InputType.PhoneNumber;
				break;
			case 3:
				this.inputType = InputType.Email;
				break;
			case 4:
				this.inputType = InputType.Password;
				break;
		}
		return this.inputType;
	}
}