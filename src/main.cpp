#include <Arduino.h>
#include <vl53l8cx.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>
#include "bird_active_1_time.h"
#include "twr/uwb_algorithm_twr_initiator.h"
#include "uwb_device_dw3000.h"
#include <Adafruit_BNO08x.h>
#include <WiFi.h>
#include "Audio.h"

#if defined(esp32_s3_devkitc_1)
  #define NEOPIXEL_PIN 7
  #define MODE_PIN 15
  #define timeSync_PIN 16
  #define I2S_BCLK  5
  #define I2S_LRCK  6
  #define I2S_DOUT  4
  #define M1_1  39
  #define M1_2  40
  #define M2_1  41
  #define M2_2  42
  #define DEV_I2C Wire
  #define LPN_PIN 8
  #define PWREN_PIN 3
  #define IRQ_PIN GPIO_NUM_9
  #define RST_PIN GPIO_NUM_14
  #define SS_PIN GPIO_NUM_10
  #define LDR_PIN 19
  #define BNO_SDA_PIN 1
  #define BNO_SCL_PIN 2
#endif

void print_result(VL53L8CX_ResultsData *Result);
void clear_screen(void);
void handle_cmd(uint8_t cmd);
void display_commands_banner(void);

const char* ssid = "bitworkshop";
const char* password = "saintgabriels";

const char* audioUrl = "http://vis.media-ice.musicradio.com/CapitalMP3"; 

Audio audio; 

using namespace bitstudio::uwb;
using namespace bitstudio::uwb::algorithm::twr;

TwoWayRangingInitiator twr;
UWBDevice uwb;

TwoWire I2C_BNO = TwoWire(1); 
Adafruit_BNO08x bno08x;

// Components.
VL53L8CX sensor_vl53l8cx_top(&DEV_I2C, LPN_PIN);

bool EnableAmbient = false;
bool EnableSignal = false;
uint8_t res = VL53L8CX_RESOLUTION_4X4;
char report[256];
uint8_t status;

