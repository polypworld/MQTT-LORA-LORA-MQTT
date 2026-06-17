LoRa ↔ MQTT Bridge ESP32 (SX1262)

ESP32 alapú LoRa–MQTT átjáró SX1262 rádióhoz, amely titkosított, megbízható kétirányú kommunikációt biztosít LoRa node-ok és MQTT rendszerek (pl. Home Assistant) között.
A rendszer kifejezetten hosszú távú, stabil üzemre készült memóriaoptimalizált architektúrával, automatikus hibakezeléssel és beépített diagnosztikával.
________________________________________
Fő funkciók
Kétirányú LoRa ↔ MQTT kommunikáció
MQTT → LoRa
MQTT üzenetek fogadása:
lora/<node_id>/to_node/#
A bridge:
•	JSON formátumba csomagolja az adatot 
•	szükség esetén automatikusan feldarabolja 
•	AES-GCM titkosítással védi 
•	LoRa hálózaton továbbítja a cél node felé 
LoRa → MQTT
A node által küldött üzenetek:
•	visszafejtésre kerülnek 
•	fragmentáció esetén automatikusan összefűződnek 
•	JSON feldolgozás után MQTT topicokra publikálódnak 
________________________________________
AES-GCM titkosítás
Minden LoRa csomag titkosított.
Jellemzők:
•	AES-128 GCM 
•	hitelesített titkosítás 
•	integritás ellenőrzés 
•	hamisított csomagok automatikus elutasítása 
•	replay támadások elleni védelem 
________________________________________
Megbízható adatátvitel
A LoRa fizikai réteg fölött saját megbízhatósági réteg működik.
ACK rendszer
Minden adatcsomag:
•	számlálóval rendelkezik 
•	ACK visszaigazolást vár 
A bridge:
•	automatikusan küld ACK csomagot 
•	figyeli a visszaigazolásokat 
•	kezeli az ismétléseket 
Automatikus újraküldés
Ha ACK nem érkezik:
•	újraküldés indul 
•	véletlenszerű késleltetéssel 
•	maximum konfigurálható próbálkozásszámmal 
________________________________________
Nagyméretű üzenetek kezelése
Automatikus fragmentáció támogatás.
Küldéskor
A nagy JSON csomagokat:
•	feldarabolja 
•	sorszámozza 
•	külön LoRa csomagokként továbbítja 
Fogadáskor
A bridge:
•	összegyűjti a fragmenteket 
•	ellenőrzi a hiányzó részeket 
•	automatikusan újraépíti az eredeti üzenetet 
________________________________________
Duplikáció védelem
A rendszer felismeri:
•	ismételten érkező csomagokat 
•	újraküldött csomagokat 
•	korábban már feldolgozott adatokat 
Ez megakadályozza:
•	duplikált MQTT publikálást 
•	hibás állapotfrissítéseket 
________________________________________
Home Assistant kompatibilis node felügyelet
A bridge automatikusan figyeli a node-ok aktivitását.
Online állapot
Amikor node adatot küld:
lora/node_X/status/on_off = online
Offline állapot
Ha a node meghatározott ideig nem jelentkezik:
lora/node_X/status/on_off = offline
A státusz retain üzenetként kerül publikálásra.
________________________________________
MQTT Availability támogatás
A bridge saját elérhetőségi állapotot is publikál.
Online
lora/bridge/status/on_off = online
Offline
MQTT Last Will segítségével:
lora/bridge/status/on_off = offline
________________________________________
WiFi funkciók
Támogatott:
•	WPA2 
•	WPA3 
Funkciók:
•	statikus IP konfiguráció 
•	automatikus újracsatlakozás 
•	RSSI monitorozás 
•	nem blokkoló kapcsolatkezelés 
________________________________________
Rendszerdiagnosztika
Opcionálisan részletes diagnosztikai adatokat publikál MQTT-n.
Mért értékek:
Memória
•	szabad heap 
•	minimális heap 
•	legnagyobb foglalható blokk 
•	heap fragmentáció 
•	heap változás 
LoRa statisztikák
•	fogadott csomagok 
•	küldött csomagok 
•	ACK csomagok 
•	újraküldések 
•	elveszett csomagok 
•	dekódolási hibák 
•	JSON hibák 
Queue monitorozás
•	várakozó csomagok száma 
•	retransmission számláló 
Rádió állapot
•	RSSI 
•	kapcsolat minőség 
Feladatok
•	stack kihasználtság 
•	watchdog állapot 
Üzemidő
•	uptime 
•	utolsó reset oka 
________________________________________
Egészségpontszám (Health Score)
A rendszer automatikusan számít egy 0–100 közötti egészségpontszámot.
Figyelembe veszi:
•	memóriaállapot 
•	fragmentáció 
•	retransmission arány 
•	queue telítettség 
•	stack tartalék 
•	kommunikációs aktivitás 
Ez lehetővé teszi a hosszú távú állapotfigyelést és a korai hibafelismerést.
________________________________________
OLED támogatás (opcionális)
Heltec OLED kijelző támogatás.
________________________________________
Memóriaoptimalizált architektúra
A rendszer hosszú távú működésre lett optimalizálva.
Jellemzők:
•	minimális heap használat 
•	fix méretű pufferek 
•	statikus fragment tárolók 
•	statikus ring buffer queue-k 
•	std::string_view 
•	StaticJsonDocument 
•	heap-fragmentáció minimalizálása 
A cél a több hónapos megszakítás nélküli üzem ESP32 platformon.
________________________________________
Biztonsági funkciók
•	AES-GCM titkosítás 
•	üzenethitelesítés 
•	replay védelem 
•	watchdog felügyelet 
•	MQTT Last Will 
•	hibás csomagok automatikus eldobása 
•	fragment timeout kezelés 
•	túlméretes csomagok szűrése 
________________________________________
Hardver
Tesztelt platform:
•	ESP32 
•	Heltec ESP32 LoRa V3 
•	SX1262 
________________________________________
Felhasználási területek
•	Home Assistant integráció 
•	szenzorhálózatok 
•	épületautomatizálás 
•	ipari telemetria 
•	távoli mérőrendszerek 
•	mezőgazdasági monitorozás 
•	LoRa alapú IoT hálózatok 
Egy titkosított, megbízható, Home Assistant-kompatibilis LoRa MQTT gateway ESP32/SX1262 platformra, amelyet kifejezetten hosszú távú stabil üzemre optimalizáltak.




