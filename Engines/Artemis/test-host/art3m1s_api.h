#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreRuntime CoreRuntime;
typedef struct HostResources HostResources;
typedef struct HostEvents HostEvents;

typedef HostEvents *(*ArtHostEventsCreateFn)(void);
typedef void (*ArtHostEventsDestroyFn)(HostEvents *events);
typedef void (*ArtHostEventsEnableFn)(HostEvents *events, int enabled);
typedef size_t (*ArtHostEventsNextFn)(HostEvents *events);
typedef size_t (*ArtPollEventsFn)(HostEvents *events, uint8_t *out,
                                  size_t capacity, uint32_t *out_count);
typedef int (*ArtSetFontListFn)(HostEvents *events, int monospace, int vertical,
                                const uint8_t *names, size_t names_len);
typedef void (*ArtSetWindowStateFn)(HostEvents *events, int flags);
typedef int (*ArtSetTextReplacementsFn)(HostEvents *events, const uint8_t *json,
                                        size_t json_len);
typedef void (*ArtSetTextTranslationEnabledFn)(HostEvents *events, int enabled);
typedef void (*ArtClearHostStateFn)(HostEvents *events);
typedef HostResources *(*ArtResourcesCreateFn)(void);
typedef void (*ArtResourcesDestroyFn)(HostResources *resources);
typedef void (*ArtResourcesClearFn)(HostResources *resources);
typedef int (*ArtResourcesMountDirectoryFn)(HostResources *resources, const char *path);
typedef int (*ArtResourcesMountPfsFn)(HostResources *resources, const char *path,
                                      const char *encoding);
typedef int (*ArtResourcesSetSaveDirFn)(HostResources *resources, const char *path);
typedef int (*ArtResourcesSetOverrideFn)(HostResources *resources, const char *path,
                                         const uint8_t *data, size_t len);

typedef CoreRuntime *(*ArtRuntimeCreateFn)(uint32_t width, uint32_t height, int32_t backend);
typedef void (*ArtRuntimeDestroyFn)(CoreRuntime *rt);
typedef int32_t (*ArtRuntimeSetResourcesFn)(CoreRuntime *rt, HostResources *resources);
typedef void (*ArtRuntimeSetRuntimeMediaEnabledFn)(CoreRuntime *rt, int enabled);
typedef int32_t (*ArtRuntimeAdvancePresentFn)(CoreRuntime *rt, uint32_t delta_ms);
typedef int32_t (*ArtRuntimeAdvanceWithoutRenderFn)(CoreRuntime *rt, uint32_t delta_ms);
typedef uint32_t (*ArtRuntimeStageFn)(const CoreRuntime *rt);
typedef int32_t (*ArtRuntimeLoadProjectFn)(CoreRuntime *rt, const char *ini,
                                           const char *platform);
typedef int32_t (*ArtRuntimeLoadProjectBytesFn)(CoreRuntime *rt, const uint8_t *ini,
                                                size_t ini_len, const char *platform);
typedef uint32_t (*ArtRuntimePixelBufferSizeFn)(const CoreRuntime *rt);
typedef uint32_t (*ArtRuntimeAdvanceRenderFn)(CoreRuntime *rt, uint32_t delta_ms,
                                              uint8_t *out_pixels, uint32_t capacity);
typedef int32_t (*ArtRuntimeSetExternalSurfaceFn)(CoreRuntime *rt, int32_t kind,
                                                  void *handle, uint32_t width,
                                                  uint32_t height);
typedef void (*ArtRuntimeClearExternalSurfaceFn)(CoreRuntime *rt);
typedef void (*ArtRuntimeFeedMouseFn)(CoreRuntime *rt, int32_t x, int32_t y);
typedef void (*ArtRuntimeFeedClickFn)(CoreRuntime *rt);
typedef void (*ArtRuntimeFeedMouseButtonFn)(CoreRuntime *rt, uint32_t button,
                                            int32_t pressed);
typedef void (*ArtRuntimeFeedTouchFn)(CoreRuntime *rt, uint32_t id, uint8_t phase,
                                      int32_t x, int32_t y);
typedef void (*ArtRuntimeFeedKeyFn)(CoreRuntime *rt, uint32_t vk, int32_t pressed);
typedef int32_t (*ArtRuntimeSubmitDialogFn)(CoreRuntime *rt, int32_t accepted,
                                            const char *text);
typedef int32_t (*ArtRuntimeSubmitTextTranslationFn)(CoreRuntime *rt, uint64_t serial,
                                                     const char *text);
typedef void (*ArtRuntimeSetReportedOsFn)(CoreRuntime *rt, const char *os);
typedef int32_t (*ArtRuntimeSetEmoteBackendFn)(CoreRuntime *rt, int32_t backend);
typedef int32_t (*ArtRuntimeConfigureSpatialUpscaleFn)(CoreRuntime *rt, float render_scale,
                                                       float sharpness);
typedef int32_t (*ArtRuntimeSetRenderQualityPresetFn)(CoreRuntime *rt, int32_t preset);
typedef void (*ArtRuntimeSetProfilerEnabledFn)(const CoreRuntime *rt, int enabled);
typedef int32_t (*ArtRuntimeProfilerSnapshotFn)(const CoreRuntime *rt, uint8_t *out,
                                                uint32_t capacity);
typedef void (*ArtRuntimeSetVolumeFn)(CoreRuntime *rt, const char *channel, float value);
typedef void (*ArtRuntimeNotifyFinishedFn)(CoreRuntime *rt, const char *id);
typedef void (*ArtRuntimeNotifyLifecycleFn)(CoreRuntime *rt, int state);
typedef int32_t (*ArtRuntimeIsExitRequestedFn)(const CoreRuntime *rt);
typedef uint64_t (*ArtRuntimeBackendCapabilitiesFn)(const CoreRuntime *rt);
typedef int32_t (*ArtRuntimeSubmitHttpResultFn)(CoreRuntime *rt, int status_code,
                                                const uint8_t *body, int body_len);
