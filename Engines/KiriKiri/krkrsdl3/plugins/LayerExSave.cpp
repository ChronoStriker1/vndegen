// layerExSave.dll adds image saving and pixel utilities to Layer. Saving goes through
// Layer.saveLayerImage, whose PNG and TLG5 encoders already exist; tags and the PNG compression
// level are not written. The rest are the plugin's pixel operations that games call while they
// prepare character images and snapshots. The asynchronous Window.startSaveLayerImage family,
// saveLayerImagePngOctet, and the newer statistics methods are left out.
#include "ncbind/ncbind.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#define NCB_MODULE_NAME TJS_N("layerExSave.dll")

namespace
{
// krkrsdl3 keeps a layer's pixels as R, G, B, A bytes, where Windows Kirikiri keeps B, G, R, A.
enum Channel
{
    Red,
    Green,
    Blue,
    Alpha
};

// A script's 0xAARRGGBB color as a pixel in memory.
tjs_uint32 stored(tjs_uint32 color)
{
    return (color & 0xff00ff00) | ((color >> 16) & 0xff) | ((color & 0xff) << 16);
}

// A layer's 32-bit image.
struct Pixels
{
    tjs_uint8* data;
    tjs_int width, height, pitch;

    tjs_uint8* at(tjs_int x, tjs_int y) const { return data + y * pitch + x * 4; }
};

Pixels pixels(iTJSDispatch2* layer, bool forWrite)
{
    auto get = [layer](const tjs_char* name)
    {
        tTJSVariant value;
        if (!layer || TJS_FAILED(layer->PropGet(0, name, nullptr, &value, layer)))
            TVPThrowExceptionMessage(TJS_N("Invalid layer image."));
        return value.AsInteger();
    };
    if (!get(TJS_N("hasImage")))
        TVPThrowExceptionMessage(TJS_N("Invalid layer image."));
    Pixels result;
    result.width = (tjs_int)get(TJS_N("imageWidth"));
    result.height = (tjs_int)get(TJS_N("imageHeight"));
    result.pitch = (tjs_int)get(TJS_N("mainImageBufferPitch"));
    result.data = (tjs_uint8*)(intptr_t)get(forWrite ? TJS_N("mainImageBufferForWrite") : TJS_N("mainImageBuffer"));
    return result;
}

tjs_uint32 pixel(const tjs_uint8* p)
{
    tjs_uint32 value;
    memcpy(&value, p, 4);
    return value;
}

void setPixel(tjs_uint8* p, tjs_uint32 value)
{
    memcpy(p, &value, 4);
}

// The smallest rectangle holding every pixel `keep` accepts, as %[x, y, w, h], or void when there
// is none.
template <typename Keep> void boundingRect(tTJSVariant* result, tjs_int width, tjs_int height, Keep keep)
{
    tjs_int left = width, right = -1, top = height, bottom = -1;
    for (tjs_int y = 0; y < height; y++)
        for (tjs_int x = 0; x < width; x++)
            if (keep(x, y))
            {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    if (!result)
        return;
    result->Clear();
    if (right < 0)
        return;
    ncbDictionaryAccessor rect;
    rect.SetValue(TJS_N("x"), left);
    rect.SetValue(TJS_N("y"), top);
    rect.SetValue(TJS_N("w"), right - left + 1);
    rect.SetValue(TJS_N("h"), bottom - top + 1);
    *result = rect;
}

// Two pixels are the same when their alpha matches and, unless both are fully transparent, their
// color does too.
bool samePixel(const tjs_uint8* a, const tjs_uint8* b)
{
    return a[Alpha] == b[Alpha] && (a[Alpha] == 0 || !memcmp(a, b, 3));
}

// Checks that `base` is a layer image the size of `image`.
Pixels baseLike(const Pixels& image, tTJSVariant* base)
{
    Pixels result = pixels(base->AsObjectNoAddRef(), false);
    if (result.width != image.width || result.height != image.height)
        TVPThrowExceptionMessage(TJS_N("Different layer size."));
    return result;
}

tjs_error save(const tjs_char* type, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 1)
        return TJS_E_BADPARAMCOUNT;
    tTJSVariant typeName(type);
    tTJSVariant* args[] = {param[0], &typeName};
    return objthis->FuncCall(0, TJS_N("saveLayerImage"), nullptr, nullptr, 2, args, objthis);
}

// saveLayerImagePng(filename, tags = void)
tjs_error saveLayerImagePng(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    return save(TJS_N("png"), numparams, param, objthis);
}

// saveLayerImageTlg5(filename, tags = void)
tjs_error saveLayerImageTlg5(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    return save(TJS_N("tlg5"), numparams, param, objthis);
}

// getCropRect(): the rectangle that is not fully transparent.
tjs_error getCropRect(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2* objthis)
{
    Pixels image = pixels(objthis, false);
    boundingRect(result, image.width, image.height, [&](tjs_int x, tjs_int y) { return image.at(x, y)[Alpha] != 0; });
    return TJS_S_OK;
}

// getCropRectZero(): the rectangle that is not 0x00000000, so a transparent pixel with a color counts.
tjs_error getCropRectZero(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2* objthis)
{
    Pixels image = pixels(objthis, false);
    boundingRect(result, image.width, image.height, [&](tjs_int x, tjs_int y) { return pixel(image.at(x, y)) != 0; });
    return TJS_S_OK;
}

// getDiffRect(base): the rectangle where this layer differs from base, a layer of the same size.
tjs_error getDiffRect(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 1)
        return TJS_E_BADPARAMCOUNT;
    Pixels image = pixels(objthis, false);
    Pixels base = baseLike(image, param[0]);
    boundingRect(result, image.width, image.height,
                 [&](tjs_int x, tjs_int y) { return !samePixel(image.at(x, y), base.at(x, y)); });
    return TJS_S_OK;
}

