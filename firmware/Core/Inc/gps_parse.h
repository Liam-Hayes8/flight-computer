#ifndef GPS_PARSE_H
#define GPS_PARSE_H

#include <stdbool.h>

typedef struct {
    bool  valid;          /* true if this sentence had a usable fix   */
    float latitude;       /* decimal degrees, negative = South        */
    float longitude;      /* decimal degrees, negative = West         */
    float altitude_m;     /* meters above mean sea level              */
    int   satellites;     /* number of satellites used in the fix     */
    float hdop;           /* horizontal dilution of precision         */
    int   hour, minute;   /* UTC time from the sentence               */
    float second;
} gps_fix_t;

/* Parse one NMEA sentence. Returns true if it was a valid GGA sentence
 * that was successfully parsed (fix may still be absent — check .valid).
 * Returns false for other sentence types or malformed input. */
bool gps_parse_gga(const char *sentence, gps_fix_t *out);

/* Verify the *HH checksum at the end of an NMEA sentence. */
bool gps_checksum_ok(const char *sentence);

#endif