LoRa MQTT kliens

Egy  LoRa ⇄ MQTT Node (végpont), amely Wi-Fi Access Pointként és helyi MQTT brokerként működik, majd az MQTT üzeneteket AES-GCM titkosítással továbbítja LoRa hálózaton keresztül egy központi híd felé. A rendszer kifejezetten alacsony memóriahasználatra, energiahatékonyságra és üzembiztosságra optimalizált.
Áttekintés
A LoRa MQTT Node egy ESP32-S3 alapú végpont, amely:
•	Saját Wi-Fi Access Pointot üzemeltet 
•	Helyi MQTT brokerként működik 
•	MQTT üzeneteket LoRa hálózatra továbbít 
•	AES-GCM titkosítást használ 
•	Automatikus ACK visszaigazolást kezel 
•	Fragmentálja a nagy méretű csomagokat 
•	Diagnosztikai és önfelügyeleti funkciókat biztosít 
•	OLED kijelzővel vagy kijelző nélkül is működik 
A rendszer támogatja:
•	Heltec LoRa V3 
•	Seeed Studio XIAO ESP32S3 + WIO SX1262 
hardvereket.
________________________________________
Fő funkciók
Helyi Wi-Fi Access Point
A node saját AP-t hoz létre:
•	rejtett SSID támogatás 
•	WPA2 védelem 
•	konfigurálható kliensszám 
•	DHCP teljes kikapcsolása memóriaoptimalizálás céljából 
Lehetővé teszi:
•	mobiltelefonok 
•	MQTT kliensek 
közvetlen csatlakozását.
________________________________________
Beépített MQTT Broker
A rendszer a PicoMQTT könyvtár segítségével:
•	MQTT Brokerként működik 
•	MQTT 3.1.1 kompatibilis 
•	felhasználónév/jelszó hitelesítést támogat 
•	kliens csatlakozásokat naplózza 
Publikált események:
online
offline
Kliens állapot topic:
lora/node_X/clients/CLIENT_ID/status

