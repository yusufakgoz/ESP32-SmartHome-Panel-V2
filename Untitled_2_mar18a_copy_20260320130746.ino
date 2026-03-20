// 1. AĞ VE JSON KÜTÜPHANELERİ
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 2. BULUT VE SECRETS
#include "arduino_secrets.h"
#include "thingProperties.h"

// 3. EKRAN, DOKUNMATİK VE SENSÖR
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h> 
#include <lvgl.h>
#include <DHT.h>
#include <time.h> 

// --- DOKUNMATİK PİNLERİ ---
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass mySpi = SPIClass(HSPI); 
XPT2046_Touchscreen ts(XPT2046_CS); 

#define DHTPIN 22     
#define DHTTYPE DHT22 
DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// --- GLOBAL SİSTEM DEĞİŞKENLERİ ---
bool is_dark_mode = false; 
bool ip_gosterildi = false;
int gecerli_hava_id = 800; // Tema değişirken ikon rengi için

// --- ARAYÜZ OBJELERİ ---
lv_obj_t * tv; 
lv_obj_t * tile1; lv_obj_t * tile2; lv_obj_t * tile3; lv_obj_t * tile4; 

lv_obj_t * label_saat; lv_obj_t * label_saat2; lv_obj_t * label_saat3; lv_obj_t * label_saat4;
lv_obj_t * label_wifi; lv_obj_t * label_sicaklik; 
lv_obj_t * bx_w; 

// Hava Durumu Kutusunun Parçaları (YENİ)
lv_obj_t * label_hava_icon;
lv_obj_t * label_hava_temp;
lv_obj_t * label_hava_desc;

lv_obj_t * forecast_day_labels[5];   
lv_obj_t * forecast_temp_labels[5];  
lv_obj_t * forecast_data_labels[5];  

lv_obj_t * sw1; lv_obj_t * sw2; lv_obj_t * sw3; lv_obj_t * sw4; lv_obj_t * sw5;

lv_obj_t * label_theme; lv_obj_t * label_br; lv_obj_t * label_ip;
static lv_style_t style_box; 

unsigned long sonOkumaZamani = 0;
unsigned long sonSaatGuncelleme = 0;
unsigned long sonHavaGuncelleme = 0;
bool ilkHavaDurumuCekildi = false;
bool need_forecast_update = false; 

String openWeatherURL = "http://api.openweathermap.org/data/2.5/weather?q=Istanbul,TR&appid=893ef4a1188c96026c9ec4bb52135949&units=metric&lang=tr";
String forecastURL = "http://api.openweathermap.org/data/2.5/forecast?q=Istanbul,TR&appid=893ef4a1188c96026c9ec4bb52135949&units=metric&lang=tr";

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1); uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite(); tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true); tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z > 500) {
      int x = map(p.x, 200, 3700, 0, 320); int y = map(p.y, 240, 3800, 0, 240);
      x = constrain(x, 0, 320); y = constrain(y, 0, 240);
      data->state = LV_INDEV_STATE_PR; data->point.x = x; data->point.y = y;
    } else { data->state = LV_INDEV_STATE_REL; }
  } else { data->state = LV_INDEV_STATE_REL; }
}

String fixTurkishChars(String text) {
  text.replace("ç", "c"); text.replace("Ç", "C"); text.replace("ğ", "g"); text.replace("Ğ", "G");
  text.replace("ı", "i"); text.replace("İ", "I"); text.replace("ö", "o"); text.replace("Ö", "O");
  text.replace("ş", "s"); text.replace("Ş", "S"); text.replace("ü", "u"); text.replace("Ü", "U");
  return text;
}

