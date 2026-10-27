// Development tool: decodes CG link n of an ALD to a PPM file.
//   cgdump <archive.ALD dir> <prefix> <kind> <n> <out.ppm>
#include "../../../Packages/VNDegenKit/Sources/CSystem3/s3.h"

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: cgdump dir prefix kind n out.ppm\n"); return 2; }
    S3Archive a;
    if (!s3_archive_open(&a, argv[1], argv[2], argv[3][0])) { fprintf(stderr, "no archive\n"); return 1; }
    S3Blob b; const char *name = "";
    if (!s3_archive_get(&a, (u32)atoi(argv[4]), &b, &name)) { fprintf(stderr, "no entry\n"); return 1; }
    S3Image img;
    if (!s3_cg_decode(b.data, b.size, &img)) { fprintf(stderr, "decode failed %s\n", name); return 1; }
    printf("%s %dx%d at %d,%d bpp %d mask %04x bank %d\n", name, img.w, img.h, img.x, img.y, img.bpp, img.palMask, img.palBank);
    FILE *f = fopen(argv[5], "wb");
    fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
    for (int i = 0; i < img.w * img.h; i++) {
        u8 px[3];
        if (img.bpp == 8) memcpy(px, img.pal[img.pixels8[i]], 3);
        else { u16 v = img.pixels16[i]; px[0] = (u8)((v >> 11) << 3); px[1] = (u8)(((v >> 5) & 63) << 2); px[2] = (u8)((v & 31) << 3); }
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    return 0;
}
