#include <obs-module.h>
#include "common.h"

namespace {

const char* srcGetName(void*) {
    return hco::kSourceDisplayName;
}

void* srcCreate(obs_data_t*, obs_source_t*) {
    return reinterpret_cast<void*>(1);
}

void srcDestroy(void*) {}

uint32_t srcGetWidth(void*)  { return 0; }
uint32_t srcGetHeight(void*) { return 0; }

void srcVideoRender(void*, gs_effect_t*) {}

}  // namespace

namespace hco {

// register a no-op source so the plugin shows up in OBS's "Add source" menu.
void registerDummySource() {
    obs_source_info info{};
    info.id           = kSourceId;
    info.type         = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    info.get_name     = srcGetName;
    info.create       = srcCreate;
    info.destroy      = srcDestroy;
    info.get_width    = srcGetWidth;
    info.get_height   = srcGetHeight;
    info.video_render = srcVideoRender;
    info.icon_type    = OBS_ICON_TYPE_CUSTOM;
    obs_register_source(&info);
}

}  // namespace hco
