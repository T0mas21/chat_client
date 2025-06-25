# Klient pro chatovací server
## Shrnutí
TCP a UDP klient pro chatovací server využívající IPK24-chat protokol. 
## Implementace
### Vývoj
- Program byl vytvářen a testován v prostředí linux na operačním systému Ubuntu 64-bit.<br>
![Virtual PC](images/virtual_pc.png) <br>
- Byl použit jazyk c s překladačem gcc viz. make file. <br>
### Spuštění
#### Argumenty
- `-t <tcp/udp>`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: tcp nebo udp klient(povinný). <br>
- `-s <server_address>`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: adresa serveru(povinný). <br> 
- `[-p <server_port>]`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: číslo portu(volitelný - implicitně 4567). <br>
- `[-d <timeout>]`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: pro udp, doba čekání na confirm zprávu ze serveru(volitelný - implicitně 250ms). <br>
- `[-r <retransmissions>]`&nbsp;&nbsp;&nbsp;: pro udp, počet opětovných zaslání zprávy, na kterou se očekává zpráva confirm(volitelný - implicitně 3). <br>
- `[-h]`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;: vypíše nápovědu v podobě přehledu argumentů(volitelný). <br>
#### Spuštění
- Kompilace proběhne příkazem: `make` <br>
- Program lze spustit pomocí přiloženého make souboru pomocí příkazu: `make run ARGS="<arguments>"`<br>
- Jinak lze spustit pomocí `./client <arguments>`
##### Příklady spuštění
- `make run ARGS="-h"`<br>
- `make run ARGS="-t tcp -s localhost"`<br>
- `make run ARGS="-t udp -s localhost -p 4567 -d 250 -r 3"`<br>
- `./client -t udp -s localhost` <br>

### UDP
#### Moduly
##### `udp.c`
- Řídí celou udp komunikaci a volá funkce z modulu `udp_functions.c`.<br>
##### `udp.h`
- Definuje rozhraní pro `udp.c`.<br>
##### `udp_functions.c`
- Implementuje funkce pro načítání a vytváření zprávy, dekódování zprávy a výpis nápovědy k podporovaným příkazům. <br> 
##### `udp_functions.h`
- Definuje rozhraní pro `udp_functions.c`.<br>
#### Funkčnost
##### Příklad úspěšné udp komunikace klienta a serveru
![UDP communication ok](images/udp_ok.png) <br>
- Klient se nejdříve pokusí o autentizaci - neúspěšně, proto se musí autentizovat znovu, bez úspěšné autentizace program klienta neumožní zasílání jiných zpráv. Klient ukončuje komunikaci příkazem BYE.<br>
##### Příklad nezaslání CONFIRM zprávy ze serveru
![UDP communication not confirmed](images/udp_no_confirm.png) <br>
- Klient se zašle zprávu a očekává CONFIRM zprávu od serveru, které ale nepříchází. Proto každou periodu, danou argumentem `timeout`, opětovně zašle zprávu. Toto opakuje počtem daným argumentem `retransmissions`(zde se `retransmissions` = 2). Při neúspěchu pošle zprávu BYE a ukončí komunikaci. <br>
##### Příklad chyby na straně klienta a ukončení komunikace
![UDP communication wrong input](images/udp_error.png) <br>
- Klient na vstupu obdrží nepodporované znaky, tudíž dojde k chybě a klient pošle ERROR zprávu serveru a ukončí komunikaci pomocí zprávy BYE. <br>
### TCP
#### Moduly
##### `tcp.c`
- Řídí celou tcp komunikaci a volá funkce z modulu `tcp_functions.c`.<br>
##### `tcp.h`
- Definuje rozhraní pro `tcp.c`.<br>
##### `tcp_functions.c`
- Implementuje funkce pro načítání a vytváření zprávy, dekódování zprávy a výpis nápovědy k podporovaným příkazům. <br> 
##### `tcp_functions.h`
- Definuje rozhraní pro `tcp_functions.c`.<br>
#### Funkčnost
- Příklady komunikace jsou stejné jako u UDP až na zprávu CONFIRM, která v TCP není. 
## Testování
Testováno na stejné platformě jako pro vývoj a na referenčním serveru.
### UDP
#### Jednoduchá komunikace se serverem
![UDP communication test 01](images/udp_test01.png) <br>
Klient posílá autentizaci, požadavek join a 3 zprávy a server odpovídá zprávami CONFIRM a REPLY a k tomu posílá jednoduché zprávy.<br>
Soubor obsahující zprávy pro server:<br>
/auth x user x<br>
/join x<br>
hello<br>
how are you?<br>
bye<br>
#### Špatný vstup
![UDP incorrect input](images/invalid_input.png) <br>
Vstup pro klienta obsahuje nepodporované znaky, zde konkrétně u zprávy. Program vypíše chybovou hlášku na standartní chybový výstup.<br>
Soubor obsahující zprávy pro server:<br>
/auth x user x<br>
/join x<br>
hello<br>
my password is ©€日<br>
bye<br>
#### Chybějící CONFIRM
![UDP not confirmed](images/not_confirmed.png) <br>
Klient při odeslané zprávě čeká na zprávu CONFIRM od serveru. Při neobržení po stanovené době zprávu znovu odešle a to tolikrát, kolikrát je stanoveno argumenty programu. Zde se klient zprávy CONFIRM nedočká a proto ukončuje komunikaci.<br>
#### Vynechání CONFIRM
![UDP not confirmed](images/udp_missed_confirm.png) <br>
Zde je server nastaven, aby posílal každý druhý confirm, proto klient musí zasílat zprávy vícekrát.<br>
#### Nepodporovaná zpráva ze serveru
![UDP not confirmed](images/incorrect_msg_udp.png) <br>
Klient obdrží nepodporovaný typ zprávy ze serveru, proto zašle serveru zprávy ERROR a BYE a ukončuje komunikaci. <br>  
### TCP
#### Jednoduchá komunikace se serverem
![TCP communication test 01](images/tcp_test01.png) <br>
Klient posílá autentizaci, požadavek join a 3 zprávy a server odpovídá zprávami REPLY a k tomu posílá jednoduché zprávy.<br>
Soubor je stejný jako u UDP.<br>
#### Špatný vstup
![TCP communication test 01](images/tcp_invalid_input.png) <br>
Vstup pro klienta obsahuje nepodporované znaky, zde konkrétně u zprávy. Program vypíše chybovou hlášku na standartní chybový výstup.<br>
#### Nepodporovaná zpráva ze serveru
![TCP communication test 01](images/tcp_invalid_from_server.png) <br>
Klient obdrží nepodporovaný typ zprávy ze serveru, proto zašle serveru zprávy ERROR a BYE a ukončuje komunikaci. <br>
## Omezení
- UDP při chybovém stavu posílá zprávu ERROR a po ní zprávu BYE, ale nečeká na zprávu CONFIRM. <br>