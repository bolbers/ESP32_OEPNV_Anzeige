#include <WiFi.h>
#include <WiFiMulti.h>                             
#include <HTTPClient.h>
#include <ArduinoJson.h>                      
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>  
#include <time.h>


/* =====================================================
   ===================== WIFI =========================
   ===================================================== */
WiFiMulti wifiMulti;

const char* ssid1     = ".home@olbers.net";
const char* password1 = "";

const char* ssid2     = "berlin.freifunk.net";
const char* password2 = "";

const char* ssid3     = "thw.freifunk";
const char* password3 = "";

/* =====================================================
   ===================== BVG v6 API ===================
   ===================================================== */
const char* bvgUrl =
  "https://v6.bvg.transport.rest/stops/900180521/departures?results=4&duration=830&lines=true&language=de"; //Betonwerk
  //"https://v6.bvg.transport.rest/stops/900086161/departures?results=4&duration=830&lines=true&language=de"; // Otisstr U6
 
/* =====================================================
   ===================== HUB75 PINS ===================
   ===================================================== */
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13

#define A_PIN  23
#define B_PIN  19
#define C_PIN  5
#define D_PIN  17
#define E_PIN  -1

#define CLK_PIN 16
#define LAT_PIN 4
#define OE_PIN  15

/* =====================================================
   ===================== DISPLAY ======================
   ===================================================== */
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32
#define PANEL_CHAIN   2     // 2 Panels → 128×32
#define SCREEN_WIDTH  128

HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);
MatrixPanel_I2S_DMA* display;

/* =====================================================
   ===================== LAYOUT =======================
   ===================================================== */
#define HEADER_HEIGHT 8
#define ROW_HEIGHT    8
#define ROW_COUNT     3

/* =====================================================
   ===================== SCROLL =======================
   ===================================================== */
#define CHAR_WIDTH     6
#define SCROLL_SPEED   40
#define SCROLL_PAUSE   1200

struct ScrollState {
  int offset = 0;
  unsigned long lastMove = 0;
  unsigned long pauseUntil = 0;
};

/* =====================================================
   ===================== STRUCT =======================
   ===================================================== */
struct Departure {
  String line;
  String destination;
  String product;
  int minutes;
  String remarktext; 
};

Departure departures[ROW_COUNT];
ScrollState rowScroll[ROW_COUNT];
ScrollState warningScroll;

bool hasWarning = false;
String warningText = "";

/* =====================================================
   ===================== 5x7 FONT =====================
   ===================================================== */
