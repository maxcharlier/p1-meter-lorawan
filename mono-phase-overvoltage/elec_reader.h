// elec_reader.h - P1 smart meter reader interface
#ifndef _ELEC_READER_H_
#define _ELEC_READER_H_

// Number of OBIS fields to extract (must match array in elec_reader.cpp)
#define P1_NUM_FIELDS 10

// Field indices for direct access
#define TIMESTAMP_INDEX 0
#define CONS_DAY_WH_INDEX 1
#define CONS_NIGHT_WH_INDEX 2
#define PROD_DAY_WH_INDEX 3
#define PROD_NIGHT_WH_INDEX 4
#define NIGHT1_DAY2_FLAG_INDEX 5
#define AVG_ENERGY_IMPORT_INDEX 6
#define CONS_W_INDEX 7
#define PROD_W_INDEX 8
#define INST_L1_DECI_V_INDEX 9
#define INST_L1_CENTI_A_INDEX 10

// Parser result codes
enum P1ParseResult {
  P1_RESULT_ERROR,       // error occurred (timeout, syntax, CRC, missing field)
  P1_RESULT_COLLECTING,  // telegram in progress, no errors yet
  P1_RESULT_AVAILABLE,   // complete telegram received, CRC valid, all fields found
};

// For backward compatibility
typedef P1ParseResult Tele_Result;
#define TELE_RESULT_ERROR P1_RESULT_ERROR
#define TELE_RESULT_COLLECTING P1_RESULT_COLLECTING
#define TELE_RESULT_AVAILABLE P1_RESULT_AVAILABLE
#define TELE_NUMFIELDS P1_NUM_FIELDS

// Initialize the P1 parser
void elec_init();

// Feed characters to parser, returns parse result
P1ParseResult elec_parser_add(int ch);

// Field accessors (valid after P1_RESULT_AVAILABLE)
// Note: 0 <= ix < P1_NUM_FIELDS
const char elec_field_key(int ix);
const char* elec_field_name(int ix);
const char* elec_field_description(int ix);
const char* elec_field_value(int ix);
const uint32_t elec_field_int_value(int ix);
const bool elec_field_has_stat(int ix);
const uint8_t elec_field_stat_index(int ix);
const uint32_t getElecTimstamp();


// Example P1 telegrams (meter IDs anonymized, CRC adapted)

#define TELE_EXAMPLE_1 \
  "/KFM5KAIFA-METER\r\n" \
  "\r\n" \
  "1-3:0.2.8(42)\r\n" \
  "0-0:1.0.0(220605191342S)\r\n" \
  "0-0:96.1.1(456d795f73657269616c5f6e756d626572)\r\n" \
  "1-0:1.8.1(019235.878*kWh)\r\n" \
  "1-0:1.8.2(016881.373*kWh)\r\n" \
  "1-0:2.8.1(000000.000*kWh)\r\n" \
  "1-0:2.8.2(000000.000*kWh)\r\n" \
  "0-0:96.14.0(0001)\r\n" \
  "1-0:1.7.0(00.586*kW)\r\n" \
  "1-0:2.7.0(00.000*kW)\r\n" \
  "0-0:96.7.21(00020)\r\n" \
  "0-0:96.7.9(00008)\r\n" \
  "1-0:99.97.0(3)(0-0:96.7.19)(211209190618W)(0000003557*s)(210416081947S)(0000004676*s)(000101000011W)(2147483647*s)\r\n" \
  "1-0:32.32.0(00000)\r\n" \
  "1-0:52.32.0(00000)\r\n" \
  "1-0:72.32.0(00000)\r\n" \
  "1-0:32.36.0(00000)\r\n" \
  "1-0:52.36.0(00000)\r\n" \
  "1-0:72.36.0(00000)\r\n" \
  "0-0:96.13.1()\r\n" \
  "0-0:96.13.0()\r\n" \
  "1-0:31.7.0(000*A)\r\n" \
  "1-0:51.7.0(001*A)\r\n" \
  "1-0:71.7.0(001*A)\r\n" \
  "1-0:21.7.0(00.004*kW)\r\n" \
  "1-0:22.7.0(00.000*kW)\r\n" \
  "1-0:41.7.0(00.269*kW)\r\n" \
  "1-0:42.7.0(00.000*kW)\r\n" \
  "1-0:61.7.0(00.309*kW)\r\n" \
  "1-0:62.7.0(00.000*kW)\r\n" \
  "0-1:24.1.0(003)\r\n" \
  "0-1:96.1.0(476d795f73657269616c5f6e756d626572)\r\n" \
  "0-1:24.2.1(220605190000S)(16051.816*m3)\r\n" \
  "!78CA\r\n"

