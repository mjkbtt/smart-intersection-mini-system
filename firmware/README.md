# ESP32 Firmware

| Sketch | Зориулалт |
|---|---|
| [`env_test/`](env_test/env_test.ino) | Орчны тест: compile, сангууд, Blink + Serial, 30 минутын тасралтгүй ажиллагаа (#3) |
| [`intersection_node/`](intersection_node/) | Уулзварын үндсэн код: Wi-Fi автомат тохиргоо (#8), MQTT холболт (#9) |

## 1. Орчин бэлдэх (#3, T1–T2)

1. **Arduino IDE 2.x** суулгана.
2. **USB драйвер** — ESP32 самбар дээрх чипээс хамаарна:
   - CP2102 → [Silicon Labs CP210x VCP driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
   - CH340 → [WCH CH340 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html)
   Суулгасны дараа *Device Manager → Ports (COM & LPT)* дээр COM порт гарч ирэх ёстой.
3. **ESP32 core**: *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   → *Boards Manager* дээр **esp32 by Espressif Systems** (3.x) суулгана.
4. **Board**: *Tools → Board → esp32 → ESP32 Dev Module*, *Upload Speed* 921600 (асуудал гарвал 115200).
5. **Сангууд** (*Library Manager*):
   - `PubSubClient` by Nick O'Leary — 2.8+
   - `WiFiManager` by tzapu — 2.0.17+
   - `WiFi`, `Preferences` нь ESP32 core-д багтсан.

Upload хийх үед `Connecting.....` дээр зогсвол самбарын **BOOT** товчийг дарж байгаад суллана.

## 2. Орчны тест (#3, T3)

`env_test/env_test.ino`-г нээж Upload хийгээд Serial Monitor-ыг **115200** baud-д тохируулна.

```
========== ESP32 ОРЧНЫ ТЕСТ ==========
Чип          : ESP32-D0WD-V3 rev 3, 2 цөм, 240 MHz
...
[WiFi]       : сан OK, 6 сүлжээ олдлоо
[PubSubClient]: сан OK, buffer 512 байт, төлөв -1
Blink эхэллээ. 30 минут тасралтгүй ажиллахыг хүлээнэ үү...
[00:00:10] heap=... (min ...) loop/s=...
...
[PASS] 30 минут тасралтгүй ажиллалаа. Heap өөрчлөлт: ... байт
```

**DONE шалгуур:** `[PASS]` мөр гарч, хооронд нь `ESP32 ОРЧНЫ ТЕСТ` гарчиг дахин хэвлэгдээгүй (reset болоогүй) байх.
Хэрэв `Reset шалтгаан: BROWNOUT` гарвал USB кабель/тэжээлээ солино.

## 3. Уулзварын код (#8, #9)

1. `intersection_node/secrets.example.h` → **`secrets.h`** болгон хуулж MQTT нууц үгийг (`broker/users.env`-ийн `ESP32_PASS`) оруулна.
   `secrets.h` нь git-д орохгүй.
2. Upload хийнэ.

### Wi-Fi автомат тохиргоо (#8)

| Нөхцөл | Үйлдэл |
|---|---|
| Анх асахад (хадгалсан сүлжээгүй) | `SmartIntersection-Setup` AP нээгдэнэ (нууц үг: `WIFI_PORTAL_PASS`). Утсаар холбогдоход тохиргооны хуудас автоматаар гарна (эсвэл http://192.168.4.1) → **Configure WiFi** → сүлжээ, нууц үг, **MQTT broker IP**, порт → Save |
| Дараагийн асалт | Хадгалсан сүлжээнд автоматаар холбогдоно (портал гарахгүй) |
| Хадгалсан сүлжээ олдохгүй | 180 секунд портал нээлттэй байж, дараа нь 10 секунд тутам дахин холбогдохыг оролдоно |
| Ажиллаж байх үед Wi-Fi тасрах | Автоматаар дахин холбогдоно; MQTT мөн дахин холбогдоно (2 с → 30 с завсар) |
| Сүлжээ солих | **BOOT товчийг 3 секунд** дарна → хадгалсан тохиргоо арилж портал нээгдэнэ |

Портал нээлттэй байх үед ч `loop()` зогсохгүй (non-blocking) тул гэрлэн дохионы логик үргэлжлэн ажиллана.

Төлвийн LED (GPIO2): **хурдан анивчих** = портал, **удаан анивчих** = холбогдож байна, **асаалттай** = холбогдсон.

### Serial дээр харагдах жишээ

```
=== Smart Intersection esp32-int01 (fw 0.2.0) ===
[WiFi] Хадгалсан сүлжээ: MyHomeWiFi
[WiFi] Холбогдлоо: MyHomeWiFi, IP 192.168.1.50, RSSI -58 dBm
[WiFi] MQTT broker: 192.168.1.10:1883
[MQTT] 192.168.1.10:1883 руу холбогдож байна...
[MQTT] Холбогдлоо
[MQTT] <- intersection/int01/command {"seq":1,"phase":"EW_GREEN",...}
```

`[MQTT] Амжилтгүй, state=4` → нэр/нууц үг буруу; `state=-2` → broker-ын IP/порт эсвэл галт хана.

### Бусад модульд үлдээсэн хэсэг

`intersection_node.ino` дотор `TODO(#4)` (IR мэдрэгч), `TODO(#11)` (команд → LED), `TODO(#13, #15)` (гэрлийн төлөвийн машин, fallback) гэж тэмдэглэсэн газарт тухайн хариуцагч кодоо нэмнэ.