// ==============================================
// WIDGET MOTORU 
// ==============================================
void update_widget_ui(lv_obj_t * widget, const char* name, bool is_on) {
    lv_obj_t * lbl_text = lv_obj_get_child(widget, 0);
    lv_obj_t * lbl_icon = lv_obj_get_child(widget, 1);
    lv_obj_set_style_border_width(widget, 1, 0);

    if (is_dark_mode) {
        lv_obj_set_style_bg_color(widget, lv_color_hex(is_on ? 0x48484A : 0x2C2C2E), 0);
        lv_obj_set_style_border_color(widget, lv_color_hex(0x48484A), 0); 
        String c_text = is_on ? "#FFCC00" : "#AAAAAA";
        lv_label_set_text_fmt(lbl_text, "%s %s#", c_text.c_str(), name); 
        lv_label_set_text_fmt(lbl_icon, "%s %s#", c_text.c_str(), LV_SYMBOL_CHARGE);
    } else {
        lv_obj_set_style_bg_color(widget, lv_color_hex(is_on ? 0xFFFFFF : 0xF9F9F9), 0);
        lv_obj_set_style_border_color(widget, lv_color_hex(0xD1D1D6), 0); 
        String c_text = is_on ? "#FFCC00" : "#8E8E93";
        lv_label_set_text_fmt(lbl_text, "%s %s#", c_text.c_str(), name); 
        lv_label_set_text_fmt(lbl_icon, "%s %s#", c_text.c_str(), LV_SYMBOL_CHARGE);
    }
}

lv_obj_t* create_iphone_widget(lv_obj_t* parent, int x, int y, const char* text) {
    lv_obj_t * widget = lv_obj_create(parent);
    lv_obj_remove_style_all(widget); 
    lv_obj_set_size(widget, 90, 80);
    lv_obj_align(widget, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_add_flag(widget, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(widget, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_radius(widget, 15, 0);
    lv_obj_set_style_bg_opa(widget, LV_OPA_COVER, 0);
    
    lv_obj_t * lbl_text = lv_label_create(widget);
    lv_label_set_recolor(lbl_text, true); 
    lv_obj_align(lbl_text, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t * lbl_icon = lv_label_create(widget);
    lv_label_set_recolor(lbl_icon, true); 
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_icon, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    update_widget_ui(widget, text, false);
    return widget;
}

// ==============================================
// TEMA UYGULAMA MOTORU 
// ==============================================
void apply_theme() {
    lv_obj_set_style_bg_color(tv, lv_color_hex(is_dark_mode ? 0x121212 : 0xF2F2F7), 0);
    
    lv_style_set_bg_color(&style_box, lv_color_hex(is_dark_mode ? 0x2C2C2E : 0xFFFFFF));
    lv_style_set_text_color(&style_box, lv_color_hex(is_dark_mode ? 0xFFFFFF : 0x1C1C1E));
    lv_obj_report_style_change(&style_box); 

    lv_color_t txt_c = lv_color_hex(is_dark_mode ? 0xFFFFFF : 0x1C1C1E);
    lv_obj_set_style_text_color(label_saat2, txt_c, 0);
    lv_obj_set_style_text_color(label_saat3, txt_c, 0);
    lv_obj_set_style_text_color(label_saat4, txt_c, 0);
    lv_obj_set_style_text_color(label_theme, txt_c, 0);
    lv_obj_set_style_text_color(label_br, txt_c, 0);
    lv_obj_set_style_text_color(label_ip, txt_c, 0);

    update_widget_ui(sw1, "Lamba 1", lamba1); update_widget_ui(sw2, "Lamba 2", lamba2);
    update_widget_ui(sw3, "Lamba 3", lamba3); update_widget_ui(sw4, "Lamba 4", lamba4);
    update_widget_ui(sw5, "Lamba 5", lamba5);

    sonOkumaZamani = 0; 
    sonHavaGuncelleme = 0;
    if(ilkHavaDurumuCekildi) need_forecast_update = true;
}

// --- EVENTLER ---
static void event_sw1(lv_event_t * e) { lamba1 = !lamba1; update_widget_ui(sw1, "Lamba 1", lamba1); }
static void event_sw2(lv_event_t * e) { lamba2 = !lamba2; update_widget_ui(sw2, "Lamba 2", lamba2); }
static void event_sw3(lv_event_t * e) { lamba3 = !lamba3; update_widget_ui(sw3, "Lamba 3", lamba3); }
static void event_sw4(lv_event_t * e) { lamba4 = !lamba4; update_widget_ui(sw4, "Lamba 4", lamba4); }
static void event_sw5(lv_event_t * e) { lamba5 = !lamba5; update_widget_ui(sw5, "Lamba 5", lamba5); }

static void weather_box_event_cb(lv_event_t * e) { lv_obj_set_tile(tv, tile3, LV_ANIM_ON); need_forecast_update = true; }

static void theme_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    is_dark_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    apply_theme(); 
}

static void slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int brightness = lv_slider_get_value(slider);
    int pwm_val = map(brightness, 5, 100, 10, 255);
    ledcWrite(21, pwm_val); 
}

static void reboot_event_cb(lv_event_t * e) { ESP.restart(); }

void fetch_5day_forecast() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http; http.begin(forecastURL); int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            JsonDocument filter;
            filter["list"][0]["dt"] = true; filter["list"][0]["main"]["temp"] = true; filter["list"][0]["weather"][0]["description"] = true;
            JsonDocument doc; DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
            
            if (!error) {
                String c_txt = is_dark_mode ? "#FFFFFF" : "#1C1C1E"; 
                
                for(int i=0; i<5; i++) {
                    int idx = i * 8; if(idx >= doc["list"].size()) break;
                    double temp = doc["list"][idx]["main"]["temp"]; int temp_int = round(temp);
                    String desc = doc["list"][idx]["weather"][0]["description"]; long dt = doc["list"][idx]["dt"];
                    desc = fixTurkishChars(desc);
                    
                    struct tm *ptm; time_t rawtime = dt; ptm = localtime(&rawtime);
                    const char* days[] = {"Paz", "Pzt", "Sal", "Car", "Per", "Cum", "Cmt"};
                    String dayName = days[ptm->tm_wday];
                    
                    String day_text = "#007AFF " + dayName + "#";
                    lv_label_set_text(forecast_day_labels[i], day_text.c_str());

                    String temp_text = c_txt + " " + String(temp_int) + "C#";
                    lv_label_set_text(forecast_temp_labels[i], temp_text.c_str());

                    String data_text = "#8E8E93 - " + desc + "#";
                    lv_label_set_text(forecast_data_labels[i], data_text.c_str());
                }
            }
        }
        http.end();
    }
}

