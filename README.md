A project célja, hogy a meglévő HomeAssistant és MQTT eszközök közötti távolságot kitolja anélkül, hogy WiFi repeaterek használatára kényszerülnénk.
Ezzel a rendszerrel a hatótáv akár több km is lehet.
Szükség van egy bridgere a HomeAssistant közelében: ő rakja át a LoRa rádión érkező adatokat WiFi hálózatra.
A másik végén a rendszernek akár 32 LoRa kliens is lehet, kliensenként 4-8 (hw függő) szabványos WiFi MQTT eszköz csatlakozási lehetőséggel (kliensenként).

# LoRa ↔ MQTT Bridge ESP32 (SX1262)

ESP32 alapú LoRa–MQTT átjáró SX1262 rádióhoz, amely titkosított, megbízható kétirányú kommunikációt biztosít LoRa node-ok és MQTT rendszerek (pl. Home Assistant) között.
A rendszer kifejezetten hosszú távú, stabil üzemre készült memóriaoptimalizált architektúrával, automatikus hibakezeléssel és beépített diagnosztikával.
________________________________________
## Fő funkciók<br/>
Kétirányú LoRa ↔ MQTT kommunikáció<br/>
MQTT → LoRa<br/>
MQTT üzenetek fogadása: lora/<node_id>/to_node/#<br/><br/>
A bridge:<br/>
•	JSON formátumba csomagolja az adatot <br/>
•	szükség esetén automatikusan feldarabolja <br/>
•	AES-GCM titkosítással védi <br/>
•	LoRa hálózaton továbbítja a cél node felé <br/><br/>
LoRa → MQTT<br/>
A node által küldött üzenetek:<br/>
•	visszafejtésre kerülnek <br/>
•	fragmentáció esetén automatikusan összefűződnek <br/>
•	JSON feldolgozás után MQTT topicokra publikálódnak <br/>
________________________________________
## AES-GCM titkosítás<br/>
Minden LoRa csomag titkosított.<br/><br/>
Jellemzők:<br/>
•	AES-128 GCM <br/>
•	hitelesített titkosítás <br/>
•	integritás ellenőrzés <br/>
•	hamisított csomagok automatikus elutasítása <br/>
•	replay támadások elleni védelem <br/>
________________________________________
## Megbízható adatátvitel<br/>
A LoRa fizikai réteg fölött saját megbízhatósági réteg működik.<br/><br/>
ACK rendszer<br/>
Minden adatcsomag:<br/>
•	számlálóval rendelkezik <br/>
•	ACK visszaigazolást vár <br/><br/>
A bridge:<br/>
•	automatikusan küld ACK csomagot <br/>
•	figyeli a visszaigazolásokat <br/>
•	kezeli az ismétléseket <br/><br/>
Automatikus újraküldés<br/>
Ha ACK nem érkezik:<br/>
•	újraküldés indul <br/>
•	véletlenszerű késleltetéssel <br/>
•	maximum konfigurálható próbálkozásszámmal <br/>
________________________________________
## Nagyméretű üzenetek kezelése<br/>
Automatikus fragmentáció támogatás.<br/><br/>
Küldéskor<br/>
A nagy JSON csomagokat:<br/>
•	feldarabolja <br/>
•	sorszámozza <br/>
•	külön LoRa csomagokként továbbítja <br/><br/>
Fogadáskor<br/>
A bridge:<br/>
•	összegyűjti a fragmenteket <br/>
•	ellenőrzi a hiányzó részeket <br/>
•	automatikusan újraépíti az eredeti üzenetet <br/>
________________________________________
## Duplikáció védelem
A rendszer felismeri:<br/>
•	ismételten érkező csomagokat <br/>
•	újraküldött csomagokat <br/>
•	korábban már feldolgozott adatokat <br/>
Ez megakadályozza:<br/>
•	duplikált MQTT publikálást <br/>
•	hibás állapotfrissítéseket <br/>
________________________________________
## Home Assistant kompatibilis node felügyelet<br/>
A bridge automatikusan figyeli a node-ok aktivitását.<br/><br/>
Online állapot<br/>
Amikor node adatot küld:<br/>
lora/node_X/status/on_off = online<br/><br/>
Offline állapot<br/>
Ha a node meghatározott ideig nem jelentkezik:<br/>
lora/node_X/status/on_off = offline<br/>
A státusz retain üzenetként kerül publikálásra.<br/>
________________________________________
## MQTT Availability támogatás<br/>
A bridge saját elérhetőségi állapotot is publikál.<br/><br/>
Online<br/>
lora/bridge/status/on_off = online<br/><br/>
Offline<br/>
MQTT Last Will segítségével:<br/>
lora/bridge/status/on_off = offline<br/>
________________________________________
## WiFi funkciók<br/>
Támogatott:<br/>
•	WPA2 <br/>
•	WPA3 <br/><br/>
Funkciók:<br/>
•	statikus IP konfiguráció <br/>
•	automatikus újracsatlakozás <br/>
•	RSSI monitorozás <br/>
•	nem blokkoló kapcsolatkezelés <br/>
________________________________________
## Rendszerdiagnosztika<br/>
Opcionálisan részletes diagnosztikai adatokat publikál MQTT-n.<br/>
Mért értékek:<br/>
Memória<br/><br/>
•	szabad heap <br/>
•	minimális heap <br/>
•	legnagyobb foglalható blokk <br/>
•	heap fragmentáció <br/>
•	heap változás <br/>
LoRa statisztikák<br/><br/>
•	fogadott csomagok <br/>
•	küldött csomagok <br/>
•	ACK csomagok <br/>
•	újraküldések <br/>
•	elveszett csomagok <br/>
•	dekódolási hibák <br/>
•	JSON hibák <br/><br/>
Queue monitorozás<br/>
•	várakozó csomagok száma <br/>
•	retransmission számláló <br/><br/>
Rádió állapot<br/>
•	RSSI <br/>
•	kapcsolat minőség <br/><br/>
Feladatok<br/>
•	stack kihasználtság <br/>
•	watchdog állapot <br/><br/>
Üzemidő<br/>
•	uptime <br/>
•	utolsó reset oka <br/>
________________________________________
## Egészségpontszám (Health Score)<br/>
A rendszer automatikusan számít egy 0–100 közötti egészségpontszámot.<br/>
Figyelembe veszi:<br/>
•	memóriaállapot <br/>
•	fragmentáció <br/>
•	retransmission arány <br/>
•	queue telítettség <br/>
•	stack tartalék <br/>
•	kommunikációs aktivitás <br/>
Ez lehetővé teszi a hosszú távú állapotfigyelést és a korai hibafelismerést.<br/>
________________________________________
## OLED támogatás (opcionális)<br/>
Heltec OLED kijelző támogatás.<br/>
________________________________________
## Memóriaoptimalizált architektúra<br/>
A rendszer hosszú távú működésre lett optimalizálva.<br/>
Jellemzők:<br/>
•	minimális heap használat <br/>
•	fix méretű pufferek <br/>
•	statikus fragment tárolók <br/>
•	statikus ring buffer queue-k <br/>
•	std::string_view <br/>
•	StaticJsonDocument <br/>
•	heap-fragmentáció minimalizálása <br/>
A cél a több hónapos megszakítás nélküli üzem ESP32 platformon.<br/>
________________________________________
## Biztonsági funkciók<br/>
•	AES-GCM titkosítás <br/>
•	üzenethitelesítés <br/>
•	replay védelem <br/>
•	watchdog felügyelet <br/>
•	MQTT Last Will <br/>
•	hibás csomagok automatikus eldobása <br/>
•	fragment timeout kezelés <br/>
•	túlméretes csomagok szűrése <br/>
________________________________________
## Hardver<br/>
•	ESP32 <br/>
•	Heltec ESP32 LoRa V3 <br/>
•	SX1262 <br/>
________________________________________
## Felhasználási területek<br/>
•	Home Assistant integráció <br/>
•	szenzorhálózatok <br/>
•	épületautomatizálás <br/>
•	ipari telemetria <br/>
•	távoli mérőrendszerek <br/>
•	mezőgazdasági monitorozás <br/>
•	LoRa alapú IoT hálózatok <br/><br/>
Egy titkosított, megbízható, Home Assistant-kompatibilis LoRa MQTT gateway ESP32/SX1262 platformra, amelyet kifejezetten hosszú távú stabil üzemre optimalizáltak.<br/><br/><br/>




