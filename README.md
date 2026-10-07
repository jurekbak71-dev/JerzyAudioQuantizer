# JERZY AUDIO QUANTIZER 2

Studyjny VST3 do zdecydowanej korekcji rytmicznej już nagranych partii gitarowych.

## Cel wersji 2

Wtyczka jest projektowana jako pomoc dla gitarzysty, który gra nierówno względem tempa lub wybranego podziału rytmicznego. Priorytetem jest poprawa timingowa bez dubli ataku, echa, przypadkowych glitchy i słyszalnego "gumowania" dźwięku. Drugim zadaniem jest lekkie wyrównanie zbyt dużych różnic dynamiki pomiędzy uderzeniami.

To nie jest efekt live. Wersja 2 świadomie używa większego, stałego opóźnienia studyjnego, aby mieć czas na analizę materiału i nie zmieniać PDC hosta w trakcie odtwarzania.

## Najważniejsze zmiany względem 0.3.2

- nowa nazwa produktu: **JERZY AUDIO QUANTIZER 2**,
- nowy bundle ID i VST3 plug-in code, więc stara i nowa wersja mogą istnieć obok siebie,
- stała latencja raportowana hostowi; brak `setLatencySamples()` w `processBlock()`,
- znacznie mniejsze bufory audio,
- brak krótkotrwałych alokacji `std::vector` w torze audio,
- współczynniki filtrów detektora są liczone raz w `prepare()`, a nie przez `exp()` dla każdej próbki,
- ulepszony detektor ataków gitary: poziom + szybka zmiana energii + przewaga pasma ataku + adaptacyjny noise floor,
- bardziej stabilne AUTO: decyzja korzysta z historii ostatnich odstępów między pewnymi uderzeniami,
- dodatkowe rytmy: shuffle 1/8 i shuffle 1/16,
- pełny segment zawsze przechodzi przez Signalsmith Stretch, dzięki czemu historia/phase state nie jest przerywana,
- początek nuty jest chroniony przez equal-power crossfade między oryginalnym atakiem a już przetworzonym segmentem,
- automatyczne ograniczanie niebezpiecznych współczynników stretch zamiast wymuszania korekcji za wszelką cenę,
- lekkie wyrównanie dynamiki po korekcji czasu,
- przebudowane GUI z polskimi opisami mówiącymi wprost, co robi każda kontrolka.

## Synchronizacja z FL Studio / hostem

Wtyczka pobiera z hosta:
- BPM,
- pozycję PPQ na początku bloku.

Na tej podstawie każdy pewny atak jest porównywany z wybraną siatką. Jeżeli host nie poda PPQ, wtyczka utrzymuje własną ciągłość PPQ jako tryb awaryjny.

Dostępne rytmy:
- AUTO — rozpoznaj z nagrania,
- ćwierćnuty 1/4,
- ósemki 1/8,
- szesnastki 1/16,
- trzydziestodwójki 1/32,
- triole ósemkowe 1/8T,
- triole szesnastkowe 1/16T,
- shuffle ósemkowy,
- shuffle szesnastkowy.

## Jak działa korekcja

1. Detektor szuka prawdziwego początku uderzenia kostki i nadaje mu confidence.
2. Quantizer wybiera najbliższy sensowny punkt rytmiczny.
3. Dwa kolejne zaakceptowane ataki tworzą odcinek podobny do pracy markerów warpu.
4. Odcinek jest rozciągany/skracany przez Signalsmith Stretch z zachowaniem wysokości.
5. Cały odcinek przechodzi przez stretcher; jego stanu nie przerywa już kopiowany "na skróty" fragment.
6. Początek nuty jest na wyjściu zastępowany naturalnym atakiem i płynnie crossfadowany do przetworzonego sustainu.
7. Zbyt agresywna korekcja jest automatycznie ograniczana do bezpieczniejszego ratio.
8. Opcjonalny leveler łagodnie uspokaja zbyt mocne uderzenia.

## Parametry GUI

### JAK ŁATWO ROZPOZNAJE UDERZENIE KOSTKI
Więcej = wykrywa delikatniejsze ataki. Zmniejsz, jeśli jako ataki traktowane są szuranie palców lub sustain.

### PONIŻEJ JAKIEGO POZIOMU IGNORUJE DŹWIĘK
Pomaga odrzucać szum, brum i ciche przecieki.

### DO JAKIEGO RYTMU MA WYRÓWNYWAĆ
AUTO analizuje historię pewnych uderzeń albo można narzucić konkretny podział.

### JAK MOCNO DOCIĄGA GRĘ DO RYTMU
Określa procent przesunięcia wykrytego uderzenia w stronę siatki.

### JAK DUŻY BŁĄD RYTMU JESZCZE NAPRAWIA
Ogranicza maksymalną odległość od siatki, którą wtyczka ma jeszcze uznać za błąd do naprawy.

### ILE NAGRANIA SPRAWDZA PRZED DECYZJĄ
Steruje zakresem materiału branym pod uwagę przed wypuszczeniem bezpiecznego audio. Latencja raportowana hostowi pozostaje stała.

### JAK MOCNO CHRONI POCZĄTEK NUTY
Określa długość oryginalnego ataku zachowanego przed płynnym wejściem w time-stretch.

### ILE SWINGU DODAJE DO PROSTEJ SIATKI
Przesuwa co drugi punkt prostej siatki. Tryby Shuffle mają własny stały układ.

### JAK MOCNO WYRÓWNUJE GŁOŚNOŚĆ UDERZEŃ
Włącza łagodne wyrównanie zbyt mocnych uderzeń po korekcji czasu.

## Wskaźniki

- **CZY TO PRAWDZIWY ATAK** — confidence detektora,
- **OSTATNIA POPRAWKA** — przesunięcie w ms,
- **RYZYKO ARTEFAKTÓW** — wynikające głównie z aktualnego stretch ratio,
- **WYRÓWNANIE DYNAMIKI** — bieżąca redukcja poziomu i BPM hosta,
- liczniki wykrytych / użytych / odrzuconych ataków.

## Zalecany punkt startowy dla nierównej gitary rytmicznej

- rytm: AUTO albo wymuszony 1/16,
- rozpoznawanie ataku: około 0.60–0.70,
- ignorowanie szumu: około -48 dB,
- siła poprawy: 80–92%,
- maksymalny błąd: 60–100 ms,
- analiza: 700–1000 ms,
- ochrona początku nuty: 15–25 ms,
- swing: 0% jeśli nie jest celowo potrzebny,
- wyrównanie dynamiki: 15–35%.

## Optymalizacja

Największe zmiany CPU/RAM w wersji 2:
- usunięcie funkcji wykładniczych z pętli per-sample detektora,
- brak heap allocation w `StudioSegmentEngine::processSegment()`,
- bufory dopasowane do realnego maksymalnego okna studyjnego zamiast 8/12/4/6 sekund,
- 20 Hz odświeżanie GUI zamiast 30 Hz,
- brak ciągłego rekonfigurowania latencji hosta,
- przetwarzanie stereo maksymalnie dla dwóch kanałów.

Jakość stretchera pozostaje w trybie `presetDefault`; optymalizacja nie przełącza go na niższą jakość.

## Build Windows

```powershell
.\build_windows.ps1
```

Oczekiwany bundle:

```text
build\JerzyAudioQuantizer2_artefacts\Release\VST3\JERZY AUDIO QUANTIZER 2.vst3
```

## Zależności

- JUCE 9.0.3
- Signalsmith Stretch — przypięty do commitu `a670068d9aeb64913331d5cc29337b19a457a7df`


<!-- Jerzy VST GUI System CI validation -->
