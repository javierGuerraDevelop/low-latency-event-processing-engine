// Serializes engine messages for the Discord bot as newline-framed JSON.

#ifndef SHOTCALLERCPP_PROTOCOL_H
#define SHOTCALLERCPP_PROTOCOL_H

#include <string>

#include "engine.h"

// Serializes one message as a single-line JSON object ending in '\n'.
std::string serialize_message(const EngineMessage& message);

#endif // SHOTCALLERCPP_PROTOCOL_H