const uint8_t font5x7[][5] = {
  // ASCII 32-126
  {0x00,0x00,0x00,0x00,0x00}, // 32 ' '
  {0x00,0x00,0x5F,0x00,0x00}, // 33 !
  {0x00,0x07,0x00,0x07,0x00}, // 34 "
  {0x22,0x54,0x54,0x78,0x42}, // 35 #    //replaced with german ä // original 0x14,0x7F,0x14,0x7F,0x14
  {0x3A,0x44,0x44,0x44,0x3A}, // 36 $    //replaced with german ö // original 0x24,0x2A,0x7F,0x2A,0x12
  {0x3A,0x40,0x40,0x20,0x7A}, // 37 %    //replaced with german ü // original 0x23,0x13,0x08,0x64,0x62
  {0x7D,0x12,0x11,0x12,0x7D}, // 38 &    //replaced with german Ä // original 0x36,0x49,0x56,0x20,0x50
  {0x00,0x05,0x03,0x00,0x00}, // 39 '
  {0x00,0x1C,0x22,0x41,0x00}, // 40 (
  {0x00,0x41,0x22,0x1C,0x00}, // 41 )
  {0x14,0x08,0x3E,0x08,0x14}, // 42 *
  {0x08,0x08,0x3E,0x08,0x08}, // 43 +
  {0x00,0x50,0x30,0x00,0x00}, // 44 ,
  {0x08,0x08,0x08,0x08,0x08}, // 45 -
  {0x00,0x60,0x60,0x00,0x00}, // 46 .
  {0x20,0x10,0x08,0x04,0x02}, // 47 /
  {0x3E,0x51,0x49,0x45,0x3E}, // 48 0
  {0x00,0x42,0x7F,0x40,0x00}, // 49 1
  {0x42,0x61,0x51,0x49,0x46}, // 50 2
  {0x21,0x41,0x45,0x4B,0x31}, // 51 3
  {0x18,0x14,0x12,0x7F,0x10}, // 52 4
  {0x27,0x45,0x45,0x45,0x39}, // 53 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 54 6
  {0x01,0x71,0x09,0x05,0x03}, // 55 7
  {0x36,0x49,0x49,0x49,0x36}, // 56 8
  {0x06,0x49,0x49,0x29,0x1E}, // 57 9
  {0x00,0x36,0x36,0x00,0x00}, // 58 :
  {0x00,0x56,0x36,0x00,0x00}, // 59 ;
  {0x08,0x14,0x22,0x41,0x00}, // 60 <
  {0x14,0x14,0x14,0x14,0x14}, // 61 =
  {0x00,0x41,0x22,0x14,0x08}, // 62 >
  {0x02,0x01,0x51,0x09,0x06}, // 63 ?
  {0x32,0x49,0x79,0x41,0x3E}, // 64 @
  {0x7E,0x11,0x11,0x11,0x7E}, // 65 A
  {0x7F,0x49,0x49,0x49,0x36}, // 66 B
  {0x3E,0x41,0x41,0x41,0x22}, // 67 C
  {0x7F,0x41,0x41,0x22,0x1C}, // 68 D
  {0x7F,0x49,0x49,0x49,0x41}, // 69 E
  {0x7F,0x09,0x09,0x09,0x01}, // 70 F
  {0x3E,0x41,0x49,0x49,0x7A}, // 71 G
  {0x7F,0x08,0x08,0x08,0x7F}, // 72 H
  {0x00,0x41,0x7F,0x41,0x00}, // 73 I
  {0x20,0x40,0x41,0x3F,0x01}, // 74 J
  {0x7F,0x08,0x14,0x22,0x41}, // 75 K
  {0x7F,0x40,0x40,0x40,0x40}, // 76 L
  {0x7F,0x02,0x0C,0x02,0x7F}, // 77 M
  {0x7F,0x04,0x08,0x10,0x7F}, // 78 N
  {0x3E,0x41,0x41,0x41,0x3E}, // 79 O
  {0x7F,0x09,0x09,0x09,0x06}, // 80 P
  {0x3E,0x41,0x51,0x21,0x5E}, // 81 Q
  {0x7F,0x09,0x19,0x29,0x46}, // 82 R
  {0x46,0x49,0x49,0x49,0x31}, // 83 S
  {0x01,0x01,0x7F,0x01,0x01}, // 84 T
  {0x3F,0x40,0x40,0x40,0x3F}, // 85 U
  {0x1F,0x20,0x40,0x20,0x1F}, // 86 V
  {0x3F,0x40,0x38,0x40,0x3F}, // 87 W
  {0x63,0x14,0x08,0x14,0x63}, // 88 X
  {0x07,0x08,0x70,0x08,0x07}, // 89 Y
  {0x61,0x51,0x49,0x45,0x43}, // 90 Z
  {0x00,0x7F,0x41,0x41,0x00}, // 91 [
  {0x02,0x04,0x08,0x10,0x20}, // 92 '\'
  {0x00,0x41,0x41,0x7F,0x00}, // 93 ]
  {0x04,0x02,0x01,0x02,0x04}, // 94 ^
  {0x40,0x40,0x40,0x40,0x40}, // 95 _
  {0x00,0x01,0x02,0x04,0x00}, // 96 `
  {0x20,0x54,0x54,0x54,0x78}, // 97 a
  {0x7F,0x48,0x44,0x44,0x38}, // 98 b
  {0x38,0x44,0x44,0x44,0x20}, // 99 c
  {0x38,0x44,0x44,0x48,0x7F}, // 100 d
  {0x38,0x54,0x54,0x54,0x18}, // 101 e
  {0x08,0x7E,0x09,0x01,0x02}, // 102 f
  {0x0C,0x52,0x52,0x52,0x3E}, // 103 g
  {0x7F,0x08,0x04,0x04,0x78}, // 104 h
  {0x00,0x44,0x7D,0x40,0x00}, // 105 i
  {0x20,0x40,0x44,0x3D,0x00}, // 106 j
  {0x7F,0x10,0x28,0x44,0x00}, // 107 k
  {0x00,0x41,0x7F,0x40,0x00}, // 108 l
  {0x7C,0x04,0x18,0x04,0x78}, // 109 m
  {0x7C,0x08,0x04,0x04,0x78}, // 110 n
  {0x38,0x44,0x44,0x44,0x38}, // 111 o
  {0x7C,0x14,0x14,0x14,0x08}, // 112 p
  {0x08,0x14,0x14,0x18,0x7C}, // 113 q
  {0x7C,0x08,0x04,0x04,0x08}, // 114 r
  {0x48,0x54,0x54,0x54,0x20}, // 115 s
  {0x04,0x3F,0x44,0x40,0x20}, // 116 t
  {0x3C,0x40,0x40,0x20,0x7C}, // 117 u
  {0x1C,0x20,0x40,0x20,0x1C}, // 118 v
  {0x3C,0x40,0x30,0x40,0x3C}, // 119 w
  {0x44,0x28,0x10,0x28,0x44}, // 120 x
  {0x0C,0x50,0x50,0x50,0x3C}, // 121 y
  {0x44,0x64,0x54,0x4C,0x44}, // 122 z
  {0x3D,0x42,0x42,0x42,0x3D}, // 123 {    //replaced with german Ö // original 0x00, 0x08, 0x36, 0x41, 0x00
  {0x00,0x00,0x7F,0x00,0x00}, // 124 |    
  {0x3D,0x40,0x40,0x40,0x3D}, // 125 }    //replaced with german Ü // original 0x00, 0x41, 0x36, 0x08, 0x00
  {0xFC,0x4A,0x4A,0x4A,0x34}, // 126 ~    //replaced with german ß // original 0x02, 0x01, 0x02, 0x04, 0x02
  // Deutsche Umlaute
  {0x7D,0x12,0x11,0x12,0x7D}, // 127 Ä
  {0x3D,0x42,0x42,0x42,0x3D}, // 128 Ö
  {0x3D,0x40,0x40,0x40,0x3D}, // 129 Ü
  {0x76,0x09,0x09,0x09,0x7E}, // 130 ä
  {0x3A,0x44,0x44,0x44,0x3A}, // 131 ö
  {0x3A,0x40,0x40,0x20,0x7A}, // 132 ü
  {0xFC,0x4A,0x4A,0x4A,0x34}  // 133 ß
};

