import router from '@ohos.router';
import { GlobalContextConstants } from '../common/GlobalConstants';
import { GlobalContext } from '../common/GlobalConstants';

@Entry
@Component
struct Index {

  build() {
    Row() {
      Column() {

      }
      .width('100%')
    }
    .height('100%')
  }

  onPageShow(){
    AppStorage.setOrCreate('showSplashScreen', true);
 
    router.replaceUrl({
      url: GlobalContextConstants.UE_MAIN_PAGE_URL
    });
  }
}