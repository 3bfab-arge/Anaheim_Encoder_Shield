// ===== DFRobot Beetle (ATmega32U4 / Leonardo compatible) =====
// Encoder: 400SI (400 CPR)
// Decode: X4 (A+B CHANGE) => 1600 count / rev
// Lead (hatve): 2 mm / rev
// Sample period: 500 ms

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED ayarlari
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS   0x3C  // Once 0x3C denenir, olmazsa 0x3D denenir
#define BUZZER_PIN     4     // D4 buzzer

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const uint8_t encoder_a = 7;   // INT0 - 400SI encoder A
const uint8_t encoder_b = 0;   // INT1 - 400SI encoder B

// KY-041 rotary encoder (kullanici arayuzu)
const uint8_t ky_clk = 11;   // KY-041 CLK -> D11
const uint8_t ky_dt  = 10;   // KY-041 DT  -> D10
const uint8_t ky_sw  = 9;    // KY-041 SW  -> D9
 
volatile long position_count = 0;   // TOPLAM konum sayacı (SIFIRLANMAZ)
volatile int8_t direction = 1;
 
// --- Encoder + mekanik parametreler ---
const uint16_t cpr = 400;
const uint8_t  decodeFactor = 4;                  // X4
const uint16_t countsPerRev = cpr * decodeFactor; // 1600
 
const float lead_mm = 2.0f; // 2 mm / rev
 
const float um_per_count = (lead_mm * 1000.0f) / countsPerRev; // 1.25 µm/count
const float mm_per_count = lead_mm / countsPerRev;             // 0.00125 mm/count
 
// --- Ölçüm periyodu ---
// Not: 500 ms iken buton basislari kacabiliyordu, 50 ms'e dusuruldu.
const uint16_t sampleMs = 50;
 
long last_position = 0;
bool oled_ok = false;

// KY-041 icin durum degiskenleri
long ky_position = 0;
int  ky_last_clk = HIGH;
int  ky_last_sw  = HIGH;

// ISR fonksiyon prototipleri
void encoderPinChangeA();
void encoderPinChangeB();

void setup()
{
  Serial.begin(9600);

  Wire.begin();

  // OLED'i once 0x3C, sonra 0x3D adresi ile dene
  oled_ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!oled_ok) {
    oled_ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  }

  if (!oled_ok) {
    // OLED hic bulunamazsa seri porta bilgi yaz
    Serial.println(F("SSD1306 OLED bulunamadi (0x3C / 0x3D)!"));
  } else {
    display.setRotation(2);   // Ekrani 180 derece cevir
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    // Metni iki satira bolerek tasmayi engelle
    // "Anaheim Encoder" (15 char) -> (128 - 15*6)/2 ≈ 19 px
    display.setCursor(19, 16);
    display.println(F("Anaheim Encoder"));

    // "Shield" (6 char) -> (128 - 6*6)/2 ≈ 46 px
    display.setCursor(46, 30);
    display.println(F("Shield"));
    display.display();
    delay(3000);  // Açılış yazisini okumak icin daha uzun bekle
  }

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(encoder_a, INPUT_PULLUP);
  pinMode(encoder_b, INPUT_PULLUP);

  pinMode(ky_clk, INPUT_PULLUP);
  pinMode(ky_dt,  INPUT_PULLUP);
  pinMode(ky_sw,  INPUT_PULLUP);

  ky_last_clk = digitalRead(ky_clk);
  ky_last_sw  = digitalRead(ky_sw);
 
  attachInterrupt(digitalPinToInterrupt(encoder_a), encoderPinChangeA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoder_b), encoderPinChangeB, CHANGE);
}
 
