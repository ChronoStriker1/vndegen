// layerExSave.dll saves a layer's image as PNG or TLG5. Both go through Layer.saveLayerImage, whose
// encoders already exist. Tags and the PNG compression level are not written. It also has
// copyBlueToAlpha, which KAG games use to make a snapshot layer opaque before saving it.
#include "ncbind/ncbind.hpp"

#include <algorithm>

#define NCB_MODULE_NAME TJS_N("layerExSave.dll")

struct LayerExSave
{
    // Calls objthis.saveLayerImage(filename, type).
    static tjs_error save(const tjs_char* type, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
    {
        if (numparams < 1)
            return TJS_E_BADPARAMCOUNT;
        tTJSVariant typeName(type);
        tTJSVariant* args[] = {param[0], &typeName};
        return objthis->FuncCall(0, TJS_N("saveLayerImage"), nullptr, nullptr, 2, args, objthis);
    }

    // saveLayerImagePng(filename, tags)
    static tjs_error savePng(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
    {
        return save(TJS_N("png"), numparams, param, objthis);
    }

    // The 32-bit pixels of a layer's image, read from its mainImageBuffer properties.
    struct Pixels
    {
        tjs_uint8* data = nullptr;
        tjs_int width = 0, height = 0, pitch = 0;
    };
    static Pixels pixels(iTJSDispatch2* layer, const tjs_char* buffer)
    {
        auto get = [layer](const tjs_char* name)
        {
            tTJSVariant value;
            if (!layer || TJS_FAILED(layer->PropGet(0, name, nullptr, &value, layer)))
                TVPThrowExceptionMessage(TJS_N("copyBlueToAlpha needs layers with images"));
            return value.AsInteger();
        };
        Pixels result;
        result.width = (tjs_int)get(TJS_N("imageWidth"));
        result.height = (tjs_int)get(TJS_N("imageHeight"));
        result.pitch = (tjs_int)get(TJS_N("mainImageBufferPitch"));
        result.data = (tjs_uint8*)(intptr_t)get(buffer);
        return result;
    }

    // copyBlueToAlpha(src) copies src's blue channel into this layer's alpha channel, over the
    // area the two images share.
    static tjs_error copyBlueToAlpha(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
    {
        if (numparams < 1)
            return TJS_E_BADPARAMCOUNT;
        Pixels src = pixels(param[0]->AsObjectNoAddRef(), TJS_N("mainImageBuffer"));
        Pixels dest = pixels(objthis, TJS_N("mainImageBufferForWrite"));
        tjs_int width = std::min(src.width, dest.width), height = std::min(src.height, dest.height);
        for (tjs_int y = 0; y < height; y++)
        {
            const tjs_uint8* from = src.data + y * src.pitch;
            tjs_uint8* to = dest.data + y * dest.pitch + 3;
            for (tjs_int x = 0; x < width; x++)
                to[x * 4] = from[x * 4];
        }
        return TJS_S_OK;
    }

    // saveLayerImageTlg5(filename, tags)
    static tjs_error saveTlg5(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
    {
        return save(TJS_N("tlg5"), numparams, param, objthis);
    }
};

NCB_ATTACH_CLASS(LayerExSave, Layer)
{
    RawCallback("saveLayerImagePng", &LayerExSave::savePng, 0);
    RawCallback("saveLayerImageTlg5", &LayerExSave::saveTlg5, 0);
    RawCallback("copyBlueToAlpha", &LayerExSave::copyBlueToAlpha, 0);
}
