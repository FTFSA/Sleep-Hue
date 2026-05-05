"""Local-only Hue bridge discovery. Gitignored — do not commit."""
import json
import ssl
import sys
import urllib.request

BRIDGE_IP = "192.168.1.68"
BRIDGE_ID = "c42996fffe685bca"
APP_KEY = "TVtFJ6IPtMIhrYoXCw7hFgIf4jnSrLAay5W9ObzR"

CTX = ssl._create_unverified_context()


def get(path):
    req = urllib.request.Request(
        f"https://{BRIDGE_IP}{path}",
        headers={"hue-application-key": APP_KEY},
    )
    with urllib.request.urlopen(req, context=CTX, timeout=5) as r:
        return json.loads(r.read())["data"]


def main():
    print(f"Bridge {BRIDGE_ID} @ {BRIDGE_IP}\n")

    rooms = get("/clip/v2/resource/room")
    print(f"Rooms ({len(rooms)}):")
    for r in rooms:
        print(f"  {r['id']}  {r['metadata']['name']}  ({len(r['children'])} devices)")

    zones = get("/clip/v2/resource/zone")
    print(f"\nZones ({len(zones)}):")
    for z in zones:
        print(f"  {z['id']}  {z['metadata']['name']}")

    lights = get("/clip/v2/resource/light")
    print(f"\nLights ({len(lights)}):")
    for l in lights:
        on = "on " if l["on"]["on"] else "off"
        print(f"  {l['id']}  {l['metadata']['name']:30s}  {on}")

    scenes = get("/clip/v2/resource/scene")
    print(f"\nScenes ({len(scenes)}):")
    for s in scenes:
        group = s["group"]["rid"][:8]
        print(f"  {s['id']}  {s['metadata']['name']:30s}  (room {group}…)")


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code}: {e.read().decode()}", file=sys.stderr)
        sys.exit(1)
