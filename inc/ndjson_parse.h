#pragma once
#include <string>
#include <cstdint>
#include "cbp_trace_reader.h"

// Parse ONE NDJSON line into db_t. Returns false if malformed.
bool parse_ndjson_line(const std::string& line, db_t& out);