void loop()
{
  static bool headerPrinted = false;
  long pos;
  int8_t dir;
 
  // Atomik okuma
  noInterrupts();
  pos = position_count;
  dir = direction;
  interrupts();
 
  // Son örnekten bu yana kaç count ilerledi?
  long delta = pos - last_position;

  // 1600 count (1 tur) siniri gecildiyse bip
  long prev_block = last_position / (long)countsPerRev;
  long curr_block = pos / (long)countsPerRev;
  if (curr_block != prev_block) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
  }

  last_position = pos;
 
  // RPM (delta işaretliyse rpm de işaretli çıkar)
  const float sampleSec = sampleMs / 1000.0f; // 0.5 s
  float rpm = (delta / sampleSec) * (60.0f / countsPerRev);
 
  // Mikron + milimetre karşılıkları
  float pos_um   = pos   * um_per_count;
  float delta_um = delta * um_per_count;
 
  float pos_mm   = pos   * mm_per_count;
  float delta_mm = delta * mm_per_count;

  // --- KY-041 test (serial cikis) ---
  int clk_now = digitalRead(ky_clk);
  int dt_now  = digitalRead(ky_dt);
  int sw_now  = digitalRead(ky_sw);

  // Donus tespiti
  if (clk_now != ky_last_clk) {
    if (clk_now == LOW) { // adim kenari
      if (dt_now != clk_now) {
        ky_position++;
        Serial.print(F("KY-041 ROT: CW  pos="));
        Serial.println(ky_position);
      } else {
        ky_position--;
        Serial.print(F("KY-041 ROT: CCW pos="));
        Serial.println(ky_position);
      }
    }
    ky_last_clk = clk_now;
  }

  // Buton tespiti (dusuk seviye = basili)
  if (ky_last_sw == HIGH && sw_now == LOW) {
    Serial.println(F("KY-041 BUTTON: PRESS -> RESET"));

    // Encoder pozisyonlarini sifirla
    noInterrupts();
    position_count = 0;
    last_position  = 0;
    interrupts();

    ky_position = 0;
  }
  ky_last_sw = sw_now;

  if (oled_ok) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // 1. satir: Dum etiketi kucuk, deger buyuk
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Dum"));
    display.setTextSize(2);
    display.setCursor(40, 4);
    display.print(pos_um, 1);

    // 2. satir: Dmm etiketi kucuk, deger buyuk
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.print(F("Dmm"));
    display.setTextSize(2);
    display.setCursor(40, 28);
    display.print(pos_mm, 4);

    // 3. satir: Dstep etiketi kucuk, deger buyuk
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print(F("Dstep"));
    display.setTextSize(2);
    display.setCursor(64, 48);
    display.print(pos);

    display.display();
  }
 
  // Log (Excel icin, etiketli ve ';' ile ayrilmis):
  // Ornek: Dum=25.8;Dmm=15.0;Dstep=150;
  if (!headerPrinted) {
    Serial.println(F("Dum;Dmm;Dstep;"));
    headerPrinted = true;
  }

  // Dum (mikron) - basta
  Serial.print(F("Dum="));
  Serial.print(pos_um, 1);    // mikron
  Serial.print(';');

  // Dmm (mm) - ortada
  Serial.print(F("Dmm="));
  Serial.print(pos_mm, 4);    // mm
  Serial.print(';');

  // Dstep (count) - sonda
  Serial.print(F("Dstep="));
  Serial.print(pos);          // adim (count)
  Serial.println(';');

  // Eski detayli loglar (gerekirse acilabilir)
  // Serial.print("   Delta: ");
  // Serial.print(delta);
  // Serial.print("   RPM: ");
  // Serial.print(rpm);
  // Serial.print("   Dir: ");
  // Serial.print(dir);
  // Serial.print("   d(um): ");
  // Serial.print(delta_um);
  // Serial.print("   d(mm): ");
  // Serial.print(delta_mm, 4);
 
  delay(sampleMs);
}
 
void encoderPinChangeA()
{
  int a = digitalRead(encoder_a);
  int b = digitalRead(encoder_b);
  direction = (a == b) ? -1 : 1;
  position_count += direction;
}
 
void encoderPinChangeB()
{
  int a = digitalRead(encoder_a);
  int b = digitalRead(encoder_b);
  direction = (a != b) ? -1 : 1;
  position_count += direction;
}