# MQTT Topic Structure

[← Docs index](README.md)

## Per-Device Topics

- **State** (subscribe): `home/<device>/stat/POWER` — current ON/OFF (retained)
- **Command** (publish): `home/<device>/cmnd/POWER` — toggle command
- **Dimmer** (publish): `home/<device>/cmnd/Dimmer` — brightness 0–100

## Panel Topics

- `sc01/status` — Panel heartbeat: `online` / `offline` (LWT, retained), re-published every 30s
- `homebridge/#` — Wildcard for HomeKit bridge compatibility

## Homebridge MQTTThing Compatibility

- Device payloads use `ON`/`OFF` matching MQTTThing `onValue`/`offValue`
- Panel status exposed as **switch** type using `sc01/status` topic (`onValue: "online"`, `offValue: "offline"`)
- Retained heartbeat every 30s keeps Homebridge status in sync

## Payload Formats

Accepted on the state topic:

`ON`/`OFF`, `1`/`0`, `true`/`false`, JSON `{"POWER":"ON"}`, `{"state":true}`,
`{"Status":{"Power":1}}`

## Offline Behaviour

When the broker is unreachable, commands go into a 16-slot ring buffer and
replay on reconnect with exponential backoff (5s → 60s). Implementation:
`src/mqtt_manager.cpp`.
