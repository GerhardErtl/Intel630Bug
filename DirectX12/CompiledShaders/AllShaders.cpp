//DEFINE_SHADER (PSTest)
#define BYTE unsigned char

#define DEFINE_SHADER(name) const unsigned char* c_p##name = g_p##name; size_t c_s##name = sizeof(g_p##name);

#include "VS.h"
#include "HS.h"
#include "DS.h"
#include "GS.h"
#include "PS.h"

DEFINE_SHADER(VS)
DEFINE_SHADER(HS)
DEFINE_SHADER(DS)
DEFINE_SHADER(GS)
DEFINE_SHADER(PS)