uint8_t getCharIndex(char c) {
  // Serial.printf("[CHAR] %d ",c);
  switch(c) {
    default:
      if (c >= 32 && c <= 126) return c - 32;
      return 0; // Leerzeichen
  }
}

void drawChar5x7(int x, int y, char c, uint16_t color) {
  uint8_t idx = getCharIndex(c);
  for (int col = 0; col < 5; col++) {
    uint8_t line = font5x7[idx][col];
    for (int row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        display->drawPixel(x + col, y + row, color);
      }
    }
  }
}

void drawString5x7(int x, int y, const String& str, uint16_t color) {
  int cursorX = x;
  for (int i = 0; i < str.length(); i++) {
    drawChar5x7(cursorX, y, str[i], color);
    cursorX += 6;
  }
}

int textWidth(const String&s){ return s.length()*CHAR_WIDTH; }

void drawScrollingString(int x,int y,const String&t,uint16_t c,ScrollState&st){
  int w=textWidth(t);
  unsigned long now=millis();

  if(w<=SCREEN_WIDTH-x){
    drawString5x7(x,y,t,c);
    st.offset=0;
    return;
  }

  if(now<st.pauseUntil){
    drawString5x7(x-st.offset,y,t,c);
    return;
  }

  if(now-st.lastMove>SCROLL_SPEED){
    st.offset++;
    st.lastMove=now;
    if(st.offset>w+20){
      st.offset=0;
      st.pauseUntil=now+SCROLL_PAUSE;
    }
  }

  drawString5x7(x-st.offset,y,t,c);
  drawString5x7(x-st.offset+w+20,y,t,c);
}

////////////////////////////////////////////////////////////
// CLIPPED SCROLL
////////////////////////////////////////////////////////////