typedef void (*ArtRuntimeSetStringVariableFn)(CoreRuntime *rt, const char *name,
                                              const char *value);
typedef int32_t (*ArtProbeCaptionFn)(HostResources *resources, const uint8_t *ini,
                                     size_t ini_len, const char *platform,
                                     uint8_t *out, int capacity);
typedef void (*ArtSetAnglePathFn)(const char *directory);
typedef void (*ArtSetDebugFn)(int enabled);
typedef int32_t (*ArtSetFontOverrideFn)(const uint8_t *data, int len);
typedef void (*ArtClearFontOverrideFn)(void);
typedef int32_t (*ArtRuntimeUploadVideoLayerFrameFn)(CoreRuntime *rt, const char *id,
                                                     uint32_t width, uint32_t height,
                                                     const uint8_t *rgba,
                                                     size_t rgba_len);

typedef struct ArtHostEventHeaderV1 {
    uint32_t abi_version;
    uint32_t kind;
    uint64_t sequence;
    uint32_t payload_size;
    uint32_t aux;
} ArtHostEventHeaderV1;

typedef struct Art3m1sApiV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t magic;

    ArtHostEventsCreateFn host_events_create;
    ArtHostEventsDestroyFn host_events_destroy;
    ArtHostEventsEnableFn host_events_enable;
    ArtHostEventsNextFn host_events_next;
    ArtPollEventsFn poll_events;
    ArtSetFontListFn set_font_list;
    ArtSetWindowStateFn set_window_state;
    ArtSetTextReplacementsFn set_text_replacements;
    ArtSetTextTranslationEnabledFn set_text_translation_enabled;
    ArtClearHostStateFn clear_host_state;

    ArtResourcesCreateFn resources_create;
    ArtResourcesDestroyFn resources_destroy;
    ArtResourcesClearFn resources_clear;
    ArtResourcesMountDirectoryFn resources_mount_directory;
    ArtResourcesMountPfsFn resources_mount_pfs;
    ArtResourcesSetSaveDirFn resources_set_save_dir;
    ArtResourcesSetOverrideFn resources_set_override;
    ArtResourcesClearFn resources_clear_overrides;

    ArtRuntimeCreateFn runtime_create;
    ArtRuntimeDestroyFn runtime_destroy;
    ArtRuntimeSetResourcesFn runtime_set_resources;
    ArtRuntimeSetRuntimeMediaEnabledFn runtime_set_runtime_media_enabled;
    ArtRuntimeAdvancePresentFn runtime_advance_and_present;
    ArtRuntimeAdvanceWithoutRenderFn runtime_advance_without_render;
    ArtRuntimeStageFn runtime_stage_width;
    ArtRuntimeStageFn runtime_stage_height;
    ArtRuntimeLoadProjectFn runtime_load_project;
    ArtRuntimeLoadProjectBytesFn runtime_load_project_bytes;
    ArtRuntimePixelBufferSizeFn runtime_pixel_buffer_size;
    ArtRuntimeAdvanceRenderFn runtime_advance_and_render;
    ArtRuntimeSetExternalSurfaceFn runtime_set_external_surface;
    ArtRuntimeClearExternalSurfaceFn runtime_clear_external_surface;
    ArtRuntimeFeedMouseFn runtime_feed_mouse;
    ArtRuntimeFeedClickFn runtime_feed_click;
    ArtRuntimeFeedMouseButtonFn runtime_feed_mouse_button;
    ArtRuntimeFeedTouchFn runtime_feed_touch;
    ArtRuntimeFeedKeyFn runtime_feed_key;
    ArtRuntimeSubmitDialogFn runtime_submit_dialog;
    ArtRuntimeSubmitTextTranslationFn runtime_submit_text_translation;
    ArtRuntimeSetReportedOsFn runtime_set_reported_os;
    ArtRuntimeConfigureSpatialUpscaleFn runtime_configure_spatial_upscale;
    ArtRuntimeSetRenderQualityPresetFn runtime_set_render_quality_preset;
    ArtRuntimeSetProfilerEnabledFn runtime_set_profiler_enabled;
    ArtRuntimeProfilerSnapshotFn runtime_profiler_snapshot;
    ArtRuntimeSetVolumeFn runtime_set_volume;
    ArtRuntimeNotifyFinishedFn runtime_notify_video_finished;
    ArtRuntimeNotifyFinishedFn runtime_notify_sound_finished;
    ArtRuntimeNotifyLifecycleFn runtime_notify_lifecycle;
    ArtRuntimeIsExitRequestedFn runtime_is_exit_requested;
    ArtRuntimeBackendCapabilitiesFn runtime_backend_capabilities;
    ArtRuntimeSubmitHttpResultFn runtime_submit_http_result;
    ArtRuntimeSetStringVariableFn runtime_set_string_variable;

    ArtProbeCaptionFn probe_caption;
    ArtSetAnglePathFn set_angle_path;
    ArtSetDebugFn set_debug;
    ArtSetDebugFn set_damage_visualization;
    ArtSetFontOverrideFn set_font_override;
    ArtClearFontOverrideFn clear_font_override;
    ArtRuntimeUploadVideoLayerFrameFn runtime_upload_video_layer_frame;
} Art3m1sApiV1;

/* Primary Artemis facade symbol. Flat migration symbols, if present, are not
 * part of the stable host ABI. */
const Art3m1sApiV1 *art3m1s_get_api_v1(size_t *out_size);

#ifdef __cplusplus
}
#endif
