#include "gps_parse.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Return a pointer to the start of comma-separated field n.
 * Field 0 is the sentence type ("$GNGGA"), field 1 is the UTC time,
 * and so on. Returns NULL if the sentence has fewer than n fields. */
static const char *field(const char *s, int n)
{
    while (n > 0 && s)
    {
        s = strchr(s, ',');
        if (s) s++;              /* step past the comma */
        n--;
    }
    return s;
}

/* True only if a field actually contains something. Before a satellite
 * fix the GPS emits sentences like "$GNGGA,000042.868,,,,,0,0,,,M,,M,,*56"
 * where most fields are empty, so every read must be guarded. */
static bool has_value(const char *f)
{
    return f && *f != ',' && *f != '\0' && *f != '*';
}

/* NMEA reports coordinates as degrees+minutes packed into one number:
 *   latitude  "3242.7482"  = 32 deg 42.7482 min   (2 degree digits)
 *   longitude "11709.4099" = 117 deg 09.4099 min  (3 degree digits)
 * Convert to plain decimal degrees. */
static float dm_to_degrees(const char *s, int deg_digits)
{
    char dbuf[4] = {0};
    memcpy(dbuf, s, (size_t)deg_digits);
    float degrees = (float)atof(dbuf);
    float minutes = (float)atof(s + deg_digits);
    return degrees + minutes / 60.0f;
}

/* ------------------------------------------------------------------ */
/* Checksum                                                            */
/* ------------------------------------------------------------------ */

/* An NMEA sentence ends with *HH, where HH is the XOR of every character
 * strictly between the '$' and the '*', written as two hex digits.
 * Validating it means a corrupted sentence is discarded rather than
 * silently parsed into plausible-looking garbage. */
bool gps_checksum_ok(const char *s)
{
    if (!s || *s != '$') return false;

    uint8_t sum = 0;
    s++;                                  /* skip '$' */
    while (*s && *s != '*') sum ^= (uint8_t)*s++;

    if (*s != '*') return false;          /* no checksum present */
    long given = strtol(s + 1, NULL, 16);
    return sum == (uint8_t)given;
}

/* ------------------------------------------------------------------ */
/* GGA parser                                                          */
/* ------------------------------------------------------------------ */

bool gps_parse_gga(const char *sentence, gps_fix_t *out)
{
    if (!sentence || !out) return false;
    if (strlen(sentence) < 6) return false;

    /* Accept $GPGGA, $GNGGA, $GLGGA — the talker prefix varies with
     * which constellations produced the fix, the payload does not. */
    if (strncmp(sentence + 3, "GGA", 3) != 0) return false;

    if (!gps_checksum_ok(sentence)) return false;

    memset(out, 0, sizeof(*out));

    /* Field 1: UTC time as hhmmss.sss */
    const char *f = field(sentence, 1);
    if (has_value(f) && strlen(f) >= 6)
    {
        char buf[3] = {0};
        memcpy(buf, f, 2);      out->hour   = atoi(buf);
        memcpy(buf, f + 2, 2);  out->minute = atoi(buf);
        out->second = (float)atof(f + 4);
    }

    /* Field 6: fix quality. 0 means no fix — everything downstream of
     * this is empty, so return early with valid still false. */
    f = field(sentence, 6);
    if (!has_value(f)) return true;
    if (atoi(f) == 0)  return true;
    out->valid = true;

    /* Field 7: satellites used */
    f = field(sentence, 7);
    if (has_value(f)) out->satellites = atoi(f);

    /* Field 8: horizontal dilution of precision */
    f = field(sentence, 8);
    if (has_value(f)) out->hdop = (float)atof(f);

    /* Fields 2-3: latitude and hemisphere */
    f = field(sentence, 2);
    if (has_value(f))
    {
        out->latitude = dm_to_degrees(f, 2);
        const char *ns = field(sentence, 3);
        if (has_value(ns) && *ns == 'S') out->latitude = -out->latitude;
    }

    /* Fields 4-5: longitude and hemisphere */
    f = field(sentence, 4);
    if (has_value(f))
    {
        out->longitude = dm_to_degrees(f, 3);
        const char *ew = field(sentence, 5);
        if (has_value(ew) && *ew == 'W') out->longitude = -out->longitude;
    }

    /* Field 9: altitude above mean sea level, meters */
    f = field(sentence, 9);
    if (has_value(f)) out->altitude_m = (float)atof(f);

    return true;
}