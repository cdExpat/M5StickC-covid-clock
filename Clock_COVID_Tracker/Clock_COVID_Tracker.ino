#include <M5StickC.h>
//#include "Seeed_BME280.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Arduino.h> 
#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <ArduinoHttpClient.h>

#define Addr 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;
//BME280 bme280;
WiFiClientSecure client;
HttpClient http(client,"covid19.who.int", 443);

RTC_TimeTypeDef RTC_TimeStruct; 
RTC_DateTypeDef RTC_DateStruct;

String s1 = "";
String week_str, month_str, time_str, sec_str;
String old_temp, old_tvoc, old_eco2;

int readcounter = 0, old_sec;
int bat_counter = 0;
int bright_state = 8 , show_country = 0;
int clock_refresh=1;
int update_wifi;

String NETW[]= {"*****","*****","*****"};
String PASS[]= {"*****","*****","*****"};
int network_count=3;

String countries[]= {"Poland", "Portugal", "France", "Germany", "Italy","Spain", "The United Kingdom"};
const int arrLen = sizeof(countries) / sizeof(countries[0]);
RTC_DATA_ATTR int infected[arrLen], deaths[arrLen];
RTC_DATA_ATTR int reboot, firstboot, menu;
bool redraw_clock, battery_refresh, temp_refresh;

void setup() {
  M5.begin();
  M5.Axp.ScreenBreath(bright_state);
  if (reboot==0) {
    firstboot=1;
  } else {
    firstboot=0;
  }
  M5.Lcd.setRotation(3);
  setCpuFrequencyMhz(80);
}

void loop() {
  if (firstboot == 1) {
    connect_wifi();
  } 
  if (reboot==1) {
    reboot=0;
    menu=1;
    redraw_clock = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(10);
  }
  if (redraw_clock) {
    draw_time_screen();
  }
  if (battery_refresh) {
    draw_battery_status();
  }
  if (temp_refresh) {
    draw_temp_screen();
  }
  if( M5.BtnA.wasReleased() ) {
    menu_loop();
  } 
  if( M5.BtnB.wasReleased() ) {
    if (update_wifi==1) {
      connect_wifi();
    } else if ((bright_state==15)) {
      bright_state=8;
    } else {
      M5.Axp.ScreenBreath(bright_state);
      bright_state++;
    }
  } 
  if(M5.Axp.GetBtnPress() == 0x02) {
    M5.Lcd.fillScreen(BLACK);
    M5.Axp.ScreenBreath(0);
    M5.Axp.SetLDO2(false);
    M5.Axp.SetLDO3(false);
    reboot=1;
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_37,LOW);
    esp_deep_sleep_start();
  }
  delay(200);
  M5.update(); 
}

void connect_wifi() {
  setCpuFrequencyMhz(80);
  print_msg("Searching networks",25);
  int n = WiFi.scanNetworks();
  if (n == 0) {
    print_msg("no network found",30);
  } else {
    for (int i = 0; i < n; ++i) {
      for (int My_SSID=0; My_SSID<network_count; ++My_SSID) {
        if ((WiFi.SSID(i))==NETW[My_SSID]) {
          print_msg(WiFi.SSID(i), 20);
          WiFi.begin(NETW[My_SSID].c_str(), PASS[My_SSID].c_str());
          while (WiFi.status() != WL_CONNECTED) {
            M5.Lcd.setCursor(37, 50);
            M5.Lcd.setTextSize(1);
            M5.Lcd.print("Please Wait...");
          }
        delay(2000);
        break;
        }
      }
    }
  }
  request_data();
  process_data();
  clock_refresh=1;
  redraw_clock = true;
  menu=1;
  firstboot=0;
  update_wifi=0;
  setCpuFrequencyMhz(10);
}