// getDiffPixel(base, samecol = void, diffcol = void): counts the pixels that differ from base,
// filling the same ones with samecol and the different ones with diffcol where those are given.
tjs_error getDiffPixel(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 1)
        return TJS_E_BADPARAMCOUNT;
    bool fillSame = numparams >= 2 && param[1]->Type() != tvtVoid;
    bool fillDiff = numparams >= 3 && param[2]->Type() != tvtVoid;
    tjs_uint32 sameColor = fillSame ? (tjs_uint32)param[1]->AsInteger() : 0;
    tjs_uint32 diffColor = fillDiff ? (tjs_uint32)param[2]->AsInteger() : 0;
    Pixels image = pixels(objthis, fillSame || fillDiff);
    Pixels base = baseLike(image, param[0]);
    tTVInteger count = 0;
    for (tjs_int y = 0; y < image.height; y++)
        for (tjs_int x = 0; x < image.width; x++)
        {
            tjs_uint8* p = image.at(x, y);
            bool same = samePixel(p, base.at(x, y));
            count += !same;
            if (same ? fillSame : fillDiff)
                setPixel(p, stored(same ? sameColor : diffColor));
        }
    if (result)
        *result = count;
    return TJS_S_OK;
}

// oozeColor(level, threshold = 1, fillColor = 0): gives pixels below the alpha threshold the
// average color of their opaque neighbors, `level` pixels deep, so scaling the image down does
// not blend in false colors at its edges. Pixels it does not reach get fillColor (0xRRGGBB).
tjs_error oozeColor(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 1)
        return TJS_E_BADPARAMCOUNT;
    tjs_int level = (tjs_int)param[0]->AsInteger();
    if (level <= 0)
        TVPThrowExceptionMessage(TJS_N("Invalid level count."));
    tjs_int threshold = std::clamp<tjs_int>(numparams > 1 ? (tjs_int)param[1]->AsInteger() : 1, 1, 255);
    tjs_uint32 fill = numparams > 2 ? (tjs_uint32)param[2]->AsInteger() : 0;
    Pixels image = pixels(objthis, true);

    // Pixels that have a color: the opaque ones at first, then each pass's newly filled ones.
    std::vector<bool> colored((size_t)image.width * image.height);
    auto isColored = [&](tjs_int x, tjs_int y)
    { return x >= 0 && y >= 0 && x < image.width && y < image.height && colored[(size_t)y * image.width + x]; };
    for (tjs_int y = 0; y < image.height; y++)
        for (tjs_int x = 0; x < image.width; x++)
        {
            tjs_uint8* p = image.at(x, y);
            if (p[Alpha] >= threshold)
                colored[(size_t)y * image.width + x] = true;
            else
                setPixel(p, (pixel(p) & 0xff000000) | (stored(fill) & 0xffffff));
        }
    static const tjs_int neighbors[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    std::vector<size_t> filled;
    for (tjs_int pass = 0; pass < level; pass++)
    {
        filled.clear();
        for (tjs_int y = 0; y < image.height; y++)
            for (tjs_int x = 0; x < image.width; x++)
            {
                if (isColored(x, y))
                    continue;
                tjs_uint32 sum[3] = {}, count = 0;
                for (const auto& [dx, dy] : neighbors)
                    if (isColored(x + dx, y + dy))
                    {
                        const tjs_uint8* n = image.at(x + dx, y + dy);
                        sum[Red] += n[Red], sum[Green] += n[Green], sum[Blue] += n[Blue], count++;
                    }
                if (!count)
                    continue;
                tjs_uint8* p = image.at(x, y);
                p[Red] = sum[Red] / count, p[Green] = sum[Green] / count, p[Blue] = sum[Blue] / count;
                filled.push_back((size_t)y * image.width + x);
            }
        for (size_t index : filled)
            colored[index] = true;
    }
    return TJS_S_OK;
}

