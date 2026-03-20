#include <LoRaWan_APP.h>
#include "elec_reader.h"
#include "elec_stats.h"
#include "elec_timestamp.h"
#include "dht_reader.h"
#include "credentials.h"

#define INVERTER_POWER_GPIO_PIN GPIO10
#define ELECTRICAL_METER_DATAREQUEST_GPIO_PIN GPIO7
#define UNUSED_GPIO_PIN GPIO5  // used to generate random seed
#define DHT_GPIO_PIN GPIO4     // DHT22 data pin

#define USER_BUTTON_GPIO_PIN USER_KEY  // The interrupt pin is attached to USER_SW


#define LED_INDICATOR_GPIO_PIN GPIO1

#define TIMEOUT 10          //time in ms
#define DEBOUNCE_DELAY 360  // Debounce delay in milliseconds

#define LORA_ELEC_FRAME_VERSION 2  // Bumped to v2 for DHT22 support
#define ELEC_METER_NB_PHASES 1
#define APPPORT 1  // LoRaWAN app port
#define LORA_RANDOM_DELAY_TRANSMIT_S_MIN 15 // delay between new quarter and effective transmission.
#define LORA_RANDOM_DELAY_TRANSMIT_S_MAX 60 // delay between new quarter and effective transmission.

#define ENABLE_LORAWAN 1

#define DEBUG 0  //all debug controlled here, set DEBUG 0 to DEBUG 1 to turn on debug, set DEBUG 1 to DEBUG 0 to turn off debug

#if DEBUG
#define debug(...) Serial.print(__VA_ARGS__)
#define debugln(...) Serial.println(__VA_ARGS__)
#define debugdelay(x) delay(x)
#else
#define debug(...)
#define debugln(...)
#define debugdelay(x)
#endif


/* LoRaWAN credentials are in credentials.h (not committed to git) */

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = LORAMAC_REGION_EU868;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t loraWanClass = CLASS_A;
/*OTAA or ABP*/
bool overTheAirActivation = true;

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash, when node reset not need to join again */
bool keepNet = LORAWAN_NET_RESERVE;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = LORAWAN_UPLINKMODE;

/* Application port */
uint8_t appPort = APPPORT;

/*the application data transmission duty cycle.  value in [ms].*/
uint8_t txInterval = 6;                          // 1h;
uint32_t appTxDutyCycle = (1 * 60 * 60 * 1000);  // not used but requeried byt arduino lib

/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 4;

int app_fail;  // used by tele converter to count the number of failed read from the meter.
uint8_t last_quarter_transmitted = 0;
uint32_t next_transmission_timestamp = 0;
bool transmission_needed = false;
bool first_msg_received = true;


volatile bool userButtonInterrupt = false;
volatile uint32_t lastUserButtonInterrupt = 0;

void onUserButton() {
  // Debounce the interrupt
  if ((lastUserButtonInterrupt + DEBOUNCE_DELAY) > millis()) {
    return;
  }
  lastUserButtonInterrupt = millis();

  userButtonInterrupt = true;
}

void setup() {

  // init random
  randomSeed(analogRead(UNUSED_GPIO_PIN));

  // Initialize Serial1 (USB serial) at a baud rate of 115200
  Serial.begin(115200);

  Serial.println("Serial 1 is setup.");

  //Set power ON for the inverter : 3.3V
  pinMode(INVERTER_POWER_GPIO_PIN, OUTPUT);
  digitalWrite(INVERTER_POWER_GPIO_PIN, HIGH);

  /**
    The A1 port is activated (start sending data) by setting 'Data Request' line high (to +5V).
    While receiving data, the requesting OSM must keep the 'Data Request' line activated (set to +5V).
    To stop receiving data the OSM has to drop the 'Data Request' line (set it to 'high impedance' mode); hence,
    the data transfer will stop immediately.
    For backward compatibility reasons, no OSM is allowed to set the 'Data Request' line low (set it to GND or 0V).

    For these reason we are using   OD_HI : Open Drain, Drives High
    3.3V when set high
    Open collector (not GND) when set down.
  **/
  pinMode(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, OD_HI);
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, LOW);

  /*
    Initialize second Serial (hardware serial) at a baud rate of 115200 (or the baud rate required by your electrical meter)
    Port settings for P1 are 115200 baud, 8N1, Xon/Xoff.
    Serial1.begin(uint32_t baud=115200, uint32_t config=SERIAL_8N1, int rxPin=-1, int txPin=-1, bool invert=false, unsigned long timeout_ms = 1000UL)
  */
  Serial1.begin(115200, SERIAL_8N1, UART_RX2, UART_TX2, false, 1000UL);

  Serial.println("Serial 2 is setup.");

  /* Initialize DHT22 sensor */
  dht_init(DHT_GPIO_PIN);

  /* Initialize the elec reader */
  elec_init();
  reset_elec_stats();

  //request data from the elec meter
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, HIGH);

  Serial.printf("\n");
  app_fail = 0;


