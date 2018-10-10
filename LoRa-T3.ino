#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "SSD1306.h"
#include <RtcDS3231.h>

#define SCK 5   // GPIO5  -- SX1278's SCK
#define MISO 19 // GPIO19 -- SX1278's MISnO
#define MOSI 27 // GPIO27 -- SX1278's MOSI
#define SS 18   // GPIO18 -- SX1278's CS
#define RST 23  // GPIO14 -- SX1278's RESET
#define DI0 26  // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND 433E6
#define LORA_MODULE_POWER 25

#define SSD1306_ADDRESS 0x3c
#define I2C_SDA 21
#define I2C_SCL 22

RtcDS3231<TwoWire> rtc(Wire);
SSD1306 oled(SSD1306_ADDRESS, I2C_SDA, I2C_SCL);

void set_next_alarm(void)
{
    RtcDateTime now = rtc.GetDateTime();

    //Set wakeup time to every minute
    DS3231AlarmOne alarm(
        now.Day(),
        now.Hour(),
        now.Minute() + 1,
        now.Second(),
        DS3231AlarmOneControl_MinutesSecondsMatch); //MATCH MIN AND SEC

    Serial.printf("SET Alarm Time: %.2d-%.2d-%.2d\n",
                  now.Hour(),
                  now.Minute() + 1,
                  now.Second());

    rtc.SetAlarmOne(alarm);

    // DS3231AlarmOne alarm1 = rtc.GetAlarmOne();
    // Serial.printf("GET: %.2d-%.2d-%.2d\n",
    //               alarm1.Hour(),
    //               alarm1.Minute(),
    //               alarm1.Second());
}

void handware_init()
{
    // pinMode(LORA_MODULE_POWER, OUTPUT);
    // digitalWrite(LORA_MODULE_POWER, LOW);

    /*
    NOTE:
    Close these notes and you will be able to turn on the OLED display，
    For power consumption, the display will be turned off here.
    */
    // oled.init();
    // oled.flipScreenVertically();
    // oled.setFont(ArialMT_Plain_24);
    // oled.setTextAlignment(TEXT_ALIGN_CENTER);
    // delay(50);
    // oled.drawString(oled.getWidth() / 2, oled.getHeight() / 2, "TTGO");
    // oled.display();

    rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!rtc.IsDateTimeValid())
    {
        rtc.SetDateTime(compiled);
    }

    if (!rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        rtc.SetIsRunning(true);
    }

    RtcDateTime now = rtc.GetDateTime();
    if (now < compiled)
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        rtc.SetDateTime(compiled);
    }

    rtc.Enable32kHzPin(false);
    rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

    rtc.LatchAlarmsTriggeredFlags();

    set_next_alarm();
}

void send_temp_handle()
{
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DI0);
    if (!LoRa.begin(BAND))
    {
        Serial.println("Starting LoRa failed!");
        return;
    }
    RtcTemperature temp = rtc.GetTemperature();
    // temp.Print(Serial);
    LoRa.beginPacket();
    temp.Print(LoRa);
    LoRa.endPacket();
}

void wakeup_event_handle()
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
    {
        rtc.Begin();
        Serial.println("Wakeup caused by external signal using RTC_IO");
        DS3231AlarmFlag flag = rtc.LatchAlarmsTriggeredFlags();
        if (flag & DS3231AlarmFlag_Alarm1)
        {
            Serial.println("alarm one triggered");
            send_temp_handle();
            set_next_alarm();
        }
        if (flag & DS3231AlarmFlag_Alarm2)
        {
            Serial.println("alarm two triggered");
        }
    }
    break;

    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.println("Wakeup was not caused by deep sleep");
        handware_init();
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    wakeup_event_handle();
    /*
    NOTE:
    Only RTC IO can be used as a source for external wake
    source. They are pins: 0,2,4,12-15,25-27,32-39.
    */
    pinMode(LORA_MODULE_POWER, PULLUP);
    delay(50);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, LOW);
    esp_deep_sleep_start();
}

void loop()
{
}