# LoRa MQTT kliens<br/>

Egy  LoRa ⇄ MQTT Node (végpont), amely Wi-Fi Access Pointként és helyi MQTT brokerként működik, majd az MQTT üzeneteket AES-GCM titkosítással továbbítja LoRa hálózaton keresztül egy központi híd felé. A rendszer kifejezetten alacsony memóriahasználatra, energiahatékonyságra és üzembiztosságra optimalizált.<br/><br/>
Áttekintés<br/>
A LoRa MQTT Node egy ESP32-S3 alapú végpont, amely:<br/>
•	Saját Wi-Fi Access Pointot üzemeltet <br/>
•	Helyi MQTT brokerként működik <br/>
•	MQTT üzeneteket LoRa hálózatra továbbít <br/>
•	AES-GCM titkosítást használ <br/>
•	Automatikus ACK visszaigazolást kezel <br/>
•	Fragmentálja a nagy méretű csomagokat <br/>
•	Diagnosztikai és önfelügyeleti funkciókat biztosít <br/>
•	OLED kijelzővel vagy kijelző nélkül is működik <br/>
A rendszer támogatja:<br/>
•	Heltec LoRa V3 <br/>
•	Seeed Studio XIAO ESP32S3 + WIO SX1262 <br/>
hardvereket.<br/>
________________________________________
## Fő funkciók<br/>
Helyi Wi-Fi Access Point<br/>
A node saját AP-t hoz létre:<br/>
•	rejtett SSID támogatás <br/>
•	WPA2 védelem <br/>
•	konfigurálható kliensszám <br/>
•	DHCP teljes kikapcsolása memóriaoptimalizálás céljából <br/>
Lehetővé teszi:<br/>
•	mobiltelefonok <br/>
•	MQTT kliensek <br/>
közvetlen csatlakozását.<br/>
________________________________________
## Beépített MQTT Broker<br/>
A rendszer a PicoMQTT könyvtár segítségével:<br/>
•	MQTT Brokerként működik <br/>
•	MQTT 3.1.1 kompatibilis <br/>
•	felhasználónév/jelszó hitelesítést támogat <br/>
•	kliens csatlakozásokat naplózza <br/>
Publikált események:<br/>
online<br/>
offline<br/>
Kliens állapot topic:<br/>
lora/node_X/clients/CLIENT_ID/status<br/>

