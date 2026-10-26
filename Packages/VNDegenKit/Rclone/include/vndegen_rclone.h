#ifndef VNDEGEN_RCLONE_H
#define VNDEGEN_RCLONE_H

// rclone's remote control API, built into VNDegen. See rclone.go.

/// Starts rclone with an in-memory config. Call once before any other function.
void VNDRcloneInitialize(void);

/// Runs the rc method `method` with the JSON object `input` and returns its JSON reply, which the caller frees with
/// VNDRcloneFree. `*status` is an HTTP status: 200 on success. Blocks until the method ends. `call` names the call for
/// VNDRcloneCancel.
char *VNDRcloneCall(long long call, const char *method, const char *input, int *status);

/// Cancels the call named `call`, running or not yet started. Its VNDRcloneCall returns an error soon after.
void VNDRcloneCancel(long long call);

void VNDRcloneFree(char *string);

#endif
