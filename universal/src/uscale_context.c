/* Context lifecycle and dispatch through the backend table: present
 * (code map: universal/src, roadmap 3.02). SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <stdlib.h>

#include "backend.h"

struct uscale_context {
    const uscale_backend_ops *ops; /* never NULL */
    void *state;                   /* owned by ops */
    uscale_backend backend;
    const char *reason;            /* constant string */
    const uscale_framegen_ops *framegen; /* NULL when frame generation is off */
    void *framegen_state;
};

static const uscale_context_hooks *g_hooks = NULL;

void uscale_context_set_hooks(const uscale_context_hooks *hooks)
{
    g_hooks = hooks;
}

static const uscale_backend_ops *ops_for(uscale_backend id)
{
    if (g_hooks != NULL && g_hooks->backend_ops != NULL)
        return g_hooks->backend_ops(id);
    switch (id) {
    case USCALE_BACKEND_FSR3:
        return uscale_backend_fsr3_ops();
    case USCALE_BACKEND_FSR4:
        return uscale_backend_fsr4_ops();
    case USCALE_BACKEND_XESS:
        return uscale_backend_xess_ops();
    case USCALE_BACKEND_DLSS:
        return uscale_backend_dlss_ops();
    default:
        return NULL;
    }
}

static const uscale_framegen_ops *framegen_ops(void)
{
    static const uscale_framegen_ops real = {
        uscale_framegen_create, uscale_framegen_record,
        uscale_framegen_retire, uscale_framegen_destroy
    };
    if (g_hooks != NULL && g_hooks->framegen != NULL)
        return g_hooks->framegen;
    return &real;
}

static uscale_result query_caps(const uscale_context_create_info *info, uscale_device_caps *caps)
{
    if (g_hooks != NULL && g_hooks->query_caps != NULL)
        return g_hooks->query_caps(info, caps);
    return uscale_query_caps(info->physical_device, info->get_instance_proc_addr, info->instance,
                             info->fsr4_asset_dir, caps);
}

static const char *fallback_reason(uscale_backend failed)
{
    switch (failed) {
    case USCALE_BACKEND_FSR4:
        return "fallback: FSR 4 failed to start, using FSR 3";
    case USCALE_BACKEND_XESS:
        return "fallback: XeSS failed to start, using FSR 3";
    case USCALE_BACKEND_DLSS:
        return "fallback: DLSS failed to start, using FSR 3";
    default:
        return "fallback: backend failed to start, using FSR 3";
    }
}

/* Starts one backend; the outputs are set only on success. */
static uscale_result start_backend(uscale_backend id, const uscale_context_create_info *info,
                                   const uscale_backend_ops **out_ops, void **out_state)
{
    const uscale_backend_ops *ops = ops_for(id);
    void *state = NULL;
    uscale_result r;

    if (ops == NULL || ops->create == NULL || ops->record_upscale == NULL || ops->destroy == NULL)
        return USCALE_ERROR_UNSUPPORTED;
    r = ops->create(info, &state);
    if (r != USCALE_OK)
        return r;
    *out_ops = ops;
    *out_state = state;
    return USCALE_OK;
}

uscale_result uscale_context_create(const uscale_context_create_info *info,
                                    uscale_context **out_context)
{
    uscale_device_caps caps;
    uscale_context *ctx;
    uscale_backend chosen;
    const char *reason;
    uscale_result r;

    if (out_context != NULL)
        *out_context = NULL;
    if (info == NULL || out_context == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->struct_size < sizeof(*info) || (info->api_version >> 22u) != USCALE_VERSION_MAJOR)
        return USCALE_ERROR_VERSION_MISMATCH;

    r = query_caps(info, &caps);
    if (r != USCALE_OK)
        return r;
    r = uscale_select_backend(&caps, info->preferred_backend, &chosen, &reason);
    if (r != USCALE_OK)
        return r;

    ctx = (uscale_context *)calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return USCALE_ERROR_OUT_OF_MEMORY;

    r = start_backend(chosen, info, &ctx->ops, &ctx->state);
    if (r != USCALE_OK && chosen != USCALE_BACKEND_FSR3) {
        /* FSR 3 is the floor: when the chosen backend cannot start, run FSR 3 and say so. */
        reason = fallback_reason(chosen);
        chosen = USCALE_BACKEND_FSR3;
        r = start_backend(chosen, info, &ctx->ops, &ctx->state);
    }
    if (r != USCALE_OK) {
        free(ctx);
        return r;
    }
    ctx->backend = chosen;
    ctx->reason = reason;

    if (info->enable_frame_generation != 0) {
        const uscale_framegen_ops *fg = framegen_ops();

        r = fg->create(info, &ctx->framegen_state);
        if (r != USCALE_OK) {
            ctx->ops->destroy(ctx->state);
            free(ctx);
            return r;
        }
        ctx->framegen = fg;
    }
    *out_context = ctx;
    return USCALE_OK;
}

uscale_backend uscale_context_backend(const uscale_context *context, const char **out_reason)
{
    if (context == NULL) {
        if (out_reason != NULL)
            *out_reason = "no context";
        return USCALE_BACKEND_AUTO;
    }
    if (out_reason != NULL)
        *out_reason = context->reason;
    return context->backend;
}

uscale_result uscale_record_upscale(uscale_context *context, const uscale_upscale_info *info)
{
    if (context == NULL || info == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->struct_size < sizeof(*info))
        return USCALE_ERROR_VERSION_MISMATCH;
    return context->ops->record_upscale(context->state, info);
}

uscale_result uscale_record_framegen(uscale_context *context, const uscale_framegen_info *info)
{
    if (context == NULL || info == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->struct_size < sizeof(*info))
        return USCALE_ERROR_VERSION_MISMATCH;
    if (context->framegen == NULL)
        return USCALE_ERROR_UNSUPPORTED; /* frame generation was not asked for */
    return context->framegen->record(context->framegen_state, info);
}

uscale_result uscale_retire_frame(uscale_context *context, uint64_t frame_id)
{
    if (context == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (context->framegen == NULL)
        return USCALE_OK; /* the upscalers hold nothing per frame */
    return context->framegen->retire(context->framegen_state, frame_id);
}

void uscale_context_destroy(uscale_context *context)
{
    if (context == NULL)
        return;
    if (context->framegen != NULL)
        context->framegen->destroy(context->framegen_state);
    context->ops->destroy(context->state);
    free(context);
}
