Anaheim Encoder Shield
======================

Bu proje, **Anaheim Encoder Shield** üzerinde yer alan hassas lineer encoder (400SI), I2C OLED ekran, buzzer ve KY‑041 rotary encoder ile çalışan **konum ölçüm ve izleme** yazılımını içerir. PlatformIO kullanılarak **Arduino Leonardo / ATmega32U4 (DFRobot Beetle uyumlu)** kartı hedef alınmıştır.

## Özellikler

- **400SI enkoder okuma**
  - 400 CPR, **X4 decode** ile **1600 count / tur**
  - **Hatve (lead)**: 2 mm / tur
  - Çözünürlük:
    - \( \text{um\_per\_count} = 1.25\,\mu m / \text{count} \)
    - \( \text{mm\_per\_count} = 0.00125\,mm / \text{count} \)
  - Hem **count**, hem **mm**, hem de **µm** cinsinden pozisyon takibi

- **I2C 128x64 OLED ekran (SSD1306)**
  - Ekran **180° döndürülmüş** kullanım (setRotation(2))
  - Açılış ekranı:
    - "Anaheim Encoder"
    - "Shield"
  - Ölçüm ekranı (her döngüde güncellenir):
    - `Dum` (mikron) – etiket küçük, değer büyük font
    - `Dmm` (milimetre)
    - `Dstep` (encoder count)

- **Seri port çıktısı (Excel’e uygun)**
  - Baud: **9600**
  - İlk satır: **sütun başlığı**
    - `Dum;Dmm;Dstep;`
  - Her ölçüm satırı örneği:
    - `Dum=25.8;Dmm=15.0000;Dstep=150;`
  - Bu format doğrudan **Excel / CSV** ortamına aktarılmak üzere tasarlanmıştır.

- **Buzzer (D4) uyarısı**
  - D4 pinine bağlı buzzer
  - Encoder pozisyonu her **1600 count (1 tur)** sınırını geçtiğinde **kısa bip** verir

- **KY‑041 kullanıcı enkoderi (test & reset)**
  - Bağlantılar:
    - `CLK` → **D11**
    - `DT`  → **D10**
    - `SW`  → **D9**
  - Seri portta dönüş yönü ve buton olayı test amaçlı loglanır:
    - `KY-041 ROT: CW  pos=...`
    - `KY-041 ROT: CCW pos=...`
    - `KY-041 BUTTON: PRESS -> RESET`
  - **Butona basıldığında**:
    - Ana encoder `position_count` ve `last_position` **sıfırlanır**
    - `ky_position` sıfırlanır
    - Ekrandaki `Dum`, `Dmm`, `Dstep` değerleri 0’dan devam eder

## Donanım Bağlantıları

### 1. Ana encoder (400SI)

- **Encoder A (CHA)** → **D7**  (`encoder_a`)
- **Encoder B (CHB)** → **D0**  (`encoder_b`)
- Besleme: kartın **5V** ve **GND**

> Not: Leonardo/Beetle kartında **D2 = SDA**, **D3 = SCL** (I2C hattı) olduğu için,
> encoder kesinlikle bu pinlere bağlanmamalıdır; aksi halde I2C (OLED) ve encoder çakışır.

### 2. I2C OLED (SSD1306, 128x64)

- **VCC** → 5V (veya modül üzerindeki işarete göre 3.3V)
- **GND** → GND
- **SDA** → **SDA pini** (Leonardo’da D2 hattı)
- **SCL** → **SCL pini** (Leonardo’da D3 hattı)

Kütüphaneler:

- `Adafruit SSD1306`
- `Adafruit GFX Library`

I2C adresi otomatik olarak **0x3C**, başarısız olursa **0x3D** ile denenir.

### 3. KY‑041 rotary encoder (arayüz enkoderi)

- **CLK** → D11  (`ky_clk`)
- **DT**  → D10  (`ky_dt`)
- **SW**  → D9   (`ky_sw`)
- **VCC** → 5V
- **GND** → GND

