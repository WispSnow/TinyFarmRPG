#pragma once

namespace tinyfarm::web {

void installShellUi();
void setShellStatus(const char* status);
void setShellMapStats(int layer_count, int tileset_count, int batch_count, int map_width, int map_height);
void reportShellWebGlFeatures();

} // namespace tinyfarm::web
