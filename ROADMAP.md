# Roadmap

> ⚠️ **Early development.** A direction, not a promise — ordering and scope will change.

**Goal:** feature parity with **MeshOS** (the closed/paid standalone MeshCore firmware) as
an open, MIT alternative — a fully smartphone-independent on-device experience. We take
MeshOS / MeshUltra only as *feature & look references* (no code is copied; they're closed).

Status legend: ✅ done · 🟡 partial · ⏳ planned

---

## UI redesign on LVGL (current focus)

The feature milestones below (M1–M8) were first built in a portable bitmap UI
(`examples/companion_radio/ui-new/`). To reach a professional, Android-app-quality look,
the standalone UI is being **rebuilt on [LVGL](https://lvgl.io)** (`examples/companion_radio/ui-lvgl/`,
desktop prototype in `sim-lvgl/`): black theme, per-feature colour-coded icon chips
(FontAwesome), charts/gauges, and real widgets (switches, sliders, dropdowns, on-screen
keyboard, QR). Design language reference: **MeshUltra** (look only).

- ✅ Shared UI kit, home launcher, chat (list + conversation), contacts (list + ops),
  settings (full field set + keyboard editors + Data export/import), diagnostics
  (noise/signal/heard), stats dashboard (telemetry/noise/radio/core/packet counters),
  repeaters (list + detail gauges), peer-details panel (+ Share/Reset-path/Export/Remove),
  trace, tools/discover (companions/repeaters/rooms/sensors), terminal (console + packet
  monitor) — all live in the LVGL desktop sim. Screen set now mirrors the Android client,
  and the feature panels share a Material panel kit (`lv_ui_md_*`: flat top app bar, solid
  cards, titleMedium headers, label/value rows, cyan accent) matching the Android look. The
  home launcher keeps its glass-card grid style.
- 🟡 **On-device LVGL bring-up** — done on the T-Deck Plus (env
  `LilyGo_TDeck_companion_radio_usb_lvgl`): LVGL 9.2 + LovyanGFX flush + GT911 touch + tick +
  screen-manager all run on hardware (`ui-lvgl/UITask`). Screens are bound to live `MyMesh`
  data through a C view-model bridge (`ui-lvgl/lv_data.h`); home status bar + Heard are live,
  the rest (contacts/stats/chat/repeaters/settings) are being bound incrementally. The bitmap
  UI stays as the fallback for mono / non-touch / companion boards.

The protocol/firmware work from each milestone (config ops, ChatStore, RF hooks, login/
status/candidate cache) is UI-framework-agnostic and carries straight over.

---

## M1 — Settings ✅ (done)
Full on-device companion configuration: data-driven settings UI (Public info, Radio +
region presets, Contacts, Message, Position, Telemetry, Experimental, Device); app-style
look; cycle-or-keyboard input; frequency-band validation. Pending on-hardware validation.

## M2 — Messaging & chat  🟡 (core done; pending on-hardware validation)
A smartphone-style chat, fully on-device.
- ✅ **Channels + DMs tabs** — channels-first (Public + configured), plus a DM list with
  last-message previews + unread badges (lists from live mesh enumeration).
- ✅ **Speech-bubble threads**, scrollable, **colour-coded usernames** (hashed palette).
- ✅ **Delivery status** — sending/sent/delivered/failed (red), via the ACK path
  (`sendTextTo` → `expected_ack` → `onMsgSendConfirmed`); send timeout → failed.
- ✅ **Compose** via physical keyboard + on-screen keyboard; word-wrapped bubbles.
- ✅ Volatile in-RAM `ChatStore` (threads lost on reboot; see M7 for persistence).
- ✅ **Tap-to-reply** quoting — tap a bubble to quote it; quoted lines render dimmed.
- ✅ **Emoji** — curated emoticon set as inline monochrome glyphs + picker (sent over-air
  as shortcodes like `:)`; word-boundary matched so URLs stay intact).
- ✅ **URL → QR code** — tap a message containing a URL to show it as a scannable
  on-screen QR (vendored MIT qrcodegen; see `THIRD_PARTY.md`).
- ⏳ **Contact-type-aware** room post/read niceties (rooms currently treated as DMs).

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

## M5 — Remote infrastructure / repeater management  🟡 (core done; pending on-hardware validation)
- ✅ **Remote login** (password) to repeaters/rooms → pull **stats** (parsed RepeaterStats:
  battery, uptime, airtime tx/rx, packet counts, noise, RSSI/SNR, errors) via
  `uiLogin`/`uiRequestStatus` + new `onLoginResult`/`onStatusResponse` hooks.
- ✅ **Repeater scanner + whitelist** — Saved | Scan tabs; heard-but-unsaved nodes
  (new candidate cache) can be added; favourite flag = whitelist star.
- ✅ **Remote triggers** — send `advert` / `clock sync` CLI commands over the air; the
  repeater's CLI reply is surfaced (`onCommandReply`).
- ⏳ Remote neighbour-list view (repeaters don't return neighbours in the status blob;
  would need a dedicated request).

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
M1 settings; the MeshOS-style cyan-on-black icon-grid launcher home with status bars; all of
M3 (packet monitor, noise scope, last-heard, signal meter, trace route); M2 chat (Channels/DMs
tabs, speech-bubble threads, colour-coded names, delivery status, compose, tap-to-reply, inline
emoji, URL→QR); the M5 core (remote login, repeater stats, scanner/whitelist, remote triggers);
and the desktop simulator.
