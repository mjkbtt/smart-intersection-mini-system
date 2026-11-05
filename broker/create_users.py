"""Mosquitto-ийн нууц үгийн файл (config/passwd)-ийг users.env-ээс үүсгэнэ.

Ашиглах:
    python create_users.py            # users.env-ийг уншина
    python create_users.py other.env

`mosquitto_passwd` PATH дээр байвал түүнийг, үгүй бол Docker image-ийг ашиглана.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CONFIG = HERE / "config"
PASSWD = CONFIG / "passwd"
IMAGE = "eclipse-mosquitto:2.0.20"
USERS = {"esp32": "ESP32_PASS", "backend": "BACKEND_PASS", "dashboard": "DASHBOARD_PASS"}


def read_env(path: Path) -> dict:
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")  # Windows console (cp1252) дээр кирилл хэвлэх
    env_path = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "users.env"
    if not env_path.exists():
        print(f"{env_path} олдсонгүй. users.env.example-ийг users.env болгон хуулж нууц үгээ оруулна уу.")
        return 1
    env = read_env(env_path)
    missing = [var for var in USERS.values() if not env.get(var)]
    if missing:
        print("Дутуу утга: " + ", ".join(missing))
        return 1
    weak = [var for var in USERS.values() if env[var].startswith("change-me")]
    if weak:
        print("Анхааруулга: жишээ нууц үг хэвээр байна: " + ", ".join(weak))

    PASSWD.unlink(missing_ok=True)
    PASSWD.touch()
    native = shutil.which("mosquitto_passwd")
    for user, var in USERS.items():
        if native:
            cmd = [native, "-b", str(PASSWD), user, env[var]]
        else:
            cmd = ["docker", "run", "--rm", "-v", f"{CONFIG}:/mosquitto/config", IMAGE,
                   "mosquitto_passwd", "-b", "/mosquitto/config/passwd", user, env[var]]
        subprocess.run(cmd, check=True)
        print(f"  + {user}")
    if os.name != "nt":  # Linux дээр mosquitto 2.x world-readable файлд анхааруулдаг
        os.chmod(PASSWD, 0o700)
        os.chmod(CONFIG / "acl", 0o700)
    print(f"Үүслээ: {PASSWD}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