void drawCharClipped(
  int x, int y,
  char c,
  uint16_t color,
  int clipX,
  int clipW
) {
  if (c < 32 || c > 127) return;
  const uint8_t* chr = font5x7[c - 32];

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 7; j++) {
      if (chr[i] & (1 << j)) {
        int px = x + i;
        if (px >= clipX && px < clipX + clipW) {
          display->drawPixel(px, y + j, color);
        }
      }
    }
  }
}

void drawStringClipped(
  int x, int y,
  String text,
  uint16_t color,
  int clipX,
  int clipW
) {
  for (int i = 0; i < text.length(); i++) {
    drawCharClipped(
      x + i * CHAR_WIDTH,
      y,
      text[i],
      color,
      clipX,
      clipW
    );
  }
}

int textWidthPx(const String &s) {
  return s.length() * CHAR_WIDTH;
}


////////////////////////////////////////////////////////////
// SCROLL MIT CLIPPING
////////////////////////////////////////////////////////////

void drawScrollingText(
  int areaX,
  int areaWidth,
  int y,
  const String& text,
  uint16_t color,
  ScrollState& state
) {
  int textWidth = textWidthPx(text);

  if (textWidth <= areaWidth) {
    drawStringClipped(areaX, y, text, color, areaX, areaWidth);
    state.offset = 0;
    return;
  }

  unsigned long now = millis();

  if (state.pauseUntil > now) {
    drawStringClipped(areaX - state.offset, y, text, color, areaX, areaWidth);
    return;
  }

  if (now - state.lastMove > SCROLL_SPEED) {
    state.offset++;
    state.lastMove = now;

    if (state.offset > textWidth + 20) {
      state.offset = 0;
      state.pauseUntil = now + SCROLL_PAUSE;
    }
  }

  drawStringClipped(areaX - state.offset, y, text, color, areaX, areaWidth);
  drawStringClipped(areaX - state.offset + textWidth + 20, y, text, color, areaX, areaWidth);
}

/* =====================================================
   ===================== COLORS =======================
   ===================================================== */
uint16_t getLineColor(const String& product) {
  if (product == "subway")    return display->color565(35, 35, 255);  // Blau
  if (product == "bus")       return display->color565(255, 35, 255); // Magenta
  if (product == "tram")      return display->color565(255, 35, 35);  // ROT
  if (product == "suburban")  return display->color565(11, 98, 11);   // Dunkelgrün
  if (product == "ferry")     return display->color565(0, 136, 255);  // Hellblau
  return display->color565(255, 255, 255); // Weiß
}

/* =====================================================
   ===================== TIME =========================
   ===================================================== */
void drawHeaderTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char timeStr[6];
  char dateStr[11];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%d.%m", &timeinfo);

  drawString5x7(0,  0, "Betonwerk", display->color565(253,119,0));
  drawString5x7(62, 0, dateStr,     display->color565(253,119,0));
  drawString5x7(98, 0, timeStr,     display->color565(253,119,0));
}

/* =====================================================
   ===================== FETCH ========================
   ===================================================== */
int getMinutesToArrival(const String& isoTimeStr) {
  int year   = isoTimeStr.substring(0,4).toInt();
  int month  = isoTimeStr.substring(5,7).toInt();
  int day    = isoTimeStr.substring(8,10).toInt();
  int hour   = isoTimeStr.substring(11,13).toInt();
  int minute = isoTimeStr.substring(14,16).toInt();
  int second = isoTimeStr.substring(17,19).toInt();

  struct tm arrivalTm = {0};
  arrivalTm.tm_year = year - 1900;
  arrivalTm.tm_mon  = month - 1;
  arrivalTm.tm_mday = day;
  arrivalTm.tm_hour = hour;
  arrivalTm.tm_min  = minute;
  arrivalTm.tm_sec  = second;
  arrivalTm.tm_isdst = -1;

  time_t arrivalTime = mktime(&arrivalTm);

  struct tm nowTm;
  if (!getLocalTime(&nowTm)) return -999;
  time_t nowTime = mktime(&nowTm);

  int diffSeconds = difftime(arrivalTime, nowTime);
  return round(diffSeconds / 60.0);
}

