import nativeRender from "libNativeGlue.so";

export class AffinityInfo{
    static SetAffinityInfo(){
        //调用C++传递核心掩码，暂时没地方获取，写死
        //nativeRender.nativeSetAffinityInfo(false, "0xffff", "0x0");//是否开启·、大核掩码、小核掩码
    }
}