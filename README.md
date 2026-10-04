# JERZY AUDIO QUANTIZER v0.3.2

Studyjny VST3 do zdecydowanej korekcji rytmicznej już nagranych partii gitarowych.

## Założenie

Wtyczka nie jest projektowana jako efekt live. Priorytetem jest możliwie czysta korekcja timingowa nagranej gitary bez echa, dubli ataku i glitchy.

Przetwarzanie odbywa się w trzech etapach:

1. inteligentna detekcja prawdziwych ataków,
2. ocena pewności i wybór punktów rytmicznych,
3. korekcja całych odcinków transient → transient przez pitch-preserving time-stretch.

## Aktualny DSP

- adaptacyjny detektor transjentów,
- filtr rumble/DC przed analizą,
- analiza kilku pasm częstotliwości,
- adaptacyjny noise floor,
- confidence score dla każdego ataku,
- odrzucanie słabych i podejrzanych transjentów,
- synchronizacja BPM i PPQ z hostem,
- siatki:
  - AUTO,
  - 1/4,
  - 1/8,
  - 1/16,
  - 1/32,
  - 1/8T,
  - 1/16T,
- automatyczny wybór siatki z histerezą,
- segmentowy time-stretch oparty o Signalsmith Stretch,
- ochrona początku dźwięku,
- ograniczenie ekstremalnych współczynników stretch,
- raportowanie latencji do hosta.

## Parametry

### WŁĄCZ KOREKCJĘ RYTMU
Włącza lub wyłącza przetwarzanie.

### SIATKA RYTMU
AUTO analizuje odstępy między pewnymi atakami i dobiera najbardziej prawdopodobny podział rytmiczny.

### CZUŁOŚĆ ATAKU KOSTKI
Określa, jak łatwo detektor uzna zmianę sygnału za prawdziwy atak gitarowy.

### PRÓG SZUMU I PRZECIEKÓW
Pomaga ignorować szum, przesuwanie palców, przydźwięk i ciche artefakty pomiędzy nutami.

### SIŁA KOREKCJI RYTMU
Określa, jak daleko wykryty atak zostanie przesunięty w stronę siatki.

### MAKSYMALNY BŁĄD CZASU
Określa, jak duże rozjechanie rytmiczne wtyczka może jeszcze uznać za nutę przeznaczoną do naprawy.

### DŁUGOŚĆ ANALIZY AUDIO
Określa, ile materiału wtyczka analizuje z wyprzedzeniem. Większa wartość oznacza większą latencję, ale stabilniejszą decyzję studyjną.

### OCHRONA ATAKU DŹWIĘKU
Początkowy fragment każdej zaakceptowanej nuty jest kopiowany 1:1 bez time-stretchu. Korekcja długości jest przenoszona na dalszą część segmentu, dzięki czemu atak kostki pozostaje wyraźny.

### SWING RYTMU
Przesuwa co drugi krok siatki w stronę shuffle/swing.

## Wskaźniki

- PEWNOŚĆ: PRAWDZIWY ATAK
- OSTATNIA KOREKTA
- RYZYKO ARTEFAKTÓW
- aktualny stretch ratio
- licznik wykrytych / zaakceptowanych / odrzuconych ataków

## Zalecane ustawienia startowe

Mocno nierówna gitara rytmiczna:

- Siatka: AUTO
- Czułość ataku: 0.60–0.70
- Próg szumu: około -48 dB
- Siła korekcji: 85–95%
- Maksymalny błąd czasu: 70–100 ms
- Długość analizy: 700–1000 ms
- Ochrona ataku: 15–25 ms
- Swing: 0%

## Build Windows

Wymagania:
- Visual Studio 2022 z Desktop development with C++
- CMake 3.22+
- Git

PowerShell:

```powershell
.\build_windows.ps1
```

lub:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Gotowy bundle:

```text
build\JerzyAudioQuantizer_artefacts\Release\VST3\JERZY AUDIO QUANTIZER.vst3
```

Instalacja:

```text
C:\Program Files\Common Files\VST3\
```

## Zależności

- JUCE 9.0.3
- Signalsmith Stretch — przypięty do konkretnego commitu upstream dla powtarzalnych buildów

## Aktualne ograniczenia

To nadal wersja rozwojowa. Bardzo duże korekcje oraz materiał o słabo zdefiniowanych atakach mogą wymagać zmniejszenia siły korekcji lub maksymalnego błędu czasu. Wskaźnik ryzyka artefaktów służy właśnie do szybkiego rozpoznawania takich sytuacji.