________________________________________
AES-GCM Titkosítás
Minden LoRa csomag:
•	AES-128 GCM titkosítást használ 
•	hitelesített 
•	manipuláció ellen védett 
Védelmek:
•	replay attack védelem 
•	csomagsorszám ellenőrzés 
•	hitelesített ACK 
________________________________________
MQTT → LoRa Átjáró
A helyi MQTT üzenetek automatikusan:
{
  "t":"topic",
  "v":"payload"
}
formátumra kerülnek és LoRa hálózaton továbbítódnak.
________________________________________
LoRa → MQTT Átjáró
Beérkező LoRa üzenetek:
1.	dekódolás 
2.	AES ellenőrzés 
3.	fragment összeállítás 
4.	JSON feldolgozás 
5.	MQTT publikálás 
Automatikusan megjelennek a helyi brokerben.
________________________________________
Nagy Csomagok Fragmentálása
Amennyiben az MQTT payload nagyobb mint a LoRa csomagméret:
•	automatikus feldarabolás 
•	többrészes továbbítás 
•	automatikus összefűzés 
Támogatott:
•	JSON állományok 
•	hosszabb Home Assistant üzenetek 
•	szenzorcsomagok 
________________________________________
ACK és Újraküldés
Megbízható adatátvitel:
•	minden adatcsomag ACK-ot vár 
•	időzített újraküldés 
•	elveszett csomagok kezelése 
•	duplikáció szűrés 
Statisztikák:
•	ACK RX 
•	ACK TX 
•	retransmission count 
•	duplicate packets 
________________________________________
Többszintű Queue Rendszer
A node:
•	32 külön node queue-t kezel 
•	prioritásos adási sorokat használ 
•	ACK és adatcsomagokat külön kezeli 
Előnyök:
•	nincs blokkolás 
•	nagy terhelés alatt is stabil 
________________________________________
Diagnosztika és Monitoring
Rendszerállapot Publikálás
Időszakosan továbbított értékek:
heap_free
min_heap
largest_block
heap_delta
heap_frag
lora_rssi
lora_lqi
queue_packets
uptime
health_score
Topic formátum:
lora/node_X/status/*
________________________________________
Egészségpontszám (Health Score)
A node folyamatosan számolja saját állapotát.
Figyelt tényezők:
•	szabad memória 
•	memóriafragmentáció 
•	stack tartalék 
•	retransmission arány 
•	queue telítettség 
•	kommunikációs aktivitás 
Eredmény:
0 - 100 %
________________________________________
MQTT Alapú Távoli Diagnosztika
Parancs topic:
internal/node_X/command
Támogatott parancsok:
STATUS
reboot
A STATUS parancs teljes riportot küld:
•	memóriaállapot 
•	LoRa statisztikák 
•	CPU frekvencia 
•	uptime 
•	újraindítás oka 
•	queue állapot 
________________________________________
OLED Kijelző Támogatás
Opcionális SSD1306 kijelző.
________________________________________
Watchdog Védelem
Beépített ESP32 Watchdog.
Tulajdonságok:
•	15 másodperces timeout 
•	task figyelés 
•	kernel panic detektálás 
Véd:
•	lefagyás 
•	deadlock 
•	végtelen ciklus 
ellen.
________________________________________
Energiaoptimalizálás
A rendszer kifejezetten alacsony fogyasztásra készült.
Alkalmazott technikák:
•	CPU 80 MHz mód 
•	Bluetooth teljes lekapcsolása 
•	lebegő GPIO-k lehúzása 
•	OLED kikapcsolás 
•	alacsony LoRa TX teljesítmény 
•	minimális Wi-Fi pufferek 
•	DHCP leállítása 
________________________________________
Memóriaoptimalizálás
Kiemelt cél a heap fragmentáció minimalizálása.
Megoldások:
•	fix méretű pufferek 
•	stack alapú JSON feldolgozás 
•	heap allokáció kerülése futás közben 
•	PSRAM támogatás (hardware függő)
•	optimalizált Wi-Fi buffer kezelés 
________________________________________
Biztonsági Funkciók
Kommunikáció
•	AES-128 GCM titkosítás 
•	hitelesített csomagok 
•	replay attack védelem 
MQTT
•	felhasználónév/jelszó alapú belépés 
•	kliens státusz követés 
LoRa
•	CRC ellenőrzés 
•	ACK alapú megbízhatóság 
•	duplikáció szűrés 
________________________________________
Publikált Topic Struktúra
lora/node_X/status/*
lora/node_X/clients/*
lora/node_X/debug_teszt
internal/node_X/command
________________________________________
Főbb Jellemzők
✅ Beépített MQTT Broker
✅ Wi-Fi Access Point
✅ AES-GCM titkosítás
✅ LoRa kommunikáció
✅ Fragmentáció és újraösszeállítás
✅ ACK + retransmission rendszer
✅ MQTT ↔ LoRa átjáró
✅ Távoli diagnosztika
✅ OLED támogatás
✅ Watchdog védelem
✅ Energiaoptimalizálás
✅ Heap monitorozás
✅ Health Score rendszer
✅ Heltec V3 támogatás
✅ XIAO WIO támogatás
✅ PSRAM támogatás
Ez a projekt gyakorlatilag egy önálló, biztonságos, alacsony fogyasztású LoRa MQTT végpont, amely közvetlenül képes szenzorok, Home Assistant eszközök vagy egyedi IoT kliensek (ESPhome, WLED, Shelly, Tasmota…) LoRa hálózatba integrálására.