________________________________________
## AES-GCM Titkosítás<br/>
Minden LoRa csomag:<br/>
•	AES-128 GCM titkosítást használ <br/>
•	hitelesített <br/>
•	manipuláció ellen védett <br/>
Védelmek:<br/>
•	replay attack védelem <br/>
•	csomagsorszám ellenőrzés <br/>
•	hitelesített ACK <br/>
________________________________________
## MQTT → LoRa Átjáró<br/>
A helyi MQTT üzenetek automatikusan:<br/>
{<br/>
  "t":"topic",<br/>
  "v":"payload"<br/>
}<br/>
formátumra kerülnek és LoRa hálózaton továbbítódnak.<br/>
________________________________________
## LoRa → MQTT Átjáró<br/>
Beérkező LoRa üzenetek:<br/>
1.	dekódolás <br/>
2.	AES ellenőrzés <br/>
3.	fragment összeállítás <br/>
4.	JSON feldolgozás <br/>
5.	MQTT publikálás <br/>
Automatikusan megjelennek a helyi brokerben.<br/>
________________________________________
## Nagy Csomagok Fragmentálása<br/>
Amennyiben az MQTT payload nagyobb mint a LoRa csomagméret:<br/>
•	automatikus feldarabolás <br/>
•	többrészes továbbítás <br/>
•	automatikus összefűzés <br/>
Támogatott:<br/>
•	JSON állományok <br/>
•	hosszabb Home Assistant üzenetek <br/>
•	szenzorcsomagok <br/>
________________________________________
## ACK és Újraküldés<br/>
Megbízható adatátvitel:<br/>
•	minden adatcsomag ACK-ot vár <br/>
•	időzített újraküldés <br/>
•	elveszett csomagok kezelése <br/>
•	duplikáció szűrés <br/>
Statisztikák:<br/>
•	ACK RX <br/>
•	ACK TX <br/>
•	retransmission count <br/>
•	duplicate packets <br/>
________________________________________
## Többszintű Queue Rendszer<br/>
A node:<br/>
•	32 külön node queue-t kezel <br/>
•	prioritásos adási sorokat használ <br/>
•	ACK és adatcsomagokat külön kezeli <br/>
Előnyök:<br/>
•	nincs blokkolás <br/>
•	nagy terhelés alatt is stabil <br/>
________________________________________
## Diagnosztika és Monitoring<br/>
Rendszerállapot Publikálás<br/>
Időszakosan továbbított értékek:<br/>
heap_free<br/>
min_heap<br/>
largest_block<br/>
heap_delta<br/>
heap_frag<br/>
lora_rssi<br/>
lora_lqi<br/>
queue_packets<br/>
uptime<br/>
health_score<br/>
Topic formátum:<br/>
lora/node_X/status/*<br/>
________________________________________
## Egészségpontszám (Health Score)<br/>
A node folyamatosan számolja saját állapotát.<br/>
Figyelt tényezők:<br/>
•	szabad memória <br/>
•	memóriafragmentáció <br/>
•	stack tartalék <br/>
•	retransmission arány <br/>
•	queue telítettség <br/>
•	kommunikációs aktivitás <br/>
Eredmény: 0 - 100 %<br/>
________________________________________
## MQTT Alapú Távoli Diagnosztika<br/>
Parancs topic:<br/>
internal/node_X/command<br/>
Támogatott parancsok:<br/>
STATUS<br/>
reboot<br/>
A STATUS parancs teljes riportot küld:<br/>
•	memóriaállapot <br/>
•	LoRa statisztikák <br/>
•	CPU frekvencia <br/>
•	uptime <br/>
•	újraindítás oka <br/>
•	queue állapot <br/>
________________________________________
## OLED Kijelző Támogatás<br/>
Opcionális SSD1306 kijelző.<br/>
________________________________________
## Watchdog Védelem<br/>
Beépített ESP32 Watchdog.<br/>
Tulajdonságok:<br/>
•	15 másodperces timeout <br/>
•	task figyelés <br/>
•	kernel panic detektálás <br/>
Véd:<br/>
•	lefagyás <br/>
•	deadlock <br/>
•	végtelen ciklus <br/>
ellen.<br/>
________________________________________
## Energiaoptimalizálás<br/>
A rendszer kifejezetten alacsony fogyasztásra készült.<br/>
Alkalmazott technikák:<br/>
•	CPU 80 MHz mód <br/>
•	Bluetooth teljes lekapcsolása <br/>
•	lebegő GPIO-k lehúzása <br/>
•	OLED kikapcsolás <br/>
•	alacsony LoRa TX teljesítmény <br/>
•	minimális Wi-Fi pufferek <br/>
•	DHCP leállítása <br/>
________________________________________
##Memóriaoptimalizálás<br/>
Kiemelt cél a heap fragmentáció minimalizálása.<br/>
Megoldások:<br/>
•	fix méretű pufferek <br/>
•	stack alapú JSON feldolgozás <br/>
•	heap allokáció kerülése futás közben <br/>
•	PSRAM támogatás (hardware függő)<br/>
•	optimalizált Wi-Fi buffer kezelés <br/>
________________________________________
## Biztonsági Funkciók<br/><br/>
Kommunikáció<br/>
•	AES-128 GCM titkosítás <br/>
•	hitelesített csomagok <br/>
•	replay attack védelem <br/><br/>
MQTT<br/>
•	felhasználónév/jelszó alapú belépés <br/>
•	kliens státusz követés <br/><br/>
LoRa<br/>
•	CRC ellenőrzés <br/>
•	ACK alapú megbízhatóság <br/>
•	duplikáció szűrés <br/>
________________________________________
## Publikált Topic Struktúra
lora/node_X/status/*<br/>
lora/node_X/clients/*<br/>
lora/node_X/debug_teszt<br/>
internal/node_X/command<br/>
________________________________________
## Főbb Jellemzők<br/>
✅ Beépített MQTT Broker<br/>
✅ Wi-Fi Access Point<br/>
✅ AES-GCM titkosítás<br/>
✅ LoRa kommunikáció<br/>
✅ Fragmentáció és újraösszeállítás<br/>
✅ ACK + retransmission rendszer<br/>
✅ MQTT ↔ LoRa átjáró<br/>
✅ Távoli diagnosztika<br/>
✅ OLED támogatás<br/>
✅ Watchdog védelem<br/>
✅ Energiaoptimalizálás<br/>
✅ Heap monitorozás<br/>
✅ Health Score rendszer<br/>
✅ Heltec V3 támogatás<br/>
✅ XIAO WIO támogatás<br/>
✅ PSRAM támogatás<br/><br/>
Ez a projekt gyakorlatilag egy önálló, biztonságos, alacsony fogyasztású LoRa MQTT végpont, amely közvetlenül képes szenzorok, Home Assistant eszközök vagy egyedi IoT kliensek (ESPhome, WLED, Shelly, Tasmota…) LoRa hálózatba integrálására.<br/>

