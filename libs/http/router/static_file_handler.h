#pragma once

#include "router.h"

#include <filesystem>

Router::Handler make_static_file_handler(std::filesystem::path document_root);