### 4. Buzzer

- **Buzzer +** → D4  (`BUZZER_PIN`)
- **Buzzer -** → GND

## Seri Port Veri Formatı ve Excel’e Aktarma

Seri port açıldığında (9600 baud):

1. İlk satır başlık:

   ```text
   Dum;Dmm;Dstep;
   ```

2. Sonraki her satır örneği:

   ```text
   Dum=25.8;Dmm=15.0000;Dstep=150;
   ```

Bu formatı **Excel**’e aktarmak için:

1. PlatformIO Serial Monitor’den tüm veriyi kopyalayın.
2. Not Defteri’ne yapıştırın, `data.txt` veya `data.csv` olarak kaydedin.
3. Excel’de:
   - **Veri → Metni Sütunlara Dönüştür (Text to Columns)** menüsünü açın.
   - **Ayırıcıya göre (Delimited)** seçin, **İleri**.
   - **Ayırıcılar** kısmında **`;` (noktalı virgül)** işaretli olsun, diğerlerini kaldırın.
   - **Son** deyin.
4. Her satır üç kolona ayrılacaktır:
   - A sütunu: `Dum=25.8`
   - B sütunu: `Dmm=15.0000`
   - C sütunu: `Dstep=150`

Sayısal değerleri etiketlerden ayırmak için, örneğin A sütunundaki `Dum=25.8`’i saf sayıya çevirmek isterseniz, yan sütunda şu formüllerle çalışabilirsiniz:

- Basit yöntem (etiketin uzunluğu değişmiyorsa):  
  `=SAĞ(A2;UZUNLUK(A2)-BUL("=",A2))`

Bu değeri sonra sayı formatına dönüştürebilirsiniz.

## Yazılım Mimarisi (Özet)

- `src/main.cpp` içerisinde:
  - **Encoder ISR’ları**: `encoderPinChangeA`, `encoderPinChangeB`
    - `position_count` ve `direction` değişkenlerini günceller.
  - **Ana döngü (`loop`)**:
    - `position_count`’u atomik olarak okur.
    - `delta`, `rpm`, `pos_um`, `pos_mm` vs. hesaplar.
    - **Buzzer**: 1600 count’luk blok geçişini tespit edip kısa bip verir.
    - **KY‑041**: buton & dönüş yönü okur, gerekirse konumu sıfırlar.
    - **OLED**: `Dum`, `Dmm`, `Dstep` değerlerini büyük fontta gösterir.
    - **Serial**: her döngüde `Dum=...;Dmm=...;Dstep=...;` satırı basar.

Örnek hesaplar:

- Bir turdaki toplam count:  
  \[
  \text{countsPerRev} = 400 \times 4 = 1600
  \]
- Mikron başına düşen count:  
  \[
  \text{um\_per\_count} = \frac{2.0 \times 1000}{1600} = 1.25\,\mu m
  \]
- Milimetre başına düşen count:  
  \[
  \text{mm\_per\_count} = \frac{2.0}{1600} = 0.00125\,mm
  \]

## Geliştirme Ortamı (PlatformIO)

`platformio.ini` içeriği özetle:

- Platform: `atmelavr`
- Kart: `leonardo`
- Framework: `arduino`
- Kütüphaneler:
  - `adafruit/Adafruit SSD1306`
  - `adafruit/Adafruit GFX Library`
  - `paulstoffregen/OneWire`
  - `milesburton/DallasTemperature` (gelecekte sıcaklık sensörü için, şu an opsiyonel)

### Derleme ve Yükleme

1. Bu projeyi **PlatformIO** ile açın.
2. Sağ alttan kart/ortam olarak `leonardo` seçili olduğundan emin olun.
3. **Build** (⌘/Ctrl+Alt+B) ile derleyin.
4. Kartın doğru COM portunu seçin.
5. **Upload** ile firmware’i karta yükleyin.

## Lisans

Bu proje, repoda yer alan `LICENSE` dosyasına göre lisanslanmıştır (**MIT License**).