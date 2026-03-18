# [ZSO] Zadanie 3 - ZSONET® Network Card Driver
## Opis
### Sterownik kart sieciowej
Implementacja sterownika karty sieciowej ZSONET® znajduje się w `zsonet/` i składa się z następujących plików
| plik | opis |
| -----|------|
| `zsonet.h` | Plik nagłówkowy definiujący wszystkie rejestry urządzenia i struktury opisujące ich ułożenie w pamięci. $\pm$ na wzorzec mikrokontrolerów STM32. |
| `zsonet-hw.h` | Pomocnicze definicje podstawowych operacji na rejestrach urządzenia |
| `zsonet-hw.c` | Implementacja procedur rozruchu i wyłączenia urządzenia ZSONET® |
| `zsonet-pci-driver.c` | Implementacja sterownika urządzenia PCI dla ZSONET® |
| `zsonet-net-driver.c` | Implementacja sterownika urządzenia sieciowego dla ZSONET® |
| `zsonet-driver-common.h` | Definicje wspólnych struktur i operacji na nich |
| `utils.h` | Przydatne makra z asercjami |

Urządzenie korzysta z wspóldzielonych przerwań oraz jego działanie jest chronione blokadą wirującą blokującą dostęp do urządzenia w kluczowych momentach.

W celu zainstalowania urządzenia należy wykonać standardową sekwencje poleceń
```
cd zsonet
make
make install
modprobe zsonet
echo "This is the best Network Card I have ever used"
```

- Internet działa.
- Nawet szybko (udało mi się zobaczyć 20Mb/s)
- Wszystkie testy przechodzą pomyślnie
  -  Test ze statystykami nie przechodzi jeżeli nie obniżymy rx_missed do 0 (prawdopodobnie ze względu na duży bufor RX)
  -  Test ze statystykami przejdzie jeżeli ZSONET® jest działjącą kartą
- Nagłówek `zsonet.h` moim zdaniem jest przepiękny

##### *Sterownik ZSONET® stanowi nieodłączną część urządzenia ZSONET® Network Card, które jest własnością ZSO2024. Po więcej informacji posimy udac się pod https://students.mimuw.edu.pl/ZSO. Ewentualne pytania prosimy kierować na w.ciszewski@mimuw.edu.pl. Ja nic nie wiem :)*

### Program transmitter
Implementacja programu `transmitter` znajduje się w `transmitter/` i składa się z następujących plików
| plik | opis |
| -----|------|
| `transmitter.c` | Implementacja głównych metod i samego transmitera |
| `transmitter-utils.h` | Pomocnicze makra i procedury debugujące |

Transmitter wykorzystuje do *wszystkiego* `io_uring` zgodnie z poleceniem zadania. Główny schemat pracy transmittera to:
- Parsowanie wejścia
- Główna pętla wykonawcza:
    - Wczytanie linii wejścia z `stdin`
    - Utworzenie wszystkich brakujących socketów (polecenia + obsługa)
    - Utworzenie poleceń nawiązania brakujących połączeń
    - Utworzenie poleceń wysłania danych
        - w przypadku brakującego połaczenia powyższe dwa *requesty* są ze sobą spięte przez `IOSQE_IO_LINK`.
    - Złożenie poleceń do wykonania
    - Obsługa wykonanych poleceń i ewentualne dodatkowe transmisje
- Zamykanie socketów

Program przechodzi **prawie** wszystkie testy, dokładniej wszystkie oprócz testu `reconnect`.
#### Problem z `reconnect`
Po głębszej analizie problem leży w momencie zauważenia zerwanego łącza, tj. mimo zamknięcia przez serwer połączeń polecenie `send` wywołanie przez program transmitter kończy się **sukcesem**, dopiero następna próba wykazuje błąd `Broken pipe`. Jako, że test oczekuje przeczytania `n` linijek na `stderr` całość się zawiesza - test nie podaje nowej linijki wejścia, ani go nie zamyka, przez co transmitter grzecznie czeka i nie widzi żadnego problemu.

Nie wiem dlaczego polecenie `send` nie wykrywa zerwanego połączenia i stwierdziłem, że nie będę dorzucał obejścia tego pojedynczego problemu w postaci dodatkowych testów na socketach czy innych, które teoretycznie byłoby sprzeczne z treścią polecenia. 

Pozostawiłem debugging w transmiterze, który wypluwa tekst do pliku. W celu włączenia debugowania należy wykonać `make debug` w katalogu z programem.

Gdyby udało się naleźć co jest przyczyną tego błędu, bardzo chciałbym się dowiedzieć :)

### Opóźnienie
Rozwiązanie nadesłane 08.06.2024 - to będą 4 dnie spóźnienia.