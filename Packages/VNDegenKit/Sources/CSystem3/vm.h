// Declarations shared by the interpreter (vm.c) and the command handlers (cmds.c).
#ifndef S3_VM_H
#define S3_VM_H

#include "s3.h"

// A variable argument. element marks an array element (var[offset]); otherwise offset steps to
// following variables, as commands that fill var, var+1, ... do.
typedef struct S3LV { u16 var; int offset; bool element; } S3LV;

int s3_byte(S3 *s);
u16 s3_u16(S3 *s);
u32 s3_u32(S3 *s);
u16 s3_expr(S3 *s);
void s3_lvarg(S3 *s, S3LV *lv);
const char *s3_strarg(S3 *s);
u16 *s3_lv_at(S3 *s, const S3LV *lv, int k);
u16 s3_lv_get(S3 *s, const S3LV *lv, int k);
void s3_lv_set(S3 *s, const S3LV *lv, int k, u16 v);

bool s3_goto_page(S3 *s, int page, u32 addr);
void s3_label_call(S3 *s, u32 addr);
void s3_page_call(S3 *s, int page, u32 addr);
void s3_return(S3 *s, bool page);
void s3_push_var(S3 *s, u16 var);
void s3_message(S3 *s, u32 index);

// cmds.c
void s3_command(S3 *s);
void s3_cmd_wait_key(S3 *s);
void s3_cmd_open_menu(S3 *s);
void s3_service_wait(S3 *s);

#endif
