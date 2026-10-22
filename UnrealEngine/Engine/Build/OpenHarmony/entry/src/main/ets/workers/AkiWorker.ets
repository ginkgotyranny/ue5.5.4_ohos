import { AkiJSBind } from "../common/AkiJSBind"
import { SensorManager } from '../sensor/Sensor';

//往worker线程绑定AKI，绑定成功后，C++调用worker线程的JS函数
AkiJSBind.UeWorkerBindAki();  
SensorManager.listen();