#define TELE_EXAMPLE_2 \
  "/KFM5KAIFA-METER\r\n" \
  "\r\n" \
  "1-3:0.2.8(42)\r\n" \
  "0-0:1.0.0(220605191351S)\r\n" \
  "0-0:96.1.1(4530303033303030303034343234323134)\r\n" \
  "1-0:1.8.1(019235.879*kWh)\r\n" \
  "1-0:1.8.2(016881.373*kWh)\r\n" \
  "1-0:2.8.1(000000.000*kWh)\r\n" \
  "1-0:2.8.2(000000.000*kWh)\r\n" \
  "0-0:96.14.0(0001)\r\n" \
  "1-0:1.7.0(00.590*kW)\r\n" \
  "1-0:2.7.0(00.000*kW)\r\n" \
  "0-0:96.7.21(00020)\r\n" \
  "0-0:96.7.9(00008)\r\n" \
  "1-0:99.97.0(3)(0-0:96.7.19)(211209190618W)(0000003557*s)(210416081947S)(0000004676*s)(000101000011W)(2147483647*s)\r\n" \
  "1-0:32.32.0(00000)\r\n" \
  "1-0:52.32.0(00000)\r\n" \
  "1-0:72.32.0(00000)\r\n" \
  "1-0:32.36.0(00000)\r\n" \
  "1-0:52.36.0(00000)\r\n" \
  "1-0:72.36.0(00000)\r\n" \
  "0-0:96.13.1()\r\n" \
  "0-0:96.13.0()\r\n" \
  "1-0:31.7.0(000*A)\r\n" \
  "1-0:51.7.0(001*A)\r\n" \
  "1-0:71.7.0(001*A)\r\n" \
  "1-0:21.7.0(00.004*kW)\r\n" \
  "1-0:22.7.0(00.000*kW)\r\n" \
  "1-0:41.7.0(00.273*kW)\r\n" \
  "1-0:42.7.0(00.000*kW)\r\n" \
  "1-0:61.7.0(00.313*kW)\r\n" \
  "1-0:62.7.0(00.000*kW)\r\n" \
  "0-1:24.1.0(003)\r\n" \
  "0-1:96.1.0(4730303032333430313934303336323135)\r\n" \
  "0-1:24.2.1(220605190000S)(16051.816*m3)\r\n" \
  "!A5F9\r\n"

#define TELE_EXAMPLE_3 \
  "/KFM5KAIFA-METER\r\n" \
  "\r\n" \
  "1-3:0.2.8(42)\r\n" \
  "0-0:1.0.0(220605191411S)\r\n" \
  "0-0:96.1.1(456d795f73657269616c5f6e756d626572)\r\n" \
  "1-0:1.8.1(019235.883*kWh)\r\n" \
  "1-0:1.8.2(016881.373*kWh)\r\n" \
  "1-0:2.8.1(000000.000*kWh)\r\n" \
  "1-0:2.8.2(000000.000*kWh)\r\n" \
  "0-0:96.14.0(0001)\r\n" \
  "1-0:1.7.0(00.584*kW)\r\n" \
  "1-0:2.7.0(00.000*kW)\r\n" \
  "0-0:96.7.21(00020)\r\n" \
  "0-0:96.7.9(00008)\r\n" \
  "1-0:99.97.0(3)(0-0:96.7.19)(211209190618W)(0000003557*s)(210416081947S)(0000004676*s)(000101000011W)(2147483647*s)\r\n" \
  "1-0:32.32.0(00000)\r\n" \
  "1-0:52.32.0(00000)\r\n" \
  "1-0:72.32.0(00000)\r\n" \
  "1-0:32.36.0(00000)\r\n" \
  "1-0:52.36.0(00000)\r\n" \
  "1-0:72.36.0(00000)\r\n" \
  "0-0:96.13.1()\r\n" \
  "0-0:96.13.0()\r\n" \
  "1-0:31.7.0(000*A)\r\n" \
  "1-0:51.7.0(001*A)\r\n" \
  "1-0:71.7.0(001*A)\r\n" \
  "1-0:21.7.0(00.004*kW)\r\n" \
  "1-0:22.7.0(00.000*kW)\r\n" \
  "1-0:41.7.0(00.268*kW)\r\n" \
  "1-0:42.7.0(00.000*kW)\r\n" \
  "1-0:61.7.0(00.315*kW)\r\n" \
  "1-0:62.7.0(00.000*kW)\r\n" \
  "0-1:24.1.0(003)\r\n" \
  "0-1:96.1.0(476d795f73657269616c5f6e756d626572)\r\n" \
  "0-1:24.2.1(220605190000S)(16051.816*m3)\r\n" \
  "!A62C\r\n"

#endif
