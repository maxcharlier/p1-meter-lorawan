// elec_reader.cpp - P1 smart meter reader - parsing DSMR/ESMR telegrams (three-phase)
//
// Adapted from Maarten Pennings' eMeterP1port project (gen2):
//   https://github.com/maarten-pennings/eMeterP1port/tree/master/gen2
// Used with permission. Original work by Maarten Pennings.
// Modifications: adapted OBIS field extraction for Belgian smart meters (ORES, Wallonia),
// added LoRaWAN payload encoding and three-phase L2/L3 voltage and current monitoring.

#include <stdint.h>
#include "cytypes.h"
#include "elec_timestamp.h"
#include "elec_stats.h"

#include <Arduino.h>
#include "elec_reader.h"


// === OBIS FIELD ===============================================================================
// An ObisField describes an OBIS object that we want to extract from the P1 telegram.

// Once a field in the telegram is parsed and passes all checks,
// its value is made available through ObisField.value[].
#define OBIS_VALUE_SIZE 16

#define DATA_TYPE_FLOAT 0
#define DATA_TYPE_NIGHT_DAY 1
#define DATA_TYPE_TIMESTAMP 2

// Represents one OBIS field definition. One instance per OBIS object we're interested in.
// All properties are compile-time constants except `value`, which is populated after parsing.
//  key         short identifier (1 char) for printf-like format strings
//  name        human-readable name (5-15 chars)
//  description from DSMR standard, see https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf
//  obis        OBIS code, e.g. "1-0:1.8.1"
//  open_delim  character before the value (rightmost occurrence)
//  close_delim character after the value (rightmost occurrence)
//  stat_index  index in stats table, -1 means no statistics for this field
//  value       parsed value (populated after successful telegram parse)
class ObisField {
public:
  ObisField(char key, const char *name, const char *description, const char *obis, char open_delim, char close_delim, int8_t stat_index)
    : key(key), name(name), description(description), obis(obis), open_delim(open_delim), close_delim(close_delim), stat_index(stat_index){};
  const char key;
  const char *const name;
  const char *const description;
  const char *const obis;
  const char open_delim;
  const char close_delim;
  const int8_t stat_index;
  char value[OBIS_VALUE_SIZE];
};



// OBIS fields to extract from P1 telegrams - modify as needed
static ObisField obis_fields[P1_NUM_FIELDS] = {
  ObisField('d', "Timestamp",    "Meter Reading Date-time stamp",                                        "0-0:1.0.0",  '(', ')', NO_STATS_INDEX),
  ObisField('L', "Cons-Day-Wh", "Meter Reading electricity delivered to client (Tariff 1)",             "1-0:1.8.1",  '(', '*', NO_STATS_INDEX),
  ObisField('H', "Cons-Night-Wh","Meter Reading electricity delivered to client (Tariff 2)",            "1-0:1.8.2",  '(', '*', NO_STATS_INDEX),
  ObisField('l', "Prod-Day-Wh", "Meter Reading electricity delivered by client (Tariff 1)",             "1-0:2.8.1",  '(', '*', NO_STATS_INDEX),
  ObisField('h', "Prod-Night-Wh","Meter Reading electricity delivered by client (Tariff 2)",            "1-0:2.8.2",  '(', '*', NO_STATS_INDEX),
  ObisField('I', "Night1-Day2", "Tariff indicator electricity",                                         "0-0:96.14.0",'(', ')', NO_STATS_INDEX),
  ObisField('P', "Cons-W",      "Instantaneous Active power import – total +P(QI+QIV) in W resolution", "1-0:1.7.0",  '(', '*', CONS_W_STATS_INDEX),
  ObisField('p', "Prod-W",      "Instantaneous Active power export – total -P(QII+QIII) in W resolution","1-0:2.7.0", '(', '*', PROD_W_STATS_INDEX),
  ObisField('V', "Inst-L1-dV",  "Instantaneous voltage L1 in deci V resolution",                       "1-0:32.7.0", '(', '*', INST_L1_DECI_V_STATS_INDEX),
  ObisField('A', "Inst-L1-cA",  "Instantaneous current L1 in centi A resolution",                      "1-0:31.7.0", '(', '*', INST_L1_CENTI_A_STATS_INDEX),
  ObisField('W', "Inst-L2-dV",  "Instantaneous voltage L2 in deci V resolution",                       "1-0:52.7.0", '(', '*', INST_L2_DECI_V_STATS_INDEX),
  ObisField('X', "Inst-L3-dV",  "Instantaneous voltage L3 in deci V resolution",                       "1-0:72.7.0", '(', '*', INST_L3_DECI_V_STATS_INDEX),
  ObisField('B', "Inst-L2-cA",  "Instantaneous current L2 in centi A resolution",                      "1-0:51.7.0", '(', '*', INST_L2_CENTI_A_STATS_INDEX),
  ObisField('C', "Inst-L3-cA",  "Instantaneous current L3 in centi A resolution",                      "1-0:71.7.0", '(', '*', INST_L3_CENTI_A_STATS_INDEX),
};
// Compile error about brace-enclosed initializer list means P1_NUM_FIELDS doesn't match array size