String decodeUtf8(String input) {  //Replace auf ungenutze Zeichen < 126 in der ASCII Tabelle
  input.replace("\u00e4", "#"); // ä
  input.replace("\u00f6", "$"); // ö
  input.replace("\u00fc", "%"); // ü
  input.replace("\u00c4", "&"); // Ä
  input.replace("\u00d6", "{"); // Ö
  input.replace("\u00dc", "}"); // Ü
  input.replace("\u00df", "~"); // ß

  for (int i=0;i<input.length();i++)
    if (input[i]<32 || input[i]>126) input[i]='?';
  return input;
}

void fetchDepartures() {
  hasWarning=false;
  warningText="";

  Serial.println("[DEBUG] Starte BVG v6-Request...");
  HTTPClient http;
  http.begin(bvgUrl);
  int httpCode = http.GET();

  if(httpCode == 200) {
    Serial.println("[DEBUG] API-Request erfolgreich");
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    JsonArray arr = doc["departures"].as<JsonArray>();
    
    for(int i=0; i<ROW_COUNT && i<arr.size(); i++){
  
      departures[i].line = arr[i]["line"]["name"].as<String>();
      departures[i].destination = arr[i]["direction"].as<String>();
      departures[i].product    = arr[i]["line"]["product"].as<String>();
      int min_to_arrive = getMinutesToArrival(arr[i]["when"].as<String>());
      departures[i].minutes = constrain(min_to_arrive,-99,999);
      
        Serial.print ("[DEBUG] Linie "); Serial.print (departures[i].line.c_str()); Serial.print (" ");
        Serial.print (departures[i].destination.c_str()); Serial.print (" "); Serial.print (departures[i].product.c_str());
        Serial.print (" in "); Serial.print (departures[i].minutes);
        Serial.println (" min");
      departures[i].destination = decodeUtf8(departures[i].destination);
      


      if (hasWarning) {
        break;
      }  
      else{
      JsonArray mdata = arr[i]["remarks"].as<JsonArray>();
      for(int x=0; x<mdata.size(); x++){
       if(mdata[x]["type"].as<String>() == "warning"){   //warning
          //Serial.print(mdata[x]["type"].as<String>());
          departures[i].remarktext = mdata[x]["text"].as<String>();
          warningText = decodeUtf8(departures[i].remarktext);
          //Serial.print (departures[i].remarktext.c_str());
          hasWarning=true;
          break;
       }           
      }        
      }
    }
  
  } else {
    Serial.printf("[DEBUG] API-Request Fehler: %d\n", httpCode);
  }
  http.end();
}

/* =====================================================
   ===================== DRAW ROW ======================
   ===================================================== */
void drawRow(const Departure& d, int y) {
  drawString5x7(0, y, d.line, getLineColor(d.product));
  drawString5x7(20, y, d.destination.substring(0,14), display->color565(180,180,180));
  //drawString5x7(110, y, String(d.minutes), display->color565(255,255,255));
  if (d.minutes < 1) {
    display ->setTextColor(display->color565(255,0,0));
  }
  else {
    display->setTextColor(display->color565(255, 255, 255));
  }
  
  display->setCursor(110, y);
  display->printf("%3d", d.minutes);
}

void drawDepartureRow(const Departure& d, int row) {
  int y = HEADER_HEIGHT + row * ROW_HEIGHT;

  // Linie (0–20 px)
  display->setCursor(0, y);
  display->setTextColor(getLineColor(d.product));
  display->print(d.line);

  // Ziel (22–108 px)
  drawScrollingText(
    22,
    86,
    y,
    d.destination,
    display->color565(200,200,200),
    rowScroll[row]
  );

  // Minuten (110–128 px)
  display->setCursor(110, y);
  // display->setTextColor(display->color565(255,255,255));
    if (d.minutes < 1) {
    display ->setTextColor(display->color565(255,0,0));
  }
  else {
    display->setTextColor(display->color565(255, 255, 255));
  }
  display->printf("%3d", d.minutes);
}

////////////////////////////////////////////////////////////

void drawWarningRow() {
  int y = HEADER_HEIGHT + 2 * ROW_HEIGHT;

  drawScrollingText(
    0,
    128,
    y,
    warningText,
    display->color565(255,140,0),
    warningScroll
  );
}

