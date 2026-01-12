/*
  if need be, allocate more mem for sketch in:
  Arduino15\packages\esp32\hardware\esp32\1.0.0\tools\sdk\sdkconfig.h
  -> #define CONFIG_ULP_COPROC_RESERVE_MEM
  for this sketch to compile. 2048b current
*/

#include "esp32/ulp.h"
#include "ulp_main.h"
#include "WiFi.h"
#include "esp_bt.h"
#include "ulptool.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include <ESP32Servo.h>

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

RTC_DATA_ATTR int bootCount_failed = 0;

bool detectQuiet(int ms, int thresh, int samples) {
  unsigned long startTime = millis();

  while (millis() - startTime < ms) {
    int mn = 4095;
    int mx = 0;
    for (int i = 0; i < samples; i++) {
      int inputMicDigital = analogRead(34);
      if (inputMicDigital <= mn) {
        mn = inputMicDigital;
      }

      if (inputMicDigital >= mx) {
        mx = inputMicDigital;
      }
    }

    int delta = mx - mn;
    if (delta >= thresh) {
      return false;
    }
  }
  return true;
}

bool detectLoud(int ms, int thresh, int samples) {
  unsigned long startTime = millis();

  while (millis() - startTime < ms) {
    int mn = 4095;
    int mx = 0;

    for (int i = 0; i < samples; i++) {
      int inputMicDigital = analogRead(34);
      if (inputMicDigital <= mn) {
        mn = inputMicDigital;
      }

      if (inputMicDigital >= mx) {
        mx = inputMicDigital;
      }
    }

    int delta = mx - mn;
    if (delta >= thresh) {
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_OFF);
  btStop();

  int unusedPins[] = {4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 26};

  for (int i = 0; i < 13; i++) {
    pinMode(unusedPins[i], INPUT);
  }

  rtc_gpio_isolate(GPIO_NUM_32);
  rtc_gpio_isolate(GPIO_NUM_33);
  rtc_gpio_isolate(GPIO_NUM_35);
  rtc_gpio_isolate(GPIO_NUM_36);
  rtc_gpio_isolate(GPIO_NUM_39);
  
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    Serial.printf("Not ULP wakeup\n");
    init_ulp_program();
  } else {
    // main cpu code goes here
    // bool servoSwitched = false;

    //instantiate servo object
    Servo switcher;
    int pos = 180;
    const int servoPin = 25;

    //servo timers
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    switcher.setPeriodHertz(50);
    switcher.attach(servoPin, 500, 2500);

    // once hardware is completed, edit this code such that
    // it doesnt work if there's a continuous loud sound
    // as in, after wake up, listen for no noise, noise, no noise
    // fine tune the listening period for both
    // i think maybe we split it evenly; 150 ms clab, 50 ms per listen
    // but don't worry too much about it

    // increase failed bootCounts when we fail
    // idk, i might just not implement this as it could be too much work
    // need to have a working prototype more desperately than this bullshit
    if (detectQuiet(50, 1500, 50)) {
      if (detectLoud(500, 1500, 50)) {
        delay(15);
        if (detectQuiet(1000, 1500, 50)) {
          Serial.println("YOU MADE IT");
          
            // servo control
            // previous with weird sound was 45
            int beginAngle = 35;
            int degree = 0;

            for (pos = degree; pos <= beginAngle; pos += 1) {
            switcher.write(pos);
            delay(10);
            }

            // delay(200);

            for (pos = beginAngle; pos >= degree; pos -= 1) {
            switcher.write(pos);
            delay(10);
            }
          
        } else {
          Serial.println("Second quiet failed");
          ++bootCount_failed;
        }
      } else {
        Serial.println("Failed loud");
        ++bootCount_failed;
      }
    } else {
      Serial.println("Failed first quiet");
      ++bootCount_failed;
    }

    Serial.printf("Deep sleep wakeup\n");
    //Serial.printf("ULP did %d measurements since last reset\n", ulp_sample_counter & UINT16_MAX);
    Serial.printf("Thresholds:  high=%d\n", ulp_high_threshold);
    ulp_ADC_reading &= UINT16_MAX;
    //printf("Value=%d was %s threshold\n", ulp_ADC_reading, ulp_ADC_reading < ulp_low_threshold ? "below" : "above");
    Serial.printf("Volume= %d", ulp_ADC_reading);
  }
  Serial.printf("\n\nEntering deep sleep\n\n");
  // delay for debugging serial
  // delay(700);
  start_ulp_program();
  ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
  esp_deep_sleep_start();

}

void loop() {
}

static void init_ulp_program()
{

  // initialize my min and max values for volume
  ulp_lowVal = 4095;
  ulp_highVal = 0;

  esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
                                  (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  ESP_ERROR_CHECK(err);

  /* Configure ADC channel */
  /* Note: when changing channel here, also change 'adc_channel' constant
     in adc.S */
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_ulp_enable();
  ulp_high_threshold = 2200;

  /* Set ULP wake up period to x ms * 1000 */
  ulp_set_wakeup_period(0, 2 * 1000);
}

static void start_ulp_program()
{
  //crude, but reinitialize adc for ulp before starting it
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_ulp_enable();

  /* Start the program */
  esp_err_t err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
  ESP_ERROR_CHECK(err);
}
