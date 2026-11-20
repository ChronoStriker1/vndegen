#include "ncbind/ncbind.hpp"

#include "extrans/mosaic.h"

static void RegisterExtrans2Dll() { RegisterMosaicInOutTransHandlerProviders(); }
static void UnregisterExtrans2Dll() { UnregisterMosaicInOutTransHandlerProviders(); }
static void RegisterExtrans2Tpm() { RegisterMosaicInOutTransHandlerProviders(); }
static void UnregisterExtrans2Tpm() { UnregisterMosaicInOutTransHandlerProviders(); }

#define NCB_MODULE_NAME TJS_N("extrans2.dll")
NCB_PRE_REGIST_CALLBACK(RegisterExtrans2Dll);
NCB_POST_UNREGIST_CALLBACK(UnregisterExtrans2Dll);
#undef NCB_MODULE_NAME

#define NCB_MODULE_NAME TJS_N("extrans2.tpm")
NCB_PRE_REGIST_CALLBACK(RegisterExtrans2Tpm);
NCB_POST_UNREGIST_CALLBACK(UnregisterExtrans2Tpm);
#undef NCB_MODULE_NAME
