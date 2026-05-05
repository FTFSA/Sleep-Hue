"""Sleep-Hue CLI: bedtime wind-down via Philips Hue v2 API."""
import argparse
import json
import os
import ssl
import sys
import urllib.error
import urllib.request

CONFIG_PATH = "config.json"
ENV_PATH = ".env"


def load_env():
    if os.path.exists(ENV_PATH):
        with open(ENV_PATH) as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))
    ip = os.environ.get("HUE_BRIDGE_IP")
    key = os.environ.get("HUE_APP_KEY")
    if not ip or not key:
        sys.exit("error: set HUE_BRIDGE_IP and HUE_APP_KEY (env or .env)")
    return ip, key


class Bridge:
    def __init__(self, ip, key):
        self.ip = ip
        self.key = key
        # Hue bridges serve a self-signed cert tied to the bridge ID; verifying
        # it requires pinning the Hue root CA, out of scope for this CLI.
        self.ctx = ssl._create_unverified_context()

    def _req(self, method, path, body=None):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(
            f"https://{self.ip}{path}",
            data=data,
            method=method,
            headers={
                "hue-application-key": self.key,
                "Content-Type": "application/json",
            },
        )
        with urllib.request.urlopen(req, context=self.ctx, timeout=5) as resp:
            return json.loads(resp.read())

    def get(self, path):
        return self._req("GET", path)["data"]

    def put(self, path, body):
        return self._req("PUT", path, body)

    def find_room(self, name):
        for r in self.get("/clip/v2/resource/room"):
            if r["metadata"]["name"] == name:
                return r
        sys.exit(f"error: room {name!r} not found on bridge")

    def find_scene(self, room_id, name):
        for s in self.get("/clip/v2/resource/scene"):
            if s["group"]["rid"] == room_id and s["metadata"]["name"] == name:
                return s
        sys.exit(f"error: scene {name!r} not found in room {room_id}")

    def grouped_light_id(self, room):
        for svc in room["services"]:
            if svc["rtype"] == "grouped_light":
                return svc["rid"]
        sys.exit("error: room has no grouped_light service")

    def activate_scene(self, scene_id, duration_ms):
        return self.put(
            f"/clip/v2/resource/scene/{scene_id}",
            {"recall": {"action": "active", "duration": duration_ms}},
        )

    def set_grouped_light_off(self, gl_id):
        return self.put(
            f"/clip/v2/resource/grouped_light/{gl_id}",
            {"on": {"on": False}},
        )


def cmd_wind_down(b, cfg):
    room = b.find_room(cfg["room"])
    scene = b.find_scene(room["id"], cfg["wind_down"]["scene"])
    duration = cfg["wind_down"]["duration_ms"]
    b.activate_scene(scene["id"], duration)
    print(f"wind-down: {cfg['room']} -> {cfg['wind_down']['scene']} over {duration/1000:.0f}s")


def cmd_off(b, cfg):
    room = b.find_room(cfg["room"])
    b.set_grouped_light_off(b.grouped_light_id(room))
    print(f"off: {cfg['room']}")


def cmd_discover(b, cfg):
    rooms = {r["id"]: r["metadata"]["name"] for r in b.get("/clip/v2/resource/room")}
    print(f"\nROOMS ({len(rooms)}):")
    for rid, name in rooms.items():
        print(f"  {rid}  {name}")
    scenes = b.get("/clip/v2/resource/scene")
    print(f"\nSCENES ({len(scenes)}):")
    for s in scenes:
        room = rooms.get(s["group"]["rid"], "?")
        print(f"  {room:10s}  {s['id']}  {s['metadata']['name']}")


COMMANDS = {
    "wind-down": cmd_wind_down,
    "off": cmd_off,
    "discover": cmd_discover,
}


def main():
    p = argparse.ArgumentParser(prog="sleep_hue", description=__doc__)
    p.add_argument("cmd", choices=COMMANDS, help="action to run")
    args = p.parse_args()

    ip, key = load_env()
    with open(CONFIG_PATH) as f:
        cfg = json.load(f)

    try:
        COMMANDS[args.cmd](Bridge(ip, key), cfg)
    except urllib.error.HTTPError as e:
        sys.exit(f"bridge HTTP {e.code}: {e.read().decode(errors='replace')}")
    except urllib.error.URLError as e:
        sys.exit(f"bridge unreachable at {ip}: {e.reason}")


if __name__ == "__main__":
    main()
