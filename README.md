# argus

esp32-based door sensor that reports state changes via HTTP webhook

## Setup

Copy the example config file:

```bash
cp main/config.h.example main/config.h
```

Then edit `main/config.h` with your settings:

- `WIFI_SSID` - Your WiFi network name
- `WIFI_PASS` - Your WiFi password
- `STATE_CHANGE_URL` - Webhook endpoint URL for state change notifications
- `WEBHOOK_SECRET` - Secret token sent in `x-webhook-secret` header
- `DOOR_ID` - Identifier for this door sensor

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```

### Monitor

```bash
idf.py monitor
```
