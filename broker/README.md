# MQTT Broker (Mosquitto)

> Issue: [1.2.2] MQTT broker (Mosquitto) тохируулах (#9)

| Файл | Агуулга |
|---|---|
| `config/mosquitto.conf` | Порт `1883` (MQTT), `9001` (WebSocket), нэвтрэлт заавал, persistence |
| `config/acl` | Хэрэглэгч бүрийн topic эрх ([docs/interfaces.md §3](../docs/interfaces.md#3-broker-хэрэглэгч-ба-эрх)) |
| `docker-compose.yml` | `eclipse-mosquitto:2.0.20` контейнер |
| `users.env.example` | Нууц үгийн загвар → `users.env` (git-д орохгүй) |
| `create_users.py` | `users.env`-ээс `config/passwd` үүсгэнэ |
| `test/pubsub_check.py` | Publish/subscribe урсгал, ACL, LWT-г шалгана |

## T1: Суулгах ба порт

### A. Docker (санал болгох)

```bash
cd broker
cp users.env.example users.env      # нууц үгээ солино
python create_users.py              # config/passwd үүснэ
docker compose up -d
docker logs -f smart-intersection-mosquitto
```

Windows дээр `passwd has world readable permissions` гэсэн анхааруулга гарна — bind mount-ын онцлог бөгөөд 2.0.x хувилбарт ажиллагаанд нөлөөлөхгүй.

### B. Windows дээр шууд суулгах

1. https://mosquitto.org/download/ -оос `mosquitto-2.0.x-install-windows-x64.exe` суулгана.
2. `config/mosquitto.conf` доторх `/mosquitto/...` замуудыг Windows зам болгоно, жишээ нь:
   `persistence_location C:/mosquitto/data/`, `password_file C:/mosquitto/passwd`, `acl_file C:/mosquitto/acl`.
3. Нууц үгийн файл: `python create_users.py` (PATH дээр `mosquitto_passwd` байвал түүнийг ашиглана).
4. Ажиллуулах: `mosquitto -c C:\path\to\mosquitto.conf -v`

### Галт хана

ESP32 broker-т хүрэхийн тулд сервер компьютер дээр `1883` (ба dashboard-д `9001`) TCP портыг нээнэ.
Windows (админ PowerShell):

```powershell
New-NetFirewallRule -DisplayName "Mosquitto MQTT" -Direction Inbound -Protocol TCP -LocalPort 1883,9001 -Action Allow
```

ESP32 болон сервер **нэг Wi-Fi сүлжээнд** байх ёстой. Серверийн IP-г `ipconfig`-оор харж ESP32-ийн тохиргооны портал дээр оруулна.

## T2: Topic бүтэц ба эрх

| Хэрэглэгч | Subscribe | Publish |
|---|---|---|
| `esp32` | `intersection/+/command`, `intersection/+/config` | `intersection/+/sensor`, `/state`, `/status` |
| `backend` | `intersection/+/sensor`, `/state`, `/status` | `intersection/+/command`, `/config` |
| `dashboard` | `intersection/+/sensor`, `/state`, `/status` | — |

Anonymous холболт хориотой. ACL-д байхгүй topic руу publish хийвэл broker чимээгүй хаяна.

## T3: Publish/Subscribe урсгал шалгах

```bash
pip install paho-mqtt
python test/pubsub_check.py --host localhost
```

Хүлээгдэх үр дүн:

```
[PASS] Буруу нууц үг татгалзагдана — Not authorized
[PASS] esp32 -> status -> backend
[PASS] esp32 -> sensor -> backend — 2.6 мс
[PASS] backend -> command -> esp32
[PASS] dashboard -> command хориглогдоно (ACL)
[PASS] esp32 тасрахад LWT offline

6/6 шалгалт амжилттай
```

Жинхэнэ ESP32-ийн урсгалыг харах (mosquitto client суусан бол):

```bash
mosquitto_sub -h <broker-ip> -u backend -P <BACKEND_PASS> -t 'intersection/#' -v
mosquitto_pub -h <broker-ip> -u backend -P <BACKEND_PASS> -t intersection/int01/command \
  -m '{"seq":1,"phase":"EW_GREEN","duration_ms":10000,"source":"manual"}'
```

ESP32-ийн Serial Monitor дээр `[MQTT] <- intersection/int01/command {...}` гарч ирвэл command topic-д subscribe хийгдсэн нь баталгаажна.
