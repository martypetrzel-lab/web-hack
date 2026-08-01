# ESP32 Marauder Web Controller

Webové rozhraní připravené pro GitHub a Railway. Se současným firmwarem komunikuje přes USB pomocí Web Serial API. Volitelně podporuje také LAN HTTP API (`/data`, `/scan`, `/stop` a další), pokud je firmware implementuje a vrací CORS hlavičku.

## Railway

1. V Railway zvolte **New Project → Deploy from GitHub repo**.
2. Vyberte `martypetrzel-lab/web-hack`.
3. Railway načte `railway.json`, spustí `npm start` a kontroluje `/health`.
4. V **Settings → Networking** vygenerujte veřejnou doménu.

Nejsou potřeba žádné proměnné prostředí.

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
