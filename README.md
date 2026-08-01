# ESP32 Marauder Web Controller

Webové rozhraní připravené pro GitHub a Railway. Podporuje USB Web Serial a zabezpečený cloudový most, přes který ESP32 odesílá telemetrii a přijímá bezpečné příkazy.

## Railway

1. V Railway zvolte **New Project → Deploy from GitHub repo**.
2. Vyberte `martypetrzel-lab/web-hack`.
3. Railway načte `railway.json`, spustí `npm start` a kontroluje `/health`.
4. Nastavte proměnnou `DEVICE_API_KEY` na dlouhý náhodný klíč.
5. V **Settings → Networking** vygenerujte veřejnou doménu.

Stejný klíč vložte do firmwaru jako `API_KEY`. Klíč neukládejte do veřejného repozitáře.

Bezpečný referenční firmware je v `firmware/heltec_v4_cloud_bridge_safe.ino`. Před nahráním doplňte Wi‑Fi údaje, Railway URL a API klíč.

## Cloudové API

- `POST /api/data` – ESP odešle JSON telemetrii; server vrátí čekající příkaz.
- `GET /api/data` – web načte poslední stav ESP.
- `POST /api/command` – web zařadí příkaz `scan`, `lorascan` nebo `stop`.
- `GET /health` – kontrola Railway služby.

ESP používá hlavičku `X-API-Key`, web `X-Control-Key`.

## Připojení ESP32

### USB Serial (současný firmware)

- Otevřete web v Chrome nebo Edge na počítači.
- Připojte ESP32 přes USB a klikněte na **Připojit USB**.
- Vyberte sériový port ESP32.
- Web posílá stejné příkazy jako sériový monitor: `W`, `S`, mezerník.

Web Serial vyžaduje HTTPS nebo localhost a nefunguje v Safari/Firefox ani běžně na iOS.

### LAN API (budoucí firmware)

Firmware musí implementovat uvedené GET endpointy a v každé odpovědi poslat:

```cpp
client.println("Access-Control-Allow-Origin: *");
```

HTTPS stránka může HTTP požadavek na lokální ESP zablokovat jako mixed content. Pro spolehlivé vzdálené ovládání je proto vhodnější Web Serial nebo firmware s odchozím zabezpečeným WebSocket spojením.

Používejte pouze na vlastním zařízení a v izolovaném laboratorním prostředí.
