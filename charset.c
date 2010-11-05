/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"
#include "charset.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

static const char mix_chars[65] = 
    " abcdefghi^jklmnopqr^^stuvwxyz0123456789.,()+-*/=$<>@;:'???????";

char mix_to_C_char(Byte mix_char)
{
    assert((unsigned)mix_char < sizeof mix_chars);
    return mix_chars[(unsigned)mix_char];
}

Byte C_char_to_mix(char c)
{
    const char *s = strchr(mix_chars, tolower(c));
    if (!s) return 63;
    return (Byte) (s - mix_chars);
}
