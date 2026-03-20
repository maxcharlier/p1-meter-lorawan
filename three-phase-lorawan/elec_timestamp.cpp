#include <stdint.h>
#include <TimeLib.h>
#include <stdlib.h>  // for atoi
#include <string.h>  // for strlen, memcpy

#define NB_SECONDS_IN_A_QUARTER 900

const uint32_t convertP1TimeStringToTimestamp(const char *meterTimestamp) {
  /*
  * We use uint32_t value, max value will be reach in Year 2038.
  */
  // Expected format: "YYMMDDhhmmssX" where X is 'W' or 'S'
  // 240904223307S

  char buf[5];  // buffer for temporary substrings
  int year, month, day, hour, minute, second;
  char timeZone;

  // Extract year (first two digits)
  memcpy(buf, meterTimestamp, 2);
  buf[2] = '\0';
  year = 2000 + atoi(buf);

  // Extract month
  memcpy(buf, meterTimestamp + 2, 2);
  buf[2] = '\0';
  month = atoi(buf);

  // Extract day
  memcpy(buf, meterTimestamp + 4, 2);
  buf[2] = '\0';
  day = atoi(buf);

  // Extract hour
  memcpy(buf, meterTimestamp + 6, 2);
  buf[2] = '\0';
  hour = atoi(buf);

  // Extract minute
  memcpy(buf, meterTimestamp + 8, 2);
  buf[2] = '\0';
  minute = atoi(buf);

  // Extract second
  memcpy(buf, meterTimestamp + 10, 2);
  buf[2] = '\0';
  second = atoi(buf);

  // Extract timezone char ('W' or 'S')
  timeZone = meterTimestamp[12];

  // Convert to Unix timestamp first (in local time)
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  uint32_t timestamp = (uint32_t)makeTime(tm);

  // Adjust for time zone (Belgium: CET/CEST) by subtracting offset
  // This handles all edge cases (day/month/year rollover) correctly
  if (timeZone == 'W') {
    timestamp -= 3600;  // CET is UTC+1, subtract 1 hour
  } else if (timeZone == 'S') {
    timestamp -= 7200;  // CEST is UTC+2, subtract 2 hours
  }

  return timestamp;
}

const uint32_t pythonModulo(uint32_t dividend, uint32_t divisor) {
  /**
   * In arduino x%y do not return a number between 0 and y but between ]-y,y[.
   * This function allow to have a modulo link in Python where the result is between 0 and y-1.
   * where x is the dividend and y the divisor
   */
  if ((dividend % divisor) < 0) {
    return (dividend % divisor) + divisor;
  }
  return dividend % divisor;
}
const uint8_t getQuarterFromTimestamp(uint32_t meterTimestamp) {
  /*
  * return Quarter (0–3) from timestamp string like "250316205831W"
  */
  return pythonModulo(meterTimestamp, 3600) / NB_SECONDS_IN_A_QUARTER;
}


const uint32_t getPreviousQuarterTimestampFromTimestamp(uint32_t meterTimestamp) {
  /*
  * return The timestamp of the previous quarter.
  */
  return (meterTimestamp / NB_SECONDS_IN_A_QUARTER) * NB_SECONDS_IN_A_QUARTER;
}
