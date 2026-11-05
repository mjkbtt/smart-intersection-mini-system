# Интерфейсийн тодорхойлолт (MQTT + REST API)

> Issue: [1.6.1] Архитектур ба интерфейс тодорхойлох (#1)
> Хувилбар: v1.0 — бүх модуль (firmware, backend, dashboard) энэ баримтыг дагана.
> Өөрчлөлт оруулах бол эхлээд энэ файлыг шинэчилж, багтаа мэдэгдэнэ.

## 1. Ерөнхий дүрэм

| Зүйл | Утга |
|---|---|
| Broker | Mosquitto 2.x, порт `1883` (MQTT), `9001` (WebSocket, dashboard-д) |
| Topic угтвар | `intersection/<intersection_id>/...` |
| `intersection_id` | `int01` (макет нэг уулзвартай; цаашид `int02`… нэмэгдэж болно) |
| Payload | UTF-8 JSON, түлхүүр нь `snake_case` |
| Чиглэлийн код | `N` (хойд), `E` (зүүн), `S` (урд), `W` (баруун) |
| Гэрлийн код | `R` (улаан), `Y` (шар), `G` (ногоон), `OFF` |
| Цаг (`ts`) | Unix epoch, **миллисекунд**. ESP32 NTP-гүй үед `uptime_ms` илгээж, `ts`-ийг backend хүлээн авсан цагаар нөхнө |
| `seq` | Илгээгч бүрийн 0-ээс өсөх тоолуур — мессеж алдагдал/давхардлыг илрүүлэхэд (#10) |

## 2. MQTT topic бүтэц

| Topic | Чиглэл | QoS | Retain | Давтамж |
|---|---|---|---|---|
| `intersection/int01/sensor` | ESP32 → Backend | 0 | үгүй | 500 мс тутам + төлөв өөрчлөгдөх бүрт |
| `intersection/int01/state` | ESP32 → Backend | 0 | тийм | гэрэл солигдох бүрт |
| `intersection/int01/status` | ESP32 → Backend | 0 (LWT: 1) | тийм | холбогдоход + 30 с тутам heartbeat, LWT |
| `intersection/int01/command` | Backend → ESP32 | 1 | үгүй | алгоритм шийдвэр гаргах бүрт |
| `intersection/int01/config` | Backend → ESP32 | 1 | тийм | тохиргоо өөрчлөгдөхөд |

> ESP32 талын PubSubClient сан зөвхөн QoS 0-оор publish хийдэг (subscribe нь QoS 1 хүртэл).
> Иймд ESP32 → Backend мессежийн алдагдлыг `seq` талбараар илрүүлнэ; `state`, `status` нь retain тул
> backend дахин холбогдоход хамгийн сүүлийн утгыг шууд авна.

Wildcard жишээ: backend `intersection/+/sensor`, `intersection/+/state`, `intersection/+/status`-д subscribe хийнэ.

### 2.1 `sensor` — мэдрэгчийн өгөгдөл

```json
{
  "device_id": "esp32-int01",
  "seq": 1542,
  "uptime_ms": 734512,
  "lanes": {
    "N": { "present": true,  "count": 2 },
    "E": { "present": false, "count": 0 },
    "S": { "present": true,  "count": 1 },
    "W": { "present": false, "count": 0 }
  }
}
```

- `present` — IR мэдрэгч яг одоо машин мэдэрч байгаа эсэх.
- `count` — өмнөх `sensor` мессежээс хойш тухайн чиглэлд шинээр ирсэн машины тоо (debounce хийгдсэн).

### 2.2 `state` — гэрлийн бодит төлөв

```json
{
  "device_id": "esp32-int01",
  "seq": 88,
  "uptime_ms": 734600,
  "phase": "NS_GREEN",
  "lights": { "N": "G", "S": "G", "E": "R", "W": "R" },
  "mode": "auto",
  "cmd_seq": 41
}
```

- `phase` — `NS_GREEN`, `NS_YELLOW`, `EW_GREEN`, `EW_YELLOW`, `ALL_RED`, `FLASH_YELLOW`.
- `mode` — `auto` (backend удирдаж байна), `fallback` (backend/broker тасарсан, ESP32 тогтмол хугацаат горимд — #15), `manual` (dashboard-аас гараар).
- `cmd_seq` — хамгийн сүүлд гүйцэтгэсэн командын `seq` (командын хариу баталгаа).

### 2.3 `status` — төхөөрөмжийн амьд эсэх (heartbeat + LWT)

```json
{ "device_id": "esp32-int01", "status": "online", "ip": "192.168.1.50",
  "rssi": -61, "uptime_ms": 734512, "fw": "0.2.0" }
```

ESP32 холбогдохдоо Last Will-ийг `{"device_id":"esp32-int01","status":"offline"}` (retain, QoS 1) гэж бүртгүүлнэ. Холболт гэнэт тасрахад broker үүнийг автоматаар нийтэлнэ.

### 2.4 `command` — гэрэл солих тушаал

```json
{ "seq": 42, "phase": "EW_GREEN", "duration_ms": 12000, "source": "algorithm" }
```

- Backend зөвхөн **ногоон фаз** (`NS_GREEN`, `EW_GREEN`) эсвэл `ALL_RED`, `FLASH_YELLOW` илгээнэ.
- Шар гэрэл ба бүгд-улаан завсрыг (`yellow_ms`, `all_red_ms`) ESP32 өөрөө оруулна — ингэснээр зөрчилтэй ногоон гэрэл хэзээ ч асахгүй.
- `duration_ms` хугацаанд дараагийн команд ирэхгүй бол ESP32 тухайн фазыг үргэлжлүүлж, `command_timeout_ms` хэтэрвэл `fallback` горимд шилжинэ.
- `source` — `algorithm` | `manual` (dashboard).

### 2.5 `config` — ESP32 тохиргоо

```json
{ "yellow_ms": 3000, "all_red_ms": 1000, "min_green_ms": 5000,
  "max_green_ms": 30000, "command_timeout_ms": 10000,
  "fallback_cycle": { "NS_GREEN": 15000, "EW_GREEN": 15000 } }
```

## 3. Broker хэрэглэгч ба эрх

Дэлгэрэнгүйг [`broker/`](../broker/README.md)-с харна уу.

| Хэрэглэгч | Унших (subscribe) | Бичих (publish) |
|---|---|---|
| `esp32` | `command`, `config` | `sensor`, `state`, `status` |
| `backend` | `sensor`, `state`, `status` | `command`, `config` |
| `dashboard` | `sensor`, `state`, `status` | — |

## 4. REST API (Backend)

Үндсэн хаяг: `http://<server>:8000/api/v1`. Хариу бүр JSON. Алдааны формат:

```json
{ "error": { "code": "NOT_FOUND", "message": "intersection int09 not found" } }
```

| Method | Endpoint | Тайлбар | Хэрэглэгч |
|---|---|---|---|
| GET | `/health` | Сервер, broker, DB холболтын төлөв | бүгд |
| GET | `/intersections` | Уулзваруудын жагсаалт ба online эсэх | Dashboard |
| GET | `/intersections/{id}/state` | Одоогийн фаз, гэрэл, горим, чиглэл бүрийн ачаалал | Dashboard (#16) |
| GET | `/intersections/{id}/traffic?from=&to=&bucket=1m` | Түүхэн машины тоо (хугацааны интервалаар) | Dashboard (#17) |
| GET | `/intersections/{id}/events?from=&to=&limit=` | Фаз солигдсон түүх | Dashboard (#17) |
| GET | `/intersections/{id}/alerts?active=true` | Анхааруулга (offline, мэдрэгчийн алдаа, fallback) | Dashboard (#17) |
| POST | `/intersections/{id}/command` | Гараар фаз сонгох (`manual` горим) | Dashboard |
| PUT | `/intersections/{id}/mode` | `{"mode":"auto"\|"manual"}` | Dashboard |
| GET / PUT | `/intersections/{id}/config` | Хугацааны тохиргоо (§2.5); PUT нь `config` topic руу нийтэлнэ | Dashboard |

### Жишээ: `GET /intersections/int01/state`

```json
{
  "id": "int01",
  "online": true,
  "mode": "auto",
  "phase": "NS_GREEN",
  "lights": { "N": "G", "S": "G", "E": "R", "W": "R" },
  "phase_remaining_ms": 7400,
  "load": { "N": 3, "E": 0, "S": 1, "W": 5 },
  "updated_at": "2026-10-28T10:15:02.120Z"
}
```

### Жишээ: `POST /intersections/int01/command`

Хүсэлт:

```json
{ "phase": "EW_GREEN", "duration_ms": 10000 }
```

Хариу `202 Accepted`:

```json
{ "seq": 57, "published": true }
```

## 5. Хилийн заагууд (хэн юуг хариуцах вэ)

- **Firmware (ESP32):** мэдрэгч уншиж debounce хийх, LED-ийн аюулгүй дараалал (шар/бүгд-улаан), Wi-Fi ба MQTT-ийн дахин холболт, fallback горим.
- **Backend:** MQTT subscriber, өгөгдөл DB-д хадгалах, гэрэл удирдах алгоритм, REST API, alert үүсгэх.
- **Dashboard:** зөвхөн REST API (ба хүсвэл WebSocket-оор `dashboard` эрхтэй MQTT)-ээр өгөгдөл авна; ESP32 руу шууд хандахгүй.