#if (AT_SUPPORT)
  Serial.println("Enable AT support");
  enableAt();
#endif

#if (ENABLE_LORAWAN == 1)

#if (LORAWAN_DEVEUI_AUTO)
  LoRaWAN.generateDeveuiByChipID();
#endif
#if (AT_SUPPORT)
  Serial.println("AT enable getDevParam");
  getDevParam();
#endif
  printDevParam();
  LoRaWAN.init(loraWanClass, loraWanRegion);
  LoRaWAN.ifskipjoin();
  delay(5);
  deviceState = DEVICE_STATE_JOIN;
#else

  deviceState = DEVICE_STATE_CYCLE;
#endif /*ENABLE_LORAWAN */

  //setup user button
  pinMode(USER_BUTTON_GPIO_PIN, INPUT_PULLDOWN);
  attachInterrupt(USER_BUTTON_GPIO_PIN, onUserButton, RISING);

  // setup indicator LED
  pinMode(LED_INDICATOR_GPIO_PIN, OUTPUT);
  digitalWrite(LED_INDICATOR_GPIO_PIN, HIGH);
}

uint8_t add_uint_32_to_app_data(uint32_t value, uint8_t index) {
  appData[index] = value & 0xFF;              // LSB
  appData[index + 1] = (value >> 8) & 0xFF;   // Middle byte
  appData[index + 2] = (value >> 16) & 0xFF;  // More significant byte
  appData[index + 3] = (value >> 24) & 0xFF;  // Most significant byte
  return index + 4;
}


uint8_t add_uint_24_to_app_data(uint32_t value, uint8_t index) {
  /*
   * Used to store Min, Max, mean value into paylaod.
   * We do not need to send the Most Signifiant byte is value is always 0.
   * Max value is 99999.
  */
  appData[index] = value & 0xFF;              // LSB
  appData[index + 1] = (value >> 8) & 0xFF;   // Middle bytes
  appData[index + 2] = (value >> 16) & 0xFF;  // More significant bytes
  return index + 3;
}

uint8_t add_uint_16_to_app_data(uint16_t value, uint8_t index) {
  appData[index] = value & 0xFF;             // LSB
  appData[index + 1] = (value >> 8) & 0xFF;  // Middle byte
  return index + 2;
}


uint8_t add_stats_values_to_app_data(uint8_t stats_index, uint8_t app_data_index) {
  /*
   * Add Min, Max, Mean values to the paylaod (app_data).
  */
  add_uint_24_to_app_data(get_elec_stat_min_value(stats_index), app_data_index);
  add_uint_24_to_app_data(get_elec_stat_max_value(stats_index), app_data_index + 3);
  add_uint_24_to_app_data(get_elec_stat_mean_value(stats_index), app_data_index + 6);
  return app_data_index + 9;
}

uint8_t add_dht22_to_app_data(uint8_t index) {
  /*
   * Add DHT22 readings to payload (2 bytes).
   * Temperature: (value / 2) - 40 = °C
   * Humidity: 0-100%
   */
  dht_read();
  appData[index] = dht_get_encoded_temperature();
  appData[index + 1] = dht_get_encoded_humidity();
  return index + 2;
}

void prepareLoraFrame(bool user_btn_pressed) {
  /*
     * App port is 1
     * Frame format (v2):
      +-------------------+-------------+-------+---------------------------+
      | Value             | Size (bits) | Index | Detail                    |
      +-------------------+-------------+-------+---------------------------+
      | Frame_version     | 4           | 0     | value: 2                  |
      | USR Button        | 1           | 0     | Pressed == 1              |
      | NB Phases         | 2           | 0     | value: 1                  |
      | RESERVED          | 1           | 0     | false                     |
      | Timestamp         | 32          | 1     | Unix timestamp            |
      | Cons-Day-Wh       | 32          | 5     | Current value             |
      | Cons-Night-Wh     | 32          | 9     | Current value             |
      | Prod-Day-Wh       | 32          | 13    | Current value             |
      | Prod-Night-Wh     | 32          | 17    | Current value             |
      | Night1-Day2       | 8           | 21    | Current tariff            |
      | Nb Samples        | 16          | 22    | Sample count              |
      | Cons-W-stats      | 72          | 24    | Min/Max/Mean              |
      | Prod-W-stats      | 72          | 33    | Min/Max/Mean              |
      | L1-dV-stats       | 72          | 42    | Min/Max/Mean (decivolts)  |
      | L1-cA-stats       | 72          | 51    | Min/Max/Mean (centiamps)  |
      | Temperature       | 8           | 60    | (val/2)-40 = °C           |
      | Humidity          | 8           | 61    | 0-100%                    |
      +-------------------+-------------+-------+---------------------------+
      Frame size: 62 bytes
     */
  uint8_t index = 0;

  digitalWrite(LED_INDICATOR_GPIO_PIN, LOW);
  appData[index] = (LORA_ELEC_FRAME_VERSION & 0XF) << 4 | (user_btn_pressed ? 1 : 0) << 3 | (ELEC_METER_NB_PHASES & 0x3) << 1;
  index++;
  index = add_uint_32_to_app_data(getElecTimstamp(), index);
  index = add_uint_32_to_app_data(elec_field_int_value(CONS_DAY_WH_INDEX), index);
  index = add_uint_32_to_app_data(elec_field_int_value(CONS_NIGHT_WH_INDEX), index);
  index = add_uint_32_to_app_data(elec_field_int_value(PROD_DAY_WH_INDEX), index);
  index = add_uint_32_to_app_data(elec_field_int_value(PROD_NIGHT_WH_INDEX), index);
  appData[index] = elec_field_int_value(NIGHT1_DAY2_FLAG_INDEX);
  index++;
  index = add_uint_16_to_app_data(get_elec_stat_nb_samples(CONS_W_STATS_INDEX), index);
  index = add_stats_values_to_app_data(CONS_W_STATS_INDEX, index);
  index = add_stats_values_to_app_data(PROD_W_STATS_INDEX, index);
  index = add_stats_values_to_app_data(INST_L1_DECI_V_STATS_INDEX, index);
  index = add_stats_values_to_app_data(INST_L1_CENTI_A_STATS_INDEX, index);

  // DHT22 temperature and humidity (2 bytes)
  index = add_dht22_to_app_data(index);

  appDataSize = index;
  if (appDataSize > sizeof(appData)) {
    Serial.println("Error: appData size exceeds buffer");
    return;
  }

  digitalWrite(LED_INDICATOR_GPIO_PIN, HIGH);
}