void setup() {
  Serial.begin(115200);
  delay(1500); 

  ledcAttach(21, 5000, 8); 
  ledcWrite(21, 255);     

  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi); ts.setRotation(1); 

  dht.begin();
  tft.begin(); tft.setRotation(1);
  tft.invertDisplay(true); 

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth; disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER; indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // --- STİLLER ---
  static lv_style_t style_dev_saat; lv_style_init(&style_dev_saat); lv_style_set_text_font(&style_dev_saat, &lv_font_montserrat_48); 
  static lv_style_t style_orta_font; lv_style_init(&style_orta_font); lv_style_set_text_font(&style_orta_font, &lv_font_montserrat_24); 
  
  lv_style_init(&style_box); lv_style_set_bg_color(&style_box, lv_color_hex(0xFFFFFF)); 
  lv_style_set_radius(&style_box, 15); lv_style_set_border_width(&style_box, 0); lv_style_set_text_color(&style_box, lv_color_hex(0x1C1C1E));

  // --- SAYFALAR ---
  tv = lv_tileview_create(lv_scr_act());
  lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF); 
  lv_obj_set_style_bg_color(tv, lv_color_hex(0xF2F2F7), 0);
  
  tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_ALL); 
  tile2 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_ALL); 
  tile3 = lv_tileview_add_tile(tv, 0, 1, LV_DIR_ALL); 
  tile4 = lv_tileview_add_tile(tv, 2, 0, LV_DIR_LEFT); 

  // ==============================================
  // SAYFA 1: ANA EKRAN
  // ==============================================
  label_wifi = lv_label_create(tile1); lv_obj_add_style(label_wifi, &style_orta_font, 0); lv_label_set_recolor(label_wifi, true);
  lv_label_set_text(label_wifi, "#007AFF " LV_SYMBOL_WIFI "#"); lv_obj_align(label_wifi, LV_ALIGN_TOP_RIGHT, -10, 10);

  lv_obj_t * bx_time = lv_obj_create(tile1); lv_obj_add_style(bx_time, &style_box, 0); lv_obj_set_size(bx_time, 270, 70); lv_obj_align(bx_time, LV_ALIGN_TOP_LEFT, 10, 10);
  label_saat = lv_label_create(bx_time); lv_obj_add_style(label_saat, &style_dev_saat, 0); lv_label_set_text(label_saat, "--:--"); lv_obj_center(label_saat);

  lv_obj_t * bx_s = lv_obj_create(tile1); lv_obj_add_style(bx_s, &style_box, 0); lv_obj_set_size(bx_s, 145, 140); lv_obj_align(bx_s, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  label_sicaklik = lv_label_create(bx_s); lv_obj_add_style(label_sicaklik, &style_orta_font, 0); lv_label_set_recolor(label_sicaklik, true);
  lv_label_set_text(label_sicaklik, "#FF9500 " LV_SYMBOL_HOME "#\n..."); lv_obj_set_width(label_sicaklik, 130); lv_obj_set_style_text_align(label_sicaklik, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(label_sicaklik);

  // DIŞ HAVA DURUMU KUTUSU (KATMANLARA BÖLÜNDÜ)
  bx_w = lv_obj_create(tile1); lv_obj_add_style(bx_w, &style_box, 0); lv_obj_set_size(bx_w, 145, 140); lv_obj_align(bx_w, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_flag(bx_w, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(bx_w, weather_box_event_cb, LV_EVENT_LONG_PRESSED, NULL);

  // Katman 1: İkon (Sabit Üstte)
  label_hava_icon = lv_label_create(bx_w); lv_obj_add_style(label_hava_icon, &style_orta_font, 0); lv_label_set_recolor(label_hava_icon, true);
  lv_label_set_text(label_hava_icon, "#007AFF " LV_SYMBOL_GPS "#"); lv_obj_align(label_hava_icon, LV_ALIGN_TOP_MID, 0, 5);

  // Katman 2: Derece (Sabit Ortada)
  label_hava_temp = lv_label_create(bx_w); lv_obj_add_style(label_hava_temp, &style_orta_font, 0); lv_label_set_recolor(label_hava_temp, true);
  lv_label_set_text(label_hava_temp, "..."); lv_obj_align(label_hava_temp, LV_ALIGN_CENTER, 0, 0);

  // Katman 3: Yazı (Akan Altta)
  label_hava_desc = lv_label_create(bx_w); lv_obj_add_style(label_hava_desc, &style_orta_font, 0); lv_label_set_recolor(label_hava_desc, true);
  lv_obj_set_width(label_hava_desc, 130); lv_label_set_long_mode(label_hava_desc, LV_LABEL_LONG_SCROLL_CIRCULAR); // KAYAN YAZI
  lv_obj_set_style_text_align(label_hava_desc, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(label_hava_desc, "YUKLENIYOR..."); lv_obj_align(label_hava_desc, LV_ALIGN_BOTTOM_MID, 0, -5);

  // ==============================================
  // SAYFA 2: KONTROL PANELİ
  // ==============================================
  lv_obj_t * p_head = lv_label_create(tile2); lv_obj_add_style(p_head, &style_orta_font, 0); lv_label_set_recolor(p_head, true); 
  lv_label_set_text(p_head, "#007AFF " LV_SYMBOL_CHARGE " AYDINLATMALAR#"); lv_obj_align(p_head, LV_ALIGN_TOP_LEFT, 15, 10);

  label_saat2 = lv_label_create(tile2); lv_obj_add_style(label_saat2, &style_orta_font, 0); lv_obj_set_style_text_color(label_saat2, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_saat2, "--:--"); lv_obj_align(label_saat2, LV_ALIGN_TOP_RIGHT, -10, 10);

  sw1 = create_iphone_widget(tile2, 15,  50, "Lamba 1"); lv_obj_add_event_cb(sw1, event_sw1, LV_EVENT_CLICKED, NULL);
  sw2 = create_iphone_widget(tile2, 115, 50, "Lamba 2"); lv_obj_add_event_cb(sw2, event_sw2, LV_EVENT_CLICKED, NULL);
  sw3 = create_iphone_widget(tile2, 215, 50, "Lamba 3"); lv_obj_add_event_cb(sw3, event_sw3, LV_EVENT_CLICKED, NULL);
  sw4 = create_iphone_widget(tile2, 65,  145, "Lamba 4"); lv_obj_add_event_cb(sw4, event_sw4, LV_EVENT_CLICKED, NULL);
  sw5 = create_iphone_widget(tile2, 165, 145, "Lamba 5"); lv_obj_add_event_cb(sw5, event_sw5, LV_EVENT_CLICKED, NULL);

  // ==============================================
  // SAYFA 3: 5 GÜNLÜK TAHMİN
  // ==============================================
  lv_obj_t * f_head = lv_label_create(tile3); lv_obj_add_style(f_head, &style_orta_font, 0); lv_label_set_recolor(f_head, true); 
  lv_label_set_text(f_head, "#007AFF " LV_SYMBOL_GPS " TAHMINLER#"); lv_obj_align(f_head, LV_ALIGN_TOP_LEFT, 15, 10);

  label_saat3 = lv_label_create(tile3); lv_obj_add_style(label_saat3, &style_orta_font, 0); lv_obj_set_style_text_color(label_saat3, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_saat3, "--:--"); lv_obj_align(label_saat3, LV_ALIGN_TOP_RIGHT, -10, 10);

  for(int i=0; i<5; i++) {
      forecast_day_labels[i] = lv_label_create(tile3); lv_obj_add_style(forecast_day_labels[i], &style_orta_font, 0); lv_label_set_recolor(forecast_day_labels[i], true);
      lv_obj_set_width(forecast_day_labels[i], 60); lv_label_set_long_mode(forecast_day_labels[i], LV_LABEL_LONG_CLIP); 
      lv_label_set_text(forecast_day_labels[i], "..."); lv_obj_align(forecast_day_labels[i], LV_ALIGN_TOP_LEFT, 15, 50 + (i * 35));

      forecast_temp_labels[i] = lv_label_create(tile3); lv_obj_add_style(forecast_temp_labels[i], &style_orta_font, 0); lv_label_set_recolor(forecast_temp_labels[i], true);
      lv_obj_set_width(forecast_temp_labels[i], 55); lv_label_set_long_mode(forecast_temp_labels[i], LV_LABEL_LONG_CLIP);
      lv_label_set_text(forecast_temp_labels[i], "..."); lv_obj_align(forecast_temp_labels[i], LV_ALIGN_TOP_LEFT, 85, 50 + (i * 35));

      forecast_data_labels[i] = lv_label_create(tile3); lv_obj_add_style(forecast_data_labels[i], &style_orta_font, 0); lv_label_set_recolor(forecast_data_labels[i], true);
      lv_obj_set_width(forecast_data_labels[i], 150); lv_label_set_long_mode(forecast_data_labels[i], LV_LABEL_LONG_SCROLL_CIRCULAR); 
      lv_label_set_text(forecast_data_labels[i], "Yukleniyor..."); lv_obj_align(forecast_data_labels[i], LV_ALIGN_TOP_LEFT, 145, 50 + (i * 35));
  }

  // ==============================================
  // SAYFA 4: AYARLAR 
  // ==============================================
  lv_obj_t * s_head = lv_label_create(tile4); lv_obj_add_style(s_head, &style_orta_font, 0); lv_label_set_recolor(s_head, true); 
  lv_label_set_text(s_head, "#007AFF " LV_SYMBOL_SETTINGS " AYARLAR#"); lv_obj_align(s_head, LV_ALIGN_TOP_LEFT, 15, 10);

  label_saat4 = lv_label_create(tile4); lv_obj_add_style(label_saat4, &style_orta_font, 0); lv_obj_set_style_text_color(label_saat4, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_saat4, "--:--"); lv_obj_align(label_saat4, LV_ALIGN_TOP_RIGHT, -10, 10);

  lv_obj_t * sw_theme = lv_switch_create(tile4); lv_obj_align(sw_theme, LV_ALIGN_TOP_RIGHT, -20, 60);
  lv_obj_add_event_cb(sw_theme, theme_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  label_theme = lv_label_create(tile4); lv_obj_set_style_text_color(label_theme, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_theme, "Karanlik Tema"); lv_obj_align(label_theme, LV_ALIGN_TOP_LEFT, 20, 65);

  lv_obj_t * slider = lv_slider_create(tile4); lv_obj_set_width(slider, 200); lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 140);
  lv_slider_set_range(slider, 5, 100); lv_slider_set_value(slider, 100, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  label_br = lv_label_create(tile4); lv_obj_set_style_text_color(label_br, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_br, LV_SYMBOL_EYE_OPEN " Ekran Parlakligi"); lv_obj_align_to(label_br, slider, LV_ALIGN_OUT_TOP_LEFT, 0, -10);

  label_ip = lv_label_create(tile4); lv_obj_set_style_text_color(label_ip, lv_color_hex(0x1C1C1E), 0); 
  lv_label_set_text(label_ip, "IP: Baglaniliyor..."); lv_obj_align(label_ip, LV_ALIGN_BOTTOM_LEFT, 20, -20);

  lv_obj_t * btn_reboot = lv_btn_create(tile4); lv_obj_align(btn_reboot, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
  lv_obj_add_event_cb(btn_reboot, reboot_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t * lbl_reboot = lv_label_create(btn_reboot); lv_label_set_text(lbl_reboot, LV_SYMBOL_REFRESH);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  setenv("TZ", "UTC-3", 1); tzset(); configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  ArduinoCloud.update();
  lv_tick_inc(5); 
  lv_timer_handler(); 
  delay(5); 

  if (WiFi.status() == WL_CONNECTED) {
      lv_label_set_text(label_wifi, "#007AFF " LV_SYMBOL_WIFI "#");
      if (!ip_gosterildi) {
          String ipStr = "IP: " + WiFi.localIP().toString();
          lv_label_set_text(label_ip, ipStr.c_str());
          ip_gosterildi = true;
      }
  } else {
      lv_label_set_text(label_wifi, "#FF3B30 " LV_SYMBOL_WIFI "#");
      ip_gosterildi = false; 
  }

  if (need_forecast_update) { fetch_5day_forecast(); need_forecast_update = false; }

  if (millis() - sonSaatGuncelleme > 1000) {
    setenv("TZ", "UTC-3", 1); tzset(); struct tm ti; 
    if(getLocalTime(&ti, 10)){ 
      char ts[10]; strftime(ts, sizeof(ts), "%H:%M", &ti); 
      lv_label_set_text(label_saat, ts); lv_label_set_text(label_saat2, ts); 
      lv_label_set_text(label_saat3, ts); lv_label_set_text(label_saat4, ts); 
    }
    sonSaatGuncelleme = millis();
  }

  if (millis() - sonOkumaZamani > 5000) {
    float s = dht.readTemperature(); float n = dht.readHumidity();
    if (!isnan(s)) { 
        sicaklikIC = s; nemIC = n; 
        String c_txt = is_dark_mode ? "#FFFFFF" : "#1C1C1E";
        String m = "#FF9500 " LV_SYMBOL_HOME "#\n" + c_txt + " " + String(s, 0) + " C#\n#007AFF " LV_SYMBOL_TINT " %" + String(n, 0) + "#"; 
        lv_label_set_text(label_sicaklik, m.c_str()); 
    }
    sonOkumaZamani = millis();
  }

  if ((millis() - sonHavaGuncelleme > 600000 || !ilkHavaDurumuCekildi) && WiFi.status() == WL_CONNECTED) {
    HTTPClient http; http.begin(openWeatherURL); int hc = http.GET();
    if (hc == HTTP_CODE_OK) {
      JsonDocument doc; deserializeJson(doc, http.getString());
      if (!doc.isNull()) {
        double td = doc["main"]["temp"]; String dr = doc["weather"][0]["description"]; int wid = doc["weather"][0]["id"];
        sicaklikDIS = td; dr = fixTurkishChars(dr); havaDurumu = dr; dr.toUpperCase();
        
        String c_txt = is_dark_mode ? "#FFFFFF" : "#1C1C1E";
        String r = "#8E8E93"; if (wid < 600) r = "#007AFF"; else if (wid < 700) r = "#8E8E93"; else if (wid == 800) r = "#FF9500";
        
        // --- DIŞ HAVA DURUMU KUTUSU (KATMANLARI AYRI AYRI GÜNCELLE) ---
        lv_label_set_text(label_hava_icon, "#007AFF " LV_SYMBOL_GPS "#");
        
        String temp_str = c_txt + " " + String(td, 0) + " C#";
        lv_label_set_text(label_hava_temp, temp_str.c_str());
        
        String desc_str = r + " " + dr + "#";
        lv_label_set_text(label_hava_desc, desc_str.c_str());
        
        ilkHavaDurumuCekildi = true;
      }
    }
    http.end(); sonHavaGuncelleme = millis();
  }
}

void onLamba1Change() { update_widget_ui(sw1, "Lamba 1", lamba1); }
void onLamba2Change() { update_widget_ui(sw2, "Lamba 2", lamba2); }
void onLamba3Change() { update_widget_ui(sw3, "Lamba 3", lamba3); }
void onLamba4Change() { update_widget_ui(sw4, "Lamba 4", lamba4); }
void onLamba5Change() { update_widget_ui(sw5, "Lamba 5", lamba5); }
void onHavaDurumuChange()  {}
void onSicaklikDISChange()  {}