////////////////////////////////////////////////////////////

void drawScreen() {
  display->clearScreen();
  drawHeaderTime();

  if (hasWarning) {
    drawDepartureRow(departures[0],0);
    drawDepartureRow(departures[1],1);
    drawWarningRow();
  } else {
    drawDepartureRow(departures[0],0);
    drawDepartureRow(departures[1],1);
    drawDepartureRow(departures[2],2);
  }
}


void drawoldScreen() {
  display->clearScreen();
  drawHeaderTime();

  int rows=hasWarning?ROW_COUNT-1:ROW_COUNT;

  //for(int i=0;i<ROW_COUNT;i++)
  //  drawRow(departures[i], HEADER_HEIGHT + i*ROW_HEIGHT);

    for(int i=0;i<rows;i++){
    int y=HEADER_HEIGHT+i*ROW_HEIGHT;
    String minlen = "";
    drawString5x7(0,y,departures[i].line,getLineColor(departures[i].product));
    drawScrollingString(20,y,departures[i].destination.substring(0,15),display->color565(200,200,200),rowScroll[i]);  //.substring(0,15)
    minlen=departures[i].minutes;
    if (departures[i].minutes <1) {
           drawString5x7(122-((minlen.length()-1)*6),y,String(departures[i].minutes),display->color565(255,0,0));
    }
    else {
           drawString5x7(122-((minlen.length()-1)*6),y,String(departures[i].minutes),display->color565(255,255,255));
    }       
  }

  if(hasWarning){
    drawScrollingString(0,HEADER_HEIGHT+2*ROW_HEIGHT,warningText,display->color565(255,120,0),warningScroll);
  }

}

/* =====================================================
   ===================== SETUP =========================
   ===================================================== */
void setup() {
  Serial.begin(115200);
  Serial.println("[DEBUG] Setup gestartet");

  mxconfig.gpio.r1  = R1_PIN;
  mxconfig.gpio.g1  = G1_PIN;
  mxconfig.gpio.b1  = B1_PIN;
  mxconfig.gpio.r2  = R2_PIN;
  mxconfig.gpio.g2  = G2_PIN;
  mxconfig.gpio.b2  = B2_PIN;
  mxconfig.gpio.a   = A_PIN;
  mxconfig.gpio.b   = B_PIN;
  mxconfig.gpio.c   = C_PIN;
  mxconfig.gpio.d   = D_PIN;
  mxconfig.gpio.e   = E_PIN;
  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe  = OE_PIN;
  // mxconfig.latch_blanking = 4;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false; 

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setBrightness8(30);

  Serial.println("[DEBUG] Display initialisiert");

  display->setTextColor(display->color565(255, 0, 0));
  display->setCursor(0, 0);
  display->print("Verbinde WLAN");  
  
//  WiFi.begin(ssid, password);
//  Serial.print("[DEBUG] Verbinde mit WLAN");
//  while(WiFi.status() != WL_CONNECTED){
//    delay(300);
//    Serial.print(".");
//    display->print(".");    
//  }
//  Serial.println("\n[DEBUG] WLAN verbunden");

  wifiMulti.addAP(ssid1,password1);
  wifiMulti.addAP(ssid2,password2);
  wifiMulti.addAP(ssid3,password3);

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.SSID());
    display->setCursor(0, 15);
    display->print(WiFi.SSID());
  }  
  display->setCursor(0, 24);
  display->print("Verbunden");
  
  configTime(1 * 3600, 3600, "pool.ntp.org");


  fetchDepartures();
  drawScreen();
  Serial.println("[DEBUG] Setup beendet");
}

/* =====================================================
   ===================== LOOP ==========================
   ===================================================== */
unsigned long lastUpdate = 0;
unsigned long lastClock  = 0;
unsigned long lastFetch  = 0;

void loop() {
  if(millis() - lastFetch > 30000) { fetchDepartures(); lastFetch=millis(); }
  if(millis() - lastClock > 10000) { drawScreen(); lastClock = millis(); }
  if(millis() - lastUpdate > 300){ drawScreen(); lastUpdate = millis(); }
  delay(50);

}
