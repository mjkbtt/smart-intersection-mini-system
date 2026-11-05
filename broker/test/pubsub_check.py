"""Broker-ын publish/subscribe урсгал ба эрхийг шалгах скрипт (#9, T3).

ESP32-ийг дуурайлган `esp32` хэрэглэгчээр, серверийг `backend` хэрэглэгчээр
холбогдож дараахыг шалгана:

  1. Буруу нууц үгээр холбогдох боломжгүй
  2. esp32 -> status (retain)  -> backend хүлээн авна
  3. esp32 -> sensor           -> backend хүлээн авна
  4. backend -> command        -> esp32 хүлээн авна
  5. dashboard -> command      -> ACL хориглож, esp32-д хүрэхгүй
  6. esp32 гэнэт тасрахад LWT "offline" -> backend хүлээн авна

Ашиглах:
    pip install paho-mqtt
    python broker/test/pubsub_check.py --host 192.168.1.10
Нууц үгийг broker/users.env-ээс уншина (эсвэл --env-аар өөр файл заана).
"""
import argparse
import json
import queue
import sys
import time
from pathlib import Path

import paho.mqtt.client as mqtt
from paho.mqtt.packettypes import PacketTypes
from paho.mqtt.reasoncodes import ReasonCode

TIMEOUT = 3.0


def read_env(path: Path) -> dict:
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


class Peer:
    """Нэг MQTT client; ирсэн мессежүүдийг queue-д хийнэ."""

    def __init__(self, args, user, password, client_id, will_topic=None):
        self.inbox = queue.Queue()
        self.connected = queue.Queue()
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id,
                                  protocol=mqtt.MQTTv5)
        self.client.username_pw_set(user, password)
        if will_topic:
            self.client.will_set(will_topic, json.dumps({"device_id": client_id, "status": "offline"}),
                                 qos=1, retain=True)
        self.client.on_connect = lambda c, u, f, rc, p: self.connected.put(rc)
        self.client.on_message = lambda c, u, m: self.inbox.put((m.topic, m.payload.decode()))
        self.client.connect(args.host, args.port, keepalive=30)
        self.client.loop_start()
        rc = self.connected.get(timeout=TIMEOUT)
        if rc.is_failure:
            self.client.loop_stop()
            raise ConnectionRefusedError(str(rc))

    def subscribe(self, topic):
        self.client.subscribe(topic, qos=1)
        time.sleep(0.3)

    def publish(self, topic, payload, qos=1, retain=False):
        self.client.publish(topic, json.dumps(payload), qos=qos, retain=retain).wait_for_publish(TIMEOUT)

    def expect(self, topic, timeout=TIMEOUT):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                got_topic, payload = self.inbox.get(timeout=max(0.05, deadline - time.time()))
            except queue.Empty:
                break
            if got_topic == topic:
                return json.loads(payload)
        return None

    def close(self, with_will=False):
        if with_will:
            # MQTT v5: 0x04 "Disconnect with Will Message" — тасралтыг дуурайна
            self.client.disconnect(reasoncode=ReasonCode(PacketTypes.DISCONNECT, "Disconnect with will message"))
        else:
            self.client.disconnect()
        self.client.loop_stop()


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")  # Windows console (cp1252) дээр кирилл хэвлэх
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--id", default="test01", help="intersection_id (жинхэнэ int01-ийг бохирдуулахгүйн тулд test01)")
    parser.add_argument("--env", type=Path, default=Path(__file__).resolve().parents[1] / "users.env")
    args = parser.parse_args()

    env = read_env(args.env)
    base = f"intersection/{args.id}"
    results = []

    def check(name, ok, detail=""):
        results.append(ok)
        print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" — {detail}" if detail else ""))

    # 1. Буруу нууц үг
    try:
        Peer(args, "esp32", "wrong-password", "pubsub-check-bad").close()
        check("Буруу нууц үг татгалзагдана", False, "холбогдчихлоо")
    except (ConnectionRefusedError, queue.Empty) as exc:
        check("Буруу нууц үг татгалзагдана", True, str(exc) or "timeout")

    backend = Peer(args, "backend", env["BACKEND_PASS"], "pubsub-check-backend")
    backend.subscribe(f"{base}/sensor")
    backend.subscribe(f"{base}/status")
    esp = Peer(args, "esp32", env["ESP32_PASS"], "pubsub-check-esp32", will_topic=f"{base}/status")
    esp.subscribe(f"{base}/command")
    dashboard = Peer(args, "dashboard", env["DASHBOARD_PASS"], "pubsub-check-dashboard")

    # 2. status (retain)
    esp.publish(f"{base}/status", {"device_id": "pubsub-check-esp32", "status": "online"}, retain=True)
    msg = backend.expect(f"{base}/status")
    check("esp32 -> status -> backend", bool(msg) and msg.get("status") == "online", str(msg))

    # 3. sensor + хоцрогдол
    t0 = time.perf_counter()
    esp.publish(f"{base}/sensor", {"device_id": "pubsub-check-esp32", "seq": 1,
                                   "lanes": {"N": {"present": True, "count": 1}}}, qos=0)
    msg = backend.expect(f"{base}/sensor")
    check("esp32 -> sensor -> backend", bool(msg) and msg.get("seq") == 1,
          f"{(time.perf_counter() - t0) * 1000:.1f} мс")

    # 4. command
    backend.publish(f"{base}/command", {"seq": 1, "phase": "EW_GREEN", "duration_ms": 10000, "source": "algorithm"})
    msg = esp.expect(f"{base}/command")
    check("backend -> command -> esp32", bool(msg) and msg.get("phase") == "EW_GREEN", str(msg))

    # 5. ACL: dashboard command бичих эрхгүй
    dashboard.publish(f"{base}/command", {"seq": 999, "phase": "NS_GREEN", "duration_ms": 1, "source": "manual"})
    msg = esp.expect(f"{base}/command", timeout=1.5)
    check("dashboard -> command хориглогдоно (ACL)", msg is None, str(msg) if msg else "")

    # 6. LWT
    esp.close(with_will=True)
    msg = backend.expect(f"{base}/status")
    check("esp32 тасрахад LWT offline", bool(msg) and msg.get("status") == "offline", str(msg))

    # Цэвэрлэгээ: test topic-ийн retain мессежийг устгана
    backend.close()
    dashboard.close()
    cleaner = Peer(args, "esp32", env["ESP32_PASS"], "pubsub-check-cleaner")
    cleaner.client.publish(f"{base}/status", b"", qos=1, retain=True).wait_for_publish(TIMEOUT)
    cleaner.close()

    passed = sum(results)
    print(f"\n{passed}/{len(results)} шалгалт амжилттай")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
