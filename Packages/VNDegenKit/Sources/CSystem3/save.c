// Save files. The format is the PC engine's, so saves move between VNDegen and Windows.
// Placeholder until the format is confirmed against System39.exe under Wine.
#include "vm.h"

bool s3_save_write(S3 *s, int slot) { (void)s; (void)slot; return false; }
bool s3_save_load(S3 *s, int slot) { (void)s; (void)slot; return false; }
bool s3_save_copy(S3 *s, int to, int from) { (void)s; (void)to; (void)from; return false; }
bool s3_save_write_part(S3 *s, int slot, const S3LV *from, int count) { (void)s; (void)slot; (void)from; (void)count; return false; }
bool s3_save_load_part(S3 *s, int slot, const S3LV *to, int count) { (void)s; (void)slot; (void)to; (void)count; return false; }
bool s3_save_time(S3 *s, int slot, struct Arg *out) { (void)s; (void)slot; (void)out; return false; }
