# Roadmap

> ⚠️ **Early development.** A direction, not a promise — ordering and scope will change.

**Goal:** feature parity with **MeshOS** (the closed/paid standalone MeshCore firmware) as
an open, MIT alternative — a fully smartphone-independent on-device experience. We take
MeshOS only as a *feature reference* (no code is copied; it's closed).

Status legend: ✅ done · 🟡 partial · ⏳ planned

---

## M1 — Settings ✅ (done)
Full on-device companion configuration: data-driven settings UI (Public info, Radio +
region presets, Contacts, Message, Position, Telemetry, Experimental, Device); app-style
look; cycle-or-keyboard input; frequency-band validation. Pending on-hardware validation.

## M2 — Messaging & chat (next, headline)
A smartphone-style chat, fully on-device.
- ⏳ **Channels + DMs tabs** — channels-first (Public + configured), plus a DM list.
- ⏳ **Speech-bubble threads**, scrollable, **colour-coded usernames**.
- ⏳ **Contact-type-aware** actions (DM users; rooms post/read; — repeaters/sensors handled in M5/M3).
- ⏳ **Delivery status** — sent/delivered/failed (red ✗), using the ACK path
  (`expected_ack`, `PUSH_CODE_SEND_CONFIRMED`).
- ⏳ **Compose** via keyboard/on-screen keyboard; **long-press-to-reply**.
- ⏳ **Emoji** support (needs a glyph set; scope to a common subset first).
- ⏳ **URL → QR code** on-screen (needs a small MIT QR encoder lib).
- History is volatile in-RAM initially (see M7 for persistence).

## M3 — RF diagnostics & tools  ✅ (done; pending on-hardware validation)
- ✅ **Packet monitor** — decoded raw packet stream + per-packet detail.
- ✅ **Live noise scope** — scrolling graph of noise-floor history (`radio_driver.getNoiseFloor()`).
- ✅ **Last-heard list** — recent stations with SNR/RSSI + estimated distance
  (extended `AdvertPath`/`getRecentlyHeard`; haversine from advert lat/lon vs our GPS).
- ✅ **Signal meter** — per-repeater mesh-coverage bars + last-heard age.
- ✅ **Trace-route tool** — hop-by-hop path with per-hop SNR
  (`MyMesh::sendTrace` + `onTraceResult`).

## M3.5 — MeshOS-style home & theme ✅ (done; pending on-hardware validation)
Delivered alongside M3 to host the diagnostic tiles:
- ✅ **Icon-grid launcher** home (4-col tiles, line-art XBM icons + labels), replacing the
  paginated home; future tiles (Chat/Contacts/Repeaters/Finder/Map) shown dimmed.
- ✅ **Cyan-on-black theme** (shared `UIStyle.h` palette; settings + packet monitor inherit it).
- ✅ **Status bars** — top (menu · channel · clock), bottom (node · device · signal bars · battery).
- ✅ **Hamburger menu** — Send advert · Bluetooth · Reboot · Shutdown.

## M4 — GPS & offline mapping
- ⏳ **Offline maps** from SD-card tiles (needs tile renderer + projection).
- ⏳ **Node/repeater plotting** from advert lat/lon (`onAdvertRecv`).
- ⏳ **Geographic route trace** (ties to M3 trace) + **share location** over RF.
- ⏳ Touch/trackball pan/zoom.
- Requires the SD card (T-Deck Plus has one) — gate behind storage availability.

## M5 — Remote infrastructure / repeater management
- ⏳ **Remote login** (password) to repeaters/rooms → pull **stats / neighbour lists**
  (`CMD_SEND_LOGIN`, `SEND_STATUS_REQ`, `getNeighbours`).
- ⏳ **Repeater scanner + whitelist** filtering (build on auto-add config) to avoid
  flooding the contact list.
- ⏳ **Remote triggers** — force a repeater to sync clock / send an advert, over the air.

## M6 — Terminal & system
- ⏳ **On-device terminal** — multi-colour MeshCore CLI client (run `advert`, `sd ls`,
  inspect packet stream); builds on the existing CLI-rescue path + `onCommandDataRecv`.
- ⏳ **Notifications** — alerts, optional audio tones (boards with a buzzer), and
  configurable screen-wake / backlight-dim rules (extend the auto-off logic).
- ⏳ **Fast boot** — trim the cold-boot path (MeshOS targets ~4 s).
- ✅ **Security** — end-to-end encryption is inherent to the MeshCore stack.

## M7 — Persistence
- ⏳ Optional **SD/flash storage** for message history, drafts, and last-heard, surviving
  reboot — enhancement on top of the volatile baseline; gated by storage availability.

## M8 — Connectivity (advanced)
- ⏳ **Unified transport image** (BLE + USB + Wi-Fi in one build) and optional **AirLink**
  bridging (relay a phone into the mesh without tethering). Largest/most speculative.

## Cross-cutting
- Portability across boards (T-Deck/T-Deck Plus first; keep the UI abstraction clean).
- Everything validated on real hardware before being called stable.
- New deps must be MIT-compatible (e.g. QR encoder); no GPL code pulled in.

## Done so far
M1 settings, region presets, frequency validation, desktop simulator; the MeshOS-style
cyan-on-black icon-grid launcher home with status bars; and all of M3 (packet monitor, noise
scope, last-heard, signal meter, trace route).
