#pragma once

#define DECLARE_SHADER(name) extern const BYTE* c_p##name; extern size_t c_s##name;

DECLARE_SHADER(VS)
DECLARE_SHADER(HS)
DECLARE_SHADER(DS)
DECLARE_SHADER(GS)
DECLARE_SHADER(PS)