void menu_loop() {
  if (menu == 0) {
    setCpuFrequencyMhz(10);
    M5.Axp.ScreenBreath(8);
    update_wifi=0;
    clock_refresh=1;
    draw_time_screen();
    redraw_clock = true;
    menu=1;
  } else if (menu == 1) {
    //temperature menu
    menu = 2;
    redraw_clock=false;
    unsigned status;
    status = bme.begin(Addr,&Wire);
    //status = bme280.init();
    if (!status) {
        Serial.println("No BME280 found");
        menu_loop();
    } else {
      bat_counter=10;
      temp_refresh=true;
      M5.Lcd.fillScreen(ORANGE);
      M5.Lcd.setTextColor(TFT_BLACK);
      M5.Lcd.drawString("Temp", 1, 5, 1);
      M5.Lcd.drawString("Humid", 1, 40, 1);
      draw_temp_screen();
    }  
  } else if (menu == 2) {
    temp_refresh=false;
    draw_covid_screen(show_country);
    if (show_country==(arrLen-1)) {
      menu = 3;
      show_country=0;
    } else {
      show_country++;
    }
  } else if (menu == 3) {
    bat_counter=10;
    battery_refresh=true;
    draw_battery_status();
    menu = 4;
    show_country=0;
  } else if(menu == 4) {
    update_wifi=1;
    battery_refresh= false;
    draw_update_screen();
    menu =0;
  }
}

void draw_covid_screen(int country){
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_GREEN);
  if (countries[country]=="The United Kingdom") {
    M5.Lcd.drawCentreString("UK",80,3,4);
  } else {
    M5.Lcd.drawCentreString(countries[country], 80, 3, 4);
  }
  M5.Lcd.setTextColor(YELLOW);
  String s_infected = String(infected[country]);
  M5.Lcd.drawCentreString(s_infected,80,30,4);
  String s_deaths = String(deaths[country]);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.drawCentreString(s_deaths,80,55,4);
}

void request_data() {
  s1="";
  print_msg("Requesting data",20);
  M5.Rtc.GetData(&RTC_DateStruct);
  int err = 0;
  err = http.get("/WHO-COVID-19-global-table-data.csv");
  if (err == 0) {
    print_msg("Requesting ok",20);
    err = http.responseStatusCode();
    if (err >= 0) {
      char c;
      while (readcounter<6000) {
        c = http.read();
        s1 = s1 + c;
        readcounter++;
      }
    } else {    
      print_msg("Response failed",20);
      delay(2000);
    }
  } else {
    print_msg("Connection failed",20);
    delay(2000);
  }
  http.stop();
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

void process_data() {
  int loop=0;
  String s2="";
  while (loop<arrLen) {
    int place = s1.indexOf(countries[loop]);
    String tempString = s1.substring(place, place+100);
    readcounter=0;
    while (readcounter<11) {
      readcounter++;
      place=tempString.indexOf(",");
      s2=tempString.substring(place+1);
      tempString=s2;
      if (readcounter == 6 ) {
        place=tempString.indexOf(",");
        s2=tempString.substring(0,place);
        infected[loop] = s2.toInt();
      }
    }
    place=tempString.indexOf(",");
    s2=tempString.substring(0,place);
    deaths[loop] = s2.toInt();
    loop++;
  }  
}

void print_msg(String msg, int y_pos) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawCentreString(msg, 80, y_pos, 2);
}

void draw_time_screen(){
  if (clock_refresh==1) {
    M5.Lcd.fillScreen(TFT_ORANGE);
    clock_refresh=0;
    old_sec= sec_str.toInt();
  }
  M5.Lcd.setTextColor(TFT_BLACK, TFT_ORANGE);
  M5.Lcd.setTextSize(2);
  M5.Rtc.GetData(&RTC_DateStruct);
  get_time_str();
  get_week_str();
  get_month_str();
  String cur_date = String(RTC_DateStruct.Date)+" "+month_str+" "+String(RTC_DateStruct.Year);
  if ((sec_str.toInt()-old_sec) == 0 || (sec_str.toInt()-old_sec) == 1 ) {
    M5.Lcd.drawString(sec_str, 130, 24, 1);
  } else {
    M5.Lcd.drawCentreString(week_str, 80, 2, 1);
    M5.Lcd.drawString(time_str, 0, 20, 4);
    M5.Lcd.drawString(sec_str, 130, 24, 1);
    M5.Lcd.drawCentreString(cur_date, 80, 65, 1);
  }
  old_sec = sec_str.toInt(); 
}