// === P1 PARSER ================================================================================================
// Character-by-character parser for P1 telegrams. Feed -1 when no char available (for timeout handling).
// Once a complete telegram is received and CRC verified, results are available via obis_fields[].

#define P1_TIMEOUT_MS 1000   // max time between telegrams
#define P1_LINE_SIZE 2100    // max line size (telegram can have 1024 chars, each encoded as HexHex)

// Parser state machine states
enum P1ParseState {
  P1_STATE_IDLE,  // waiting for header start ('/')
  P1_STATE_HEAD,  // collecting header (until blank line)
  P1_STATE_BODY,  // collecting OBIS objects
  P1_STATE_CSUM   // collecting CRC
};

// P1 telegram parser
class P1Parser {
public:
  void begin();
  P1ParseResult add(int ch);
private:
  void set_state_idle();
  void set_state_head();
  void set_state_body();
  void set_state_csum();
  void update_crc(int len);
  bool header_ok();
  bool bodyln_ok();
  bool csumln_ok();
private:
  P1ParseState _state;
  uint32_t _time;
  char _data[P1_LINE_SIZE];
  int _len;
  unsigned int _crc;
};


// Sets the parser to initial state
void P1Parser::begin() {
  set_state_idle();
}


// Updates the `_crc` with the bytes in `_data`, up to `len`.
void P1Parser::update_crc(int len) {
  for (int pos = 0; pos < len; pos++) {
    _crc ^= _data[pos];
    for (int i = 8; i != 0; i--) {
      int bit = _crc & 0x0001;
      _crc >>= 1;
      if (bit) _crc ^= 0xA001;  // If the LSB is set xor with polynome x16+x15+x2+1
    }
  }
}


void P1Parser::set_state_idle() {
  _state = P1_STATE_IDLE;
  _time = millis();
  _len = 0;
}


void P1Parser::set_state_head() {
  _state = P1_STATE_HEAD;
  _time = millis();
  _len = 0;
  _crc = 0x0000;
}


void P1Parser::set_state_body() {
  _state = P1_STATE_BODY;
  _len = 0;
}


void P1Parser::set_state_csum() {
  _state = P1_STATE_CSUM;
}


// Validates header line, updates CRC, clears field values
bool P1Parser::header_ok() {
  // "/KFM5KAIFA-METER<CR><LF><CR><LF>"
  bool ok = _len > 8 && _data[0] == '/' && _data[4] == '5' && _data[_len - 4] == '\r' && _data[_len - 3] == '\n' && _data[_len - 2] == '\r' && _data[_len - 1] == '\n';
  if (!ok) {
    _data[_len - 4] = '\0';
    Serial.printf("p1: ERROR header corrupt '%s'\n", _data);
    return false;
  }

  update_crc(_len);

  for (int i = 0; i < P1_NUM_FIELDS; i++) {
    obis_fields[i].value[0] = '\0';
  }

  return true;
}


// Returns true iff the `_data[0.._len)` is a body line (obis object).
// Prints and error if not.
// Updates CRC with received bytes, sets field value if it matches one of the fields
bool P1Parser::bodyln_ok() {
  // "1-0:1.8.1(012345.678*kWh)<CR><LF>"
  bool ok = _len > 2 && _data[_len - 2] == '\r' && _data[_len - 1] == '\n';
  if (!ok) {
    _data[_len - 2] = '\0';
    Serial.printf("p1: ERROR body line corrupt '%s'\n", _data);
    return false;
  }

  update_crc(_len);
  _data[_len - 2] = '\0';

  for (int i = 0; i < P1_NUM_FIELDS; i++) {
    if (strncmp(_data, obis_fields[i].obis, strlen(obis_fields[i].obis)) == 0) {
      char *pos_open_delim = strrchr(_data, obis_fields[i].open_delim);
      if (pos_open_delim == 0) {
        Serial.printf("p1: ERROR body line '%s' could not find open delim '%c'\n", _data, obis_fields[i].open_delim);
        return false;
      }
      char *pos_close_delim = strrchr(_data, obis_fields[i].close_delim);
      if (pos_open_delim == 0) {
        Serial.printf("p1: ERROR body line '%s' could not find close delim '%c'\n", _data, obis_fields[i].close_delim);
        return false;
      }

      int len = pos_close_delim - pos_open_delim - 1;
      if (len <= 0 || len >= OBIS_VALUE_SIZE) {
        // 0 len not allowed (and empty value means not found)
        // OBIS_VALUE_SIZE not allowed (we need to append the terminating zero)
        Serial.printf("p1: ERROR body line '%s' data width mismatch (%d)\n", _data, len);
        return false;
      }

      memcpy(obis_fields[i].value, pos_open_delim + 1, len);
      obis_fields[i].value[len] = '\0';
      break;
    }
  }
  return true;
}


// Returns true iff the `_data[0.._len)` is a valid crc line, the CRC matches that of all collected bytes, and all field are found.
// Prints and error if not.
bool P1Parser::csumln_ok() {
  // "!70CE<CR><LF>"
  bool ok = _len == 7 && _data[0] == '!' && isxdigit(_data[1]) && isxdigit(_data[2]) && isxdigit(_data[3]) && isxdigit(_data[4]) && _data[5] == '\r' && _data[6] == '\n';
  if (!ok) {
    _data[_len - 2] = '\0';
    Serial.printf("p1: ERROR csum line corrupt '%s'\n", _data);
    return false;
  }

  update_crc(1);  // include '!' in crc
  int c1 = toupper(_data[1]);
  int d1 = (c1 >= 'A') ? (c1 - 'A' + 10) : (c1 - '0');
  int c2 = toupper(_data[2]);
  int d2 = (c2 >= 'A') ? (c2 - 'A' + 10) : (c2 - '0');
  int c3 = toupper(_data[3]);
  int d3 = (c3 >= 'A') ? (c3 - 'A' + 10) : (c3 - '0');
  int c4 = toupper(_data[4]);
  int d4 = (c4 >= 'A') ? (c4 - 'A' + 10) : (c4 - '0');
  int csum = ((d1 * 16 + d2) * 16 + d3) * 16 + d4;

  if (csum != _crc) {
    Serial.printf("p1: ERROR crc mismatch (actual %04X, expected %04X)\n", _crc, csum);
    return false;
  }

  for (int i = 0; i < P1_NUM_FIELDS; i++) {
    if (obis_fields[i].value[0] == '\0') {
      Serial.printf("p1: missing field '%s' (%s)\n", obis_fields[i].name, obis_fields[i].obis);
      return false;
    }
  }

  return true;
}


