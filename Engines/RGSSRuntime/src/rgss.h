// VNDegen's RGSS runtime: shared declarations for the C side. Everything here runs on the game thread unless a
// comment says otherwise. docs/rgss-cleanroom.md says where each part's behavior comes from.
#ifndef RGSS_H
#define RGSS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { RGSS1 = 1, RGSS2 = 2, RGSS3 = 3 } RGSSVersion;

extern RGSSVersion rgss_version;

void rgss_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

// MARK: Files

// Sets up the game's file system: the save folder first, then the game folder, then its archive, then RTP folders.
void rgss_fs_open(const char *saveDir, const char *gameDir, const char *archive, const char *const *rtps, int rtpCount);
void rgss_fs_close(void);
// Reads a game file by its path as the game writes it, any case and either slash. When `extensions` is given, each is
// tried in turn after the path itself fails, as RGSS does for images and sounds ("Graphics/Titles/Title").
// The buffer is the caller's to free. The resolved name's extension goes to `foundExtension` when given.
bool rgss_fs_read(const char *path, const char *const *extensions, uint8_t **data, size_t *size, const char **foundExtension);
// The real path of a file on disk for a game path, searching like rgss_fs_read but skipping the archive. Writes go to
// the save folder. Returns false when no file exists, after filling `out` with the save folder path.
bool rgss_fs_disk_path(const char *path, char *out, size_t length);
// A path under the save folder for writing a game path, creating no folders.
void rgss_fs_write_path(const char *path, char *out, size_t length);
// Whether a game path exists in any source.
bool rgss_fs_exists(const char *path, const char *const *extensions);
// Finds `relative` under `base` whatever the case of each component. Fills `out` and returns true when it exists.
bool rgss_resolve_case(const char *base, const char *relative, char *out, size_t length);

extern const char *const rgss_image_extensions[];
extern const char *const rgss_audio_extensions[];

// MARK: Pixels

// Colors are straight (not premultiplied) RGBA, one byte each, in memory order R, G, B, A.
typedef uint32_t Pixel;
static inline uint8_t px_r(Pixel p) { return (uint8_t)p; }
static inline uint8_t px_g(Pixel p) { return (uint8_t)(p >> 8); }
static inline uint8_t px_b(Pixel p) { return (uint8_t)(p >> 16); }
static inline uint8_t px_a(Pixel p) { return (uint8_t)(p >> 24); }
static inline Pixel px_make(int r, int g, int b, int a) {
    return (Pixel)(uint8_t)r | (Pixel)(uint8_t)g << 8 | (Pixel)(uint8_t)b << 16 | (Pixel)(uint8_t)a << 24;
}
static inline int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

typedef struct { int x, y, w, h; } IRect;

typedef struct {
    double r, g, b, a;
} ColorF;

typedef struct {
    double r, g, b, gray;
} ToneF;

typedef struct Image {
    int w, h;
    Pixel *px;
} Image;

bool image_alloc(Image *image, int w, int h);
void image_free(Image *image);
// Decodes PNG, JPEG or BMP.
bool image_decode(const uint8_t *data, size_t size, Image *out);
bool image_write_png(const Image *image, const char *path);

// MARK: Bitmap operations (bitmap.c)

void image_blt(Image *dst, int x, int y, const Image *src, IRect srcRect, int opacity);
void image_stretch_blt(Image *dst, IRect dstRect, const Image *src, IRect srcRect, int opacity);
void image_fill(Image *dst, IRect rect, Pixel color);
void image_gradient(Image *dst, IRect rect, Pixel a, Pixel b, bool vertical);
void image_hue_change(Image *image, int hue);
void image_blur(Image *image);
void image_radial_blur(Image *image, int angle, int division);
IRect irect_clip(IRect rect, int w, int h);

// MARK: Text (text.c)

typedef struct {
    const char *name; // the first face name the game asked for that exists, or NULL for the default
    const char *const *names; // every name the game gave, in order
    int nameCount;
    int size;
    bool bold, italic;
} FontRequest;

// The width and height draw_text would use for a string.
void text_size(const FontRequest *font, const char *utf8, int *w, int *h);
// Draws a string's coverage into an 8-bit mask the caller frees. `ascent` is the baseline's distance from the top.
bool text_render(const FontRequest *font, const char *utf8, uint8_t **mask, int *w, int *h, int *ascent);
bool text_font_exists(const char *name);
// Registers the font files in a folder (the game's Fonts/, the RTP's Fonts/) for lookups by family name.
void text_add_font_folder(const char *path);
void text_reset(void);

#endif