void process_elec_message() {
  uint32_t elect_timestamp = getElecTimstamp();
  uint8_t current_msg_quarter = getQuarterFromTimestamp(elect_timestamp);
  Serial.printf("current_msg_quarter %d %-15s\n", current_msg_quarter, elec_field_value(TIMESTAMP_INDEX));
  for (int i = 0; i < TELE_NUMFIELDS; i++) {
    // update stat
    if (elec_field_has_stat(i)) {
      update_elec_stats(elec_field_stat_index(i), elec_field_int_value(i));
      debug("i %-15s %d\n", elec_field_name(i), get_elec_stat_max_value(elec_field_stat_index(i)));
    } else {
      debug("elec_field_has no stat %d \n", i);
    }
  }

  if (first_msg_received) {
    debug("first_msg_received\n");
    last_quarter_transmitted = current_msg_quarter;
    first_msg_received = false;
  }
  if (current_msg_quarter != last_quarter_transmitted) {
    /*
      New quarter detected, prepare new frame and set flag to transmit it after random delay.
    */
    debug("Prepare new frame\n");
    prepareLoraFrame(false);
    transmission_needed = true;
    next_transmission_timestamp = elect_timestamp + random(LORA_RANDOM_DELAY_TRANSMIT_S_MIN, LORA_RANDOM_DELAY_TRANSMIT_S_MAX);
    last_quarter_transmitted = current_msg_quarter;

    debug("reset_elect_stats\n");
    reset_elec_stats();
  }
  if (transmission_needed) {
    debug("transmission_needed %l\n", next_transmission_timestamp);
    if (next_transmission_timestamp <= elect_timestamp) {
      sendPulseFrame();
      transmission_needed = false;
    }
  }
}
void sendPulseFrame() {
  digitalWrite(LED_INDICATOR_GPIO_PIN, LOW);

  //request data from the elec meter
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, LOW);
  LoRaWAN.send();  // Send the pulse count payload

  Serial.println("Sent msg.");

  //request data from the elec meter
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, HIGH);

  digitalWrite(LED_INDICATOR_GPIO_PIN, HIGH);
}

void loop() {
  if (userButtonInterrupt) {
    userButtonInterrupt = false;
    Serial.println("USR btn pressed");
    prepareLoraFrame(true);
    sendPulseFrame();
  }

  switch (deviceState) {
    case DEVICE_STATE_JOIN:
      {
        LoRaWAN.join();
        break;
      }
    case DEVICE_STATE_CYCLE:
      {
        // Only read if data is available, otherwise pass -1 for timeout handling
        int ch = Serial1.available() ? Serial1.read() : -1;
        Tele_Result res = elec_parser_add(ch);

        if (res == TELE_RESULT_ERROR) app_fail++;
        if (res == TELE_RESULT_AVAILABLE) {
          debug("elec values: available\n  %-15s %d\n", "Num-Fail", app_fail);
          debugdelay(200);  //allow lora mac to run.

          for (int i = 0; i < TELE_NUMFIELDS; i++) {
            debug("  %-15s %s\n", elec_field_name(i), elec_field_value(i));
            debugdelay(200);  //allow lora mac to run.
          }
          process_elec_message();

          Serial.printf("End processing \n");
          delay(200);
          app_fail = 0;
        }
        break;
      }
    default:
      {
        deviceState = DEVICE_STATE_CYCLE;
        break;
      }
  }
}
