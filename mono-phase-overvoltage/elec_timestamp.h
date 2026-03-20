#ifndef _ELEC_TIMESTAMP_H_
#define _ELEC_TIMESTAMP_H_


const uint32_t convertP1TimeStringToTimestamp(const char* meterTimestamp);
const uint8_t getQuarterFromTimestamp(uint32_t meterTimestamp);
const uint32_t getPreviousQuarterTimestampFromTimestamp(uint32_t meterTimestamp);

#endif