// Feed next character from P1 port. Feed -1 when no char available (for timeout handling).
// Returns P1_RESULT_AVAILABLE when complete telegram received with valid CRC and all fields found.
// Returns P1_RESULT_ERROR on timeout, syntax error, CRC mismatch, or missing field.
// Returns P1_RESULT_COLLECTING while telegram is being received.
P1ParseResult P1Parser::add(int ch) {
  P1ParseResult res = P1_RESULT_COLLECTING;

  // Timeout check
  if (ch < 0) {
    if (millis() - _time > P1_TIMEOUT_MS) {
      if (_len > 0) Serial.printf("p1: timeout (%d bytes discarded)\n", _len);
      else Serial.printf("p1: ERROR timeout\n");
      set_state_idle();
      res = P1_RESULT_ERROR;
    }
    return res;
  }

  // Waiting for header
  if (_state == P1_STATE_IDLE) {
    if (ch == '/') {
      if (_len > 0) {
        Serial.printf("p1: found header (%d bytes discarded)\n", _len);
        res = P1_RESULT_ERROR;
      }
      set_state_head();
      _data[_len++] = ch;
    } else {
      if (_len == 0) Serial.printf("p1: ERROR data without header\n");
      _len++;
    }
    return res;
  }

  // Buffer overflow check
  if (_len == P1_LINE_SIZE) {
    Serial.printf("p1: ERROR line too long (%d)\n", _len);
    res = P1_RESULT_ERROR;
    set_state_idle();
    return res;
  }
  _data[_len++] = ch;

  switch (_state) {
    case P1_STATE_HEAD:
      if (_len > 3 && _data[_len - 3] == '\n' && _data[_len - 1] == '\n') {
        if (header_ok()) {
          set_state_body();
        } else {
          res = P1_RESULT_ERROR;
          set_state_idle();
        }
      }
      break;

    case P1_STATE_BODY:
      if (ch == '!') {
        set_state_csum();
      } else if (ch == '\n') {
        if (bodyln_ok()) {
          _len = 0;
        } else {
          res = P1_RESULT_ERROR;
          set_state_idle();
        }
      }
      break;

    case P1_STATE_CSUM:
      if (ch == '\n') {
        if (csumln_ok()) res = P1_RESULT_AVAILABLE;
        else res = P1_RESULT_ERROR;
        set_state_idle();
      }
      break;

    default:
      break;
  }
  return res;
}



// === Public API ================================================================

static P1Parser p1_parser;


void elec_init() {
  p1_parser.begin();
  Serial.printf("p1: init\n");
}

P1ParseResult elec_parser_add(int ch) {
  return p1_parser.add(ch);
}


const char elec_field_key(int ix) {
  return obis_fields[ix].key;
}


const char *elec_field_name(int ix) {
  return obis_fields[ix].name;
}


const char *elec_field_description(int ix) {
  return obis_fields[ix].description;
}


const char *elec_field_value(int ix) {
  return obis_fields[ix].value;
}

const uint8_t elec_field_stat_index(int ix) {
  return obis_fields[ix].stat_index;
}


const bool elec_field_has_stat(int ix) {
  return obis_fields[ix].stat_index != -1;
}


const uint32_t elec_field_int_value(int ix) {
  const char *value = elec_field_value(ix);
  char cleaned[TELE_VALUE_SIZE];
  int j = 0;

  // Copy everything except '.'
  for (int i = 0; i < TELE_VALUE_SIZE && value[i] != '\0'; ++i) {
    if (value[i] != '.') cleaned[j++] = value[i];
  }
  cleaned[j] = '\0';

  return std::atoi(cleaned);
}


const uint32_t getElecTimstamp() {
  return convertP1TimeStringToTimestamp(elec_field_value(TIMESTAMP_INDEX));
}
