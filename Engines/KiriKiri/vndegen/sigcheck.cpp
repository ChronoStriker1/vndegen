// sigcheck.dll checks a publisher's signature on the game's files before it starts. VNDegen does not
// verify them, so every check passes, as it does for untouched files on Windows.
#include "ncbind/ncbind.hpp"

#include <SDL3/SDL_log.h>

#define NCB_MODULE_NAME TJS_N("sigcheck.dll")

struct SignatureCheck
{
    // Window.checkSignature(file, publicKey, info) returns a handler and reports through
    // onCheckSignatureDone(handler, info, result, error), where a result of 1 is a valid signature.
    static tjs_error checkSignature(tTJSVariant* result,
                                    tjs_int numparams,
                                    tTJSVariant** param,
                                    iTJSDispatch2* objthis)
    {
        if (numparams < 2)
            return TJS_E_BADPARAMCOUNT;
        static tjs_int nextHandler = 1;
        tjs_int handler = nextHandler++;
        SDL_Log("vnkk: skipping the signature check of %s", ttstr(*param[0]).AsStdString().c_str());
        tTJSVariant args[] = {handler, numparams > 2 ? *param[2] : tTJSVariant(), (tjs_int)1, ttstr()};
        tTJSVariant* argp[] = {&args[0], &args[1], &args[2], &args[3]};
        objthis->FuncCall(0, TJS_N("onCheckSignatureDone"), nullptr, nullptr, 4, argp, objthis);
        if (result)
            *result = handler;
        return TJS_S_OK;
    }
};

NCB_ATTACH_CLASS(SignatureCheck, Window)
{
    RawCallback("checkSignature", &SignatureCheck::checkSignature, 0);
}
