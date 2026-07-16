#pragma once

#include <filesystem>

#include "extra_data.h"
#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path, bool randomize_spawn_points,
                      extra_data::LootTypesData& extra_data);

}  // namespace json_loader