#define NUM_PIXELS 56
Adafruit_NeoPixel strip(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ===== ตั้งค่า LDR =====
int ldrValue = 0;
int refValue = 0;
int diffTrigValue = 100; //100   LDR in 90
int diffUpdateValue = 20; //20  LDR in 20

bool trigger = false;
bool triggerGold = false;
unsigned long triggerStart = 0;
unsigned long lastRead = 0;

// ===== ตั้งค่าแสงสีทอง =====
unsigned long syncStart = 0;
const unsigned long cycleDuration = 15000;
const unsigned long goldCycleDuration = 3000;
unsigned long lastButtonPress = 0;

// ===== ตั้งค่าเสียง =====
float volume = 1;       // 0.0 (เบา) ถึง 1.0 (เต็ม)
int sample_rate_factor = 0;
uint32_t sample_rate_value = 16000;
bool birdSoundTrig = false;

// void i2s_init() {
//   i2s_driver_uninstall(I2S_NUM_0);
//   i2s_config_t i2s_config = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
//     .sample_rate = sample_rate_value,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Mono
//     .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
//     .intr_alloc_flags = 0,
//     .dma_buf_count = 8,
//     .dma_buf_len = 512,
//     .use_apll = false,
//     .tx_desc_auto_clear = true,
//     .fixed_mclk = 0
//   };

//   i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_BCLK,
//     .ws_io_num = I2S_LRCK,
//     .data_out_num = I2S_DOUT,
//     .data_in_num = I2S_PIN_NO_CHANGE
//   };

//   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//   i2s_set_pin(I2S_NUM_0, &pin_config);
//   i2s_zero_dma_buffer(I2S_NUM_0);
// }

// void playSound(float vol, float sample_rate) {
//   size_t bytesWritten;
//   int16_t sample;
//   size_t idx = 0;

//   sample_rate_value = sample_rate;
//   i2s_init();

//   Serial.printf("Playing sound: volume=%.2f\n", vol);

//   for (size_t i = 0; i < bird_active_1_time_data_len / 2; i++) {
//     memcpy(&sample, &bird_active_1_time_data[i*2], 2); // อ่าน sample 16-bit
//     sample = (int16_t)(sample * vol);                    // ปรับ volume
//     i2s_write(I2S_NUM_0, &sample, 2, &bytesWritten, portMAX_DELAY);

//     // if (i % 256 == 0) vTaskDelay(1);
//   }

//   Serial.println("Done playing.");
// }

void stoked() {
  //stoked version
  if(!trigger) {
    triggerStart = millis();
    trigger = true;
    birdSoundTrig = true;
  }

    if (trigger) {
    unsigned long elapsed = millis() - triggerStart;

    if (elapsed >= 0 && elapsed < 1000) {
      if (elapsed >= 0 && elapsed < 500) {
        digitalWrite(M1_1, HIGH);
        digitalWrite(M1_2, LOW);  
        digitalWrite(M2_1, HIGH);
        digitalWrite(M2_2, LOW);
      }
      else if (elapsed >= 500 && elapsed < 1000) {
        digitalWrite(M1_1, LOW);
        digitalWrite(M1_2, HIGH);  
        digitalWrite(M2_1, LOW);
        digitalWrite(M2_2, HIGH);
      }

      strip.setBrightness(255);
      for (int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
      }
      strip.show();
      // if(birdSoundTrig == true) {
      //   sample_rate_factor = 16000;
      //   playSound(volume, sample_rate_factor);
      //   birdSoundTrig = false;
      // }
    } 
    else if (elapsed >= 1000 && elapsed < 3000) {
      if (elapsed >= 1000 && elapsed < 1500) {
        digitalWrite(M1_1, HIGH);
        digitalWrite(M1_2, LOW);  
        digitalWrite(M2_1, HIGH);
        digitalWrite(M2_2, LOW);
      }
      else if (elapsed >= 1500 && elapsed < 2000){
        digitalWrite(M1_1, LOW);
        digitalWrite(M1_2, HIGH);
        digitalWrite(M2_1, LOW);
        digitalWrite(M2_2, HIGH);
      }
      else if (elapsed >= 2000 && elapsed < 2500){
        digitalWrite(M1_1, HIGH);
        digitalWrite(M1_2, LOW);  
        digitalWrite(M2_1, HIGH);
        digitalWrite(M2_2, LOW);
      }
      else if (elapsed >= 2500 && elapsed < 3000){
        digitalWrite(M1_1, LOW);
        digitalWrite(M1_2, HIGH);
        digitalWrite(M2_1, LOW);
        digitalWrite(M2_2, HIGH);
      }

      //comment
      // unsigned long fadeElapsed = elapsed - 1000;
      // int brightness = map(fadeElapsed, 0, 1500, 255, 0);

      // if(brightness < 0) {
      //   brightness = 0;
      // }

      // strip.setBrightness(brightness);
      // for (int i = 0; i < NUM_PIXELS; i++) {
      //   strip.setPixelColor(i, strip.Color(255, 255, 255));
      // }
      // strip.show();
    } 
    else if (elapsed >= 3000) {
      trigger = false;
      //comment
      // for (int i = 0; i < NUM_PIXELS; i++) {
      //   strip.setPixelColor(i, strip.Color(0, 0, 0));
      // }
      // strip.show();
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(timeSync_PIN, INPUT_PULLUP);

  pinMode(M1_1, OUTPUT);
  pinMode(M1_2, OUTPUT);
  pinMode(M2_1, OUTPUT);
  pinMode(M2_2, OUTPUT);

  digitalWrite(M1_1, LOW);
  digitalWrite(M1_2, LOW);
  digitalWrite(M2_1, LOW);
  digitalWrite(M2_2, LOW);

    Serial.println("\n--- ESP32 Capital FM Streamer ---");

    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT); 
    audio.setVolume(100);

    Serial.printf("Attempting to connect to stream: %s\n", audioUrl);
    audio.connecttohost(audioUrl);

  // Enable PWREN pin if present
  if (PWREN_PIN >= 0) {
    pinMode(PWREN_PIN, OUTPUT);
    digitalWrite(PWREN_PIN, HIGH);
    delay(10);
  }

  // Initialize I2C bus.
  DEV_I2C.begin();
  I2C_BNO.begin(BNO_SDA_PIN, BNO_SCL_PIN, 100000);

  // Configure VL53L8CX component.
  sensor_vl53l8cx_top.begin();
  status = sensor_vl53l8cx_top.init();

  // Start Measurements
  status = sensor_vl53l8cx_top.start_ranging();

  strip.begin();
  strip.show();

  // i2s_init();

  uwb.setPins(IRQ_PIN, RST_PIN, SS_PIN);
  twr.bindDevice(&uwb);
  twr.setDeviceId("f001", 4);
  twr.init();
  Serial.println("Done init");

  if (!bno08x.begin_I2C(0x4A, &I2C_BNO)) {   // address 0x4A or 0x4B
    Serial.println("Failed to find BNO08x chip!");
    while (1) delay(10);
  }

  Serial.println("BNO08x Found!");

  // เปิดการอ่าน Rotation Vector
  if (!bno08x.enableReport(SH2_ROTATION_VECTOR)) {
    Serial.println("Could not enable rotation vector report");
  }
}

TwrInitiatorData data[16];
uint8_t dataCount = 0;

char* targets[] = {
  "f002"
};

void loop()
{
  audio.loop();

  sh2_SensorValue_t sensorValue;

  if (bno08x.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      Serial.print("Quat: ");
      Serial.print(sensorValue.un.rotationVector.real, 4);
      Serial.print(", ");
      Serial.print(sensorValue.un.rotationVector.i, 4);
      Serial.print(", ");
      Serial.print(sensorValue.un.rotationVector.j, 4);
      Serial.print(", ");
      Serial.println(sensorValue.un.rotationVector.k, 4);
    }
  }

  // VL53L8CX_ResultsData Results;
  // uint8_t NewDataReady = 0;

  // status = sensor_vl53l8cx_top.check_data_ready(&NewDataReady);
  // if ((!status) && (NewDataReady != 0)) {
  //   status = sensor_vl53l8cx_top.get_ranging_data(&Results);
  //   print_result(&Results);
  // }

  stoked(); 

  // twr.setTargets(targets, 1);
  // dataCount = twr.ranging(data, 16);
  // if (dataCount > 0)
  // {
  //   for (uint8_t i=0; i < dataCount; i++)
  //   {
  //     Serial.printf("Range with %s is %s result: %f\n", data[i].destinationId, data[i].success ? "success" : "failed", data[i].distance);
  //   }
  // }
  Serial.print("LDR Value: ");
  Serial.println(analogRead(LDR_PIN));

  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead >= 200) {
    lastSensorRead = millis();

    VL53L8CX_ResultsData Results;
    uint8_t NewDataReady = 0;

    status = sensor_vl53l8cx_top.check_data_ready(&NewDataReady);
    if ((!status) && (NewDataReady != 0)) {
      status = sensor_vl53l8cx_top.get_ranging_data(&Results);
      print_result(&Results);
    }

    twr.setTargets(targets, 1);
    dataCount = twr.ranging(data, 16);
    if (dataCount > 0)
    {
      for (uint8_t i=0; i < dataCount; i++)
      {
        Serial.printf("Range with %s is %s result: %f\n", data[i].destinationId, data[i].success ? "success" : "failed", data[i].distance);
      }
    }
  }
}

