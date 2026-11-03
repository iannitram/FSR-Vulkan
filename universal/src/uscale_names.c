/* uscale_result_name, uscale_backend_name: present (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include "uscale.h"

const char *uscale_result_name(uscale_result result)
{
    switch (result) {
    case USCALE_OK:                          return "USCALE_OK";
    case USCALE_ERROR_INVALID_ARGUMENT:      return "USCALE_ERROR_INVALID_ARGUMENT";
    case USCALE_ERROR_UNSUPPORTED:           return "USCALE_ERROR_UNSUPPORTED";
    case USCALE_ERROR_NOT_IMPLEMENTED:       return "USCALE_ERROR_NOT_IMPLEMENTED";
    case USCALE_ERROR_OUT_OF_MEMORY:         return "USCALE_ERROR_OUT_OF_MEMORY";
    case USCALE_ERROR_VULKAN:                return "USCALE_ERROR_VULKAN";
    case USCALE_ERROR_VERSION_MISMATCH:      return "USCALE_ERROR_VERSION_MISMATCH";
    case USCALE_ERROR_RUNTIME_MISSING:       return "USCALE_ERROR_RUNTIME_MISSING";
    default:                                 return "USCALE_ERROR_UNKNOWN";
    }
}

const char *uscale_backend_name(uscale_backend backend)
{
    switch (backend) {
    case USCALE_BACKEND_AUTO:                return "Auto";
    case USCALE_BACKEND_FSR3:                return "FSR 3";
    case USCALE_BACKEND_FSR4:                return "FSR 4";
    case USCALE_BACKEND_XESS:                return "XeSS";
    case USCALE_BACKEND_DLSS:                return "DLSS";
    default:                                 return "Unknown";
    }
}