void get_time_str() {
  M5.Rtc.GetBm8563Time();
  int hr = M5.Rtc.Hour;
  if (hr<10) {
    time_str="0"+String(hr)+":";
  } else {
    time_str=String(hr)+":";
  }
  int mi = M5.Rtc.Minute;
  if (mi<10) {
    time_str=time_str+"0"+String(mi);
  } else {
    time_str=time_str+String(mi);
  }
  int sec = M5.Rtc.Second;
  if (sec<10) {
    sec_str="0"+String(sec);
  } else {
    sec_str=String(sec);
  }
}
  
void get_week_str() {
  int wkday = RTC_DateStruct.WeekDay;
  switch (wkday) {
    case 0: { week_str = "Sunday"; break; }
    case 1: { week_str = "Monday"; break; }
    case 2: { week_str = "Tuesday"; break; }
    case 3: { week_str = "Wednesday"; break; }
    case 4: { week_str = "Thursday"; break; }
    case 5: { week_str = "Friday"; break; }
    case 6: { week_str = "Saterday"; break; }
  } 
}

void get_month_str() {
  int month_nr = RTC_DateStruct.Month;
  switch (month_nr) {
    case 1: { month_str = "Jan"; break; }
    case 2: { month_str = "Feb"; break; }
    case 3: { month_str = "Mar"; break; }
    case 4: { month_str = "Apr"; break; }
    case 5: { month_str = "May"; break; }
    case 6: { month_str = "Jun"; break; }
    case 7: { month_str = "Jul"; break; }
    case 8: { month_str = "Aug"; break; }
    case 9: { month_str = "Sep"; break; }
    case 10: { month_str = "Oct"; break; }
    case 11: { month_str = "Nov"; break; }
    case 12: { month_str = "Dec"; break; }
  }
}

void draw_battery_status() {
  if (bat_counter==10) {
    M5.Lcd.fillScreen(ORANGE);
    M5.Lcd.setTextColor(TFT_BLACK);
    String Bat_V_Status="Bat: "+String(M5.Axp.GetBatVoltage(),2)+"V";
    String Bat_A_Status="Cur: "+String(M5.Axp.GetBatCurrent(),0)+"mA";
    M5.Lcd.drawCentreString(Bat_V_Status, 80, 10, 4);
    M5.Lcd.drawCentreString(Bat_A_Status, 80, 44, 4);
    bat_counter=0;
    } else {
      bat_counter++;
    }
}

void draw_temp_screen() {
    if (bat_counter==10) {
      setCpuFrequencyMhz(25);
      M5.Lcd.setTextColor(TFT_BLACK);
      String temp_Value=String(bme.readTemperature(),1);
      String humi_Value=String(bme.readHumidity(),0);
      //String temp_Value=String(bme280.getTemperature(),1);
      //String humi_Value=String(bme280.getHumidity());
      M5.Lcd.fillRect(89, 7, 70 ,65, ORANGE);
      M5.Lcd.drawString(temp_Value, 90, 5, 2);
      M5.Lcd.drawString(humi_Value, 90, 40, 2);
      setCpuFrequencyMhz(10);
      bat_counter=0;
    } else {
      bat_counter++;
    }
}

void draw_update_screen() {
  M5.Lcd.fillScreen(TFT_ORANGE);
  M5.Lcd.setTextColor(TFT_BLACK);
  String us1="Press button";
  String us2="B for update";
  M5.Lcd.drawCentreString(us1, 80, 8, 4);
  M5.Lcd.drawCentreString(us2, 80, 42, 4);
}