void print_result(VL53L8CX_ResultsData *Result)
{
  int8_t i, j, k;
  uint8_t l, zones_per_line;
  uint8_t number_of_zones = res;

  zones_per_line = (number_of_zones == 16) ? 4 : 8;

  display_commands_banner();

  Serial.print("Cell Format :\n\n");

  for (l = 0; l < VL53L8CX_NB_TARGET_PER_ZONE; l++) {
    snprintf(report, sizeof(report), " %20s : %20s\n", "Distance [mm]", "Status");
    Serial.print(report);

    if (EnableAmbient || EnableSignal) {
      snprintf(report, sizeof(report), " %20s : %20s\n", "Signal [kcps/spad]", "Ambient [kcps/spad]");
      Serial.print(report);
    }
  }

  Serial.print("\n\n");

  for (j = 0; j < number_of_zones; j += zones_per_line) {
    for (i = 0; i < zones_per_line; i++) {
      Serial.print(" -----------------");
    }
    Serial.print("\n");

    for (i = 0; i < zones_per_line; i++) {
      Serial.print("|                 ");
    }
    Serial.print("|\n");

    for (l = 0; l < VL53L8CX_NB_TARGET_PER_ZONE; l++) {
      // Print distance and status
      for (k = (zones_per_line - 1); k >= 0; k--) {
        if (Result->nb_target_detected[j + k] > 0) {
          snprintf(report, sizeof(report), "| %5ld  :  %5ld ",
                      (long)Result->distance_mm[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l],
                      (long)Result->target_status[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l]);
          Serial.print(report);
        } else {
          snprintf(report, sizeof(report), "| %5s  :  %5s ", "X", "X");
          Serial.print(report);
        }
      }
      Serial.print("|\n");

      if (EnableAmbient || EnableSignal) {
        // Print Signal and Ambient
        for (k = (zones_per_line - 1); k >= 0; k--) {
          if (Result->nb_target_detected[j + k] > 0) {
            if (EnableSignal) {
              snprintf(report, sizeof(report), "| %5ld  :  ", (long)Result->signal_per_spad[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l]);
              Serial.print(report);
            } else {
              snprintf(report, sizeof(report), "| %5s  :  ", "X");
              Serial.print(report);
            }
            if (EnableAmbient) {
              snprintf(report, sizeof(report), "%5ld ", (long)Result->ambient_per_spad[j + k]);
              Serial.print(report);
            } else {
              snprintf(report, sizeof(report), "%5s ", "X");
              Serial.print(report);
            }
          } else {
            snprintf(report, sizeof(report), "| %5s  :  %5s ", "X", "X");
            Serial.print(report);
          }
        }
        Serial.print("|\n");
      }
    }
  }
  for (i = 0; i < zones_per_line; i++) {
    Serial.print(" -----------------");
  }
  Serial.print("\n");
}