// copyBlueToAlpha(src): copies src's blue channel into this layer's alpha over the area the two
// images share. KAG games use it to make a snapshot layer opaque before saving it.
tjs_error copyBlueToAlpha(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 1)
        return TJS_E_BADPARAMCOUNT;
    Pixels src = pixels(param[0]->AsObjectNoAddRef(), false);
    Pixels dest = pixels(objthis, true);
    tjs_int width = std::min(src.width, dest.width), height = std::min(src.height, dest.height);
    for (tjs_int y = 0; y < height; y++)
        for (tjs_int x = 0; x < width; x++)
            dest.at(x, y)[Alpha] = src.at(x, y)[Blue];
    return TJS_S_OK;
}

// isBlank(x, y, w, h): whether the area, clipped to the image, has 0 in every pixel's blue
// channel, the one byte the original tests.
tjs_error isBlank(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    if (numparams < 4)
        return TJS_E_BADPARAMCOUNT;
    Pixels image = pixels(objthis, false);
    tjs_int left = std::max<tjs_int>(0, (tjs_int)*param[0]), top = std::max<tjs_int>(0, (tjs_int)*param[1]);
    tjs_int right = std::min<tjs_int>(image.width, (tjs_int)*param[0] + (tjs_int)*param[2]);
    tjs_int bottom = std::min<tjs_int>(image.height, (tjs_int)*param[1] + (tjs_int)*param[3]);
    bool blank = true;
    for (tjs_int y = top; blank && y < bottom; y++)
        for (tjs_int x = left; blank && x < right; x++)
            blank = image.at(x, y)[Blue] == 0;
    if (result)
        *result = (tTVInteger)blank;
    return TJS_S_OK;
}

// clearAlpha(threshold = 0, fillColor = 0): makes pixels whose alpha is at most threshold fully
// transparent with color fillColor (0xRRGGBB).
tjs_error clearAlpha(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis)
{
    tjs_int threshold = numparams > 0 ? (tjs_int)param[0]->AsInteger() : 0;
    tjs_uint32 fill = (numparams > 1 ? (tjs_uint32)param[1]->AsInteger() : 0) & 0xffffff;
    Pixels image = pixels(objthis, true);
    for (tjs_int y = 0; y < image.height; y++)
        for (tjs_int x = 0; x < image.width; x++)
            if (image.at(x, y)[Alpha] <= threshold)
                setPixel(image.at(x, y), stored(fill));
    return TJS_S_OK;
}
} // namespace

NCB_ATTACH_FUNCTION(saveLayerImagePng, Layer, saveLayerImagePng);
NCB_ATTACH_FUNCTION(saveLayerImageTlg5, Layer, saveLayerImageTlg5);
NCB_ATTACH_FUNCTION(getCropRect, Layer, getCropRect);
NCB_ATTACH_FUNCTION(getCropRectZero, Layer, getCropRectZero);
NCB_ATTACH_FUNCTION(getDiffRect, Layer, getDiffRect);
NCB_ATTACH_FUNCTION(getDiffPixel, Layer, getDiffPixel);
NCB_ATTACH_FUNCTION(oozeColor, Layer, oozeColor);
NCB_ATTACH_FUNCTION(copyBlueToAlpha, Layer, copyBlueToAlpha);
NCB_ATTACH_FUNCTION(isBlank, Layer, isBlank);
NCB_ATTACH_FUNCTION(clearAlpha, Layer, clearAlpha);
