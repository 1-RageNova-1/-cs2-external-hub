#!/usr/bin/env python3
"""
RageNova VPS offset auto-sync

Polls public CS2 dumps, writes YOUR private offsets.json.
Run on the VPS via cron every 30-60 min. Serve offsets.json only to your cheat
(firewall / basic auth / secret path) — never make it a public index page.

Usage:
  python3 vps_offset_sync.py
  python3 vps_offset_sync.py --out /var/www/ragenova/offsets.json

Cron example (every 30 min):
  */30 * * * * /usr/bin/python3 /opt/ragenova/vps_offset_sync.py >> /var/log/ragenova_offsets.log 2>&1
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.request
from pathlib import Path

# Public sources (used ONLY on your VPS — clients never hit these)
SOURCES = [
    # sezzyaep / exitscam style (often faster than a2x)
    "https://raw.githubusercontent.com/sezzyaep/CS2-OFFSETS/main/offsets.hpp",
    "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.hpp",
]

CLIENT_DLL_SOURCES = [
    "https://raw.githubusercontent.com/sezzyaep/CS2-OFFSETS/main/client_dll.hpp",
    "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/client_dll.hpp",
]


def fetch(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "RageNova-OffsetSync/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.read().decode("utf-8", errors="replace")


def parse_hpp_offset(text: str, name: str) -> int | None:
    # constexpr std::ptrdiff_t dwViewAngles = 0x2572AE8;
    import re
    m = re.search(rf"{name}\s*=\s*(0x[0-9A-Fa-f]+|\d+)", text)
    if not m:
        return None
    return int(m.group(1), 0)


def parse_build(text: str) -> str:
    import re
    m = re.search(r"Build No:\s*(\d+)", text)
    if m:
        return m.group(1)
    m = re.search(r"build[_\s-]*number[\"'\s:=]+(\d+)", text, re.I)
    if m:
        return m.group(1)
    return "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="offsets.json", help="output path for private offsets.json")
    args = ap.parse_args()

    view = None
    ident = None
    punch = None
    build = "unknown"
    used = None

    for url in SOURCES:
        try:
            text = fetch(url)
            v = parse_hpp_offset(text, "dwViewAngles")
            if v is None:
                continue
            view = v
            build = parse_build(text)
            used = url
            break
        except Exception as e:
            print(f"skip {url}: {e}", file=sys.stderr)

    for url in CLIENT_DLL_SOURCES:
        try:
            text = fetch(url)
            # prefer C_CSPlayerPawn field (last strong match near m_iIDEntIndex)
            import re
            matches = re.findall(r"m_iIDEntIndex\s*=\s*(0x[0-9A-Fa-f]+)", text)
            if matches:
                # C_CSPlayerPawn value is typically the one in the 0x36xx range
                for m in matches:
                    val = int(m, 0)
                    if 0x3000 <= val <= 0x5000:
                        ident = val
                        break
                if ident is None:
                    ident = int(matches[-1], 0)
            matches = re.findall(r"m_aimPunchAngle\s*=\s*(0x[0-9A-Fa-f]+)", text)
            if matches:
                for m in matches:
                    val = int(m, 0)
                    if 0x1000 <= val <= 0x3000:
                        punch = val
                        break
            break
        except Exception as e:
            print(f"skip client {url}: {e}", file=sys.stderr)

    if view is None and ident is None:
        print("failed to resolve any offsets", file=sys.stderr)
        return 1

    payload = {
        "build": build,
        "source": used,
        "dwViewAngles": hex(view) if view is not None else None,
        "m_iIDEntIndex": hex(ident) if ident is not None else None,
        "m_aimPunchAngle": hex(punch) if punch is not None else None,
    }
    # drop nulls
    payload = {k: v for k, v in payload.items() if v is not None}

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"wrote {out}: {payload}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