void toggle_resolution(void)
{
  status = sensor_vl53l8cx_top.stop_ranging();

  switch (res) {
    case VL53L8CX_RESOLUTION_4X4:
      res = VL53L8CX_RESOLUTION_8X8;
      break;

    case VL53L8CX_RESOLUTION_8X8:
      res = VL53L8CX_RESOLUTION_4X4;
      break;

    default:
      break;
  }
  status = sensor_vl53l8cx_top.set_resolution(res);
  status = sensor_vl53l8cx_top.start_ranging();
}

void toggle_signal_and_ambient(void)
{
  EnableAmbient = (EnableAmbient) ? false : true;
  EnableSignal = (EnableSignal) ? false : true;
}

void clear_screen(void)
{
  // snprintf(report, sizeof(report), "%c[2J", 27); /* 27 is ESC command */
  snprintf(report, sizeof(report), "");
  Serial.print(report);
}

void display_commands_banner(void)
{
  // snprintf(report, sizeof(report), "%c[2H", 27);
  snprintf(report, sizeof(report), "");
  Serial.print(report);

  Serial.print("53L8A1 Simple Ranging demo application\n");
  Serial.print("--------------------------------------\n\n");

  Serial.print("Use the following keys to control application\n");
  Serial.print(" 'r' : change resolution\n");
  Serial.print(" 's' : enable signal and ambient\n");
  Serial.print(" 'c' : clear screen\n");
  Serial.print("\n");
}

void handle_cmd(uint8_t cmd)
{
  switch (cmd) {
    case 'r':
      toggle_resolution();
      clear_screen();
      break;

    case 's':
      toggle_signal_and_ambient();
      clear_screen();
      break;

    case 'c':
      clear_screen();
      break;

    default:
      break;
  }
}

void audio_info(const char *info){
    Serial.print("INFO: "); Serial.println(info);
}

void audio_eof_mp3(const char *info){
    Serial.print("INFO: "); Serial.println(info);
    audio.connecttohost(audioUrl);
}


















