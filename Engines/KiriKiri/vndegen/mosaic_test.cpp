#include "tjsCommHead.h"
#include "transhandler.h"
#include "TVPTrans.h"
#include "gl/tvpgl.h"
#include "ncbind/ncbind.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

static int failures;

static void expect(bool ok, const std::string &what)
{
    printf("%s: %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (!ok)
        failures++;
}

class Image : public iTVPScanLineProvider
{
public:
    tjs_int W, H;
    std::vector<tjs_uint32> Pixels;
    Image(tjs_int w, tjs_int h) : W(w), H(h), Pixels(w * h) {}
    tjs_error AddRef() override { return TJS_S_OK; }
    tjs_error Release() override { return TJS_S_OK; }
    tjs_error GetWidth(tjs_int *width) override { *width = W; return TJS_S_OK; }
    tjs_error GetHeight(tjs_int *height) override { *height = H; return TJS_S_OK; }
    tjs_error GetPixelFormat(tjs_int *bpp) override { *bpp = 32; return TJS_S_OK; }
    tjs_error GetPitchBytes(tjs_int *pitch) override { *pitch = W * 4; return TJS_S_OK; }
    tjs_error GetScanLine(tjs_int line, const void **scanline) override
    {
        *scanline = Pixels.data() + line * W;
        return TJS_S_OK;
    }
    tjs_error GetScanLineForWrite(tjs_int line, void **scanline) override
    {
        *scanline = Pixels.data() + line * W;
        return TJS_S_OK;
    }
    iTVPTexture2D *GetTexture() override { return nullptr; }
    iTVPTexture2D *GetTextureForRender() override { return nullptr; }
};

class Options : public iTVPSimpleOptionProvider
{
public:
    std::map<std::string, tjs_int64> Values;
    tjs_error AddRef() override { return TJS_S_OK; }
    tjs_error Release() override { return TJS_S_OK; }
    tjs_error GetAsNumber(const tjs_char *name, tjs_int64 *value) override
    {
        auto it = Values.find(name);
        if (it == Values.end())
            return TJS_E_MEMBERNOTFOUND;
        *value = it->second;
        return TJS_S_OK;
    }
    tjs_error GetAsString(const tjs_char *, const tjs_char **) override { return TJS_E_NOTIMPL; }
    tjs_error GetValue(const tjs_char *name, tTJSVariant *dest) override
    {
        auto it = Values.find(name);
        if (it == Values.end())
            return TJS_E_MEMBERNOTFOUND;
        *dest = tTJSVariant(it->second);
        return TJS_S_OK;
    }
    tjs_error Reserved2() override { return TJS_E_NOTIMPL; }
    tjs_error GetDispatchObject(iTJSDispatch2 **) override { return TJS_E_NOTIMPL; }
};

struct Frame
{
    std::vector<tjs_int> Runs;
    tjs_uint32 Center;
    bool More;
};

static const tjs_int W = 640, H = 480;

static iTVPDivisibleTransHandler *start(const char *method, Options &options)
{
    iTVPTransHandlerProvider *provider = TVPFindTransHandlerProvider(ttstr(method));
    iTVPBaseTransHandler *handler = nullptr;
    tTVPTransType type;
    tTVPTransUpdateType update;
    tjs_error er = provider->StartTransition(&options, nullptr, ltOpaque, W, H, W, H, &type, &update, &handler);
    provider->Release();
    if (TJS_FAILED(er) || type != ttExchange || update != tutDivisible)
        return nullptr;
    return static_cast<iTVPDivisibleTransHandler *>(handler);
}

static Frame draw(iTVPDivisibleTransHandler *handler, Image &src1, Image &src2, tjs_uint64 tick)
{
    Image dest(W, H);
    tTVPDivisibleData data = {0, 0, W, H, &dest, 0, 0, &src1, 0, 0, &src2, 0, 0};
    handler->StartProcess(tick);
    handler->Process(&data);
    Frame frame;
    frame.More = handler->EndProcess() == TJS_S_TRUE;
    const tjs_uint32 *row = dest.Pixels.data() + (H / 2) * W;
    tjs_int run = 1;
    for (tjs_int x = 1; x < W; x++)
    {
        if (row[x] == row[x - 1])
            run++;
        else
        {
            frame.Runs.push_back(run);
            run = 1;
        }
    }
    frame.Runs.push_back(run);
    frame.Center = row[W / 2];
    return frame;
}

static bool blocks(const Frame &frame, tjs_int first, tjs_int size)
{
    if (frame.Runs.size() < 3 || frame.Runs.front() != first)
        return false;
    for (size_t i = 1; i + 1 < frame.Runs.size(); i++)
        if (frame.Runs[i] != size)
            return false;
    return true;
}

static void checkBlocks(const char *name, iTVPDivisibleTransHandler *handler, Image &src1, Image &src2,
                        tjs_uint64 tick, tjs_int first, tjs_int size, bool more)
{
    Frame frame = draw(handler, src1, src2, tick);
    std::string runs;
    for (tjs_int r : frame.Runs)
        runs += " " + std::to_string(r);
    expect(blocks(frame, first, size) && frame.More == more,
           std::string(name) + " at " + std::to_string(tick) + " ms: blocks of " + std::to_string(size) +
               " from " + std::to_string(first) + (more ? ", going on" : ", done") + "; got" + runs +
               (frame.More ? ", going on" : ", done"));
}

static void checkBlend(const char *name, iTVPDivisibleTransHandler *handler, Image &black, Image &white,
                       tjs_uint64 tick, tjs_uint32 expected)
{
    Frame frame = draw(handler, black, white, tick);
    char buffer[160];
    snprintf(buffer, sizeof buffer, "%s at %llu ms: center 0x%08x, expected 0x%08x", name,
             (unsigned long long)tick, frame.Center, expected);
    expect(frame.Center == expected, buffer);
}

static bool named(const char *method)
{
    iTVPTransHandlerProvider *provider = TVPFindTransHandlerProvider(ttstr(method));
    const tjs_char *name = nullptr;
    provider->GetName(&name);
    provider->Release();
    return ttstr(name) == ttstr(method);
}

int main()
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    TVPInitTVPGL();
    Image gradient(W, H), black(W, H), white(W, H);
    for (tjs_int y = 0; y < H; y++)
        for (tjs_int x = 0; x < W; x++)
        {
            gradient.Pixels[y * W + x] = 0xff000000 | (y << 12) | x;
            black.Pixels[y * W + x] = 0xff000000;
            white.Pixels[y * W + x] = 0xffffffff;
        }

    expect(ncbAutoRegister::LoadModule(TJS_N("extrans2.tpm")), "the plugin folder's extrans2.tpm loads");
    expect(named("mosaicin") && named("mosaicout"), "mosaicin and mosaicout are registered");
    expect(ncbAutoRegister::LoadModule(TJS_N("extrans2.dll")), "Plugins.link of extrans2.dll as well");
    ncbAutoRegister::UnloadModule(TJS_N("extrans2.tpm"));
    ncbAutoRegister::UnloadModule(TJS_N("extrans2.dll"));
    expect(ncbAutoRegister::LoadModule(TJS_N("extrans2.dll")), "extrans2.dll loads again after both unload");
    expect(ncbAutoRegister::LoadModule(TJS_N("extrans.dll")), "extrans.dll loads beside it");

    Options tenioha;
    tenioha.Values = {{"time", 1600}, {"maxsize", 100}, {"stepsize", 5}};
    iTVPDivisibleTransHandler *out = start("mosaicout", tenioha);
    expect(out != nullptr, "mosaicout starts with time=1600 maxsize=100 stepsize=5");
    if (out)
    {
        checkBlocks("mosaicout", out, gradient, gradient, 0, 77, 97, true);
        checkBlocks("mosaicout", out, gradient, gradient, 800, 14, 47, true);
        checkBlocks("mosaicout", out, gradient, gradient, 1000, 5, 37, true);
        out->Release();
    }
    out = start("mosaicout", tenioha);
    if (out)
    {
        checkBlend("mosaicout", out, black, white, 0, 0xff000000);
        checkBlend("mosaicout", out, black, white, 800, 0xff7e7e7e);
        checkBlend("mosaicout", out, black, white, 1000, 0xff9e9e9e);
        checkBlocks("mosaicout", out, gradient, gradient, 1600, 1, 2, false);
        out->Release();
    }

    Options mosaicin;
    mosaicin.Values = {{"time", 1000}, {"maxsize", 60}, {"stepsize", 5}};
    iTVPDivisibleTransHandler *in = start("mosaicin", mosaicin);
    expect(in != nullptr, "mosaicin starts with time=1000 maxsize=60 stepsize=5");
    if (in)
    {
        checkBlocks("mosaicin", in, gradient, gradient, 0, 1, 2, true);
        checkBlocks("mosaicin", in, gradient, gradient, 500, 9, 27, true);
        checkBlocks("mosaicin", in, gradient, gradient, 1000, 6, 57, false);
        in->Release();
    }

    Options defaults;
    defaults.Values = {{"time", 1000}};
    in = start("mosaicin", defaults);
    expect(in != nullptr, "mosaicin starts with only time=1000");
    if (in)
    {
        checkBlocks("mosaicin, default maxsize and stepsize", in, gradient, gradient, 0, 1, 2, true);
        checkBlocks("mosaicin, default maxsize and stepsize", in, gradient, gradient, 250, 9, 9, true);
        in->Release();
    }

    Options none;
    expect(start("mosaicout", none) == nullptr, "mosaicout without time fails, as on the PC");

    iTVPDivisibleTransHandler *mosaic = start("mosaic", defaults);
    expect(mosaic != nullptr, "extrans's mosaic starts with time=1000");
    if (mosaic)
    {
        checkBlocks("mosaic", mosaic, gradient, gradient, 0, 1, 2, true);
        checkBlocks("mosaic", mosaic, gradient, gradient, 250, 8, 16, true);
        mosaic->Release();
    }

    printf("%s\n", failures ? "mosaic checks failed" : "mosaic checks passed");
    return failures != 0;
}
