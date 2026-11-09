# RISACCA (Collateral Effects)

Plugin VST3 / AU / Standalone che emula, a livello di circuito, il pedale **4ms Noise Swash**
nella versione grande a 12 manopole ("Standard Swash" / "Max Tweaker"), con in più gli ingressi
CV del **Noise Swash Eurorack** e le varianti di schema come mod nascoste.

"Risacca" è il movimento dell'acqua che torna indietro dopo l'onda, cioè lo *swash*.

## Da dove viene il circuito

| Fonte | Uso |
|---|---|
| Schema 4ms "NOISE SWASH eurorack" (Dann Green, rev 1.0, 2009) | riferimento per il nucleo e per i CV (Mood, Swash CV con vactrol) |
| Manuale del Noise Swash Eurorack | funzioni dei comandi |
| Layout veroboard "4ms Standard Swash" (tagboardeffects.blogspot.com, basato sullo Swash 2.2) | netlist completa del pedale a 12 manopole: LFO, Clean, Bass/Treble, Noise vol, Tame/Trouble |
| Layout stripboard del Noise Swash base (storyboardist) | verifica incrociata del nucleo |

La netlist ricostruita (61 tagli, 32 ponticelli, 58 componenti, 4 IC) è in `docs/standard_swash.cir`.

## Come funziona il motore

`engine/` è C++17 puro (nessuna dipendenza da JUCE):

- **Circuit**: risolutore nodale con Newton-Raphson, ricerca lineare sul residuo, suddivisione
  adattiva del passo, matrice compressa con eliminazione precalcolata dei nodi passivi e blocco
  denso con pivoting per le uscite degli op-amp.
- **Op-amp** (TL062, TL072, 4558): polo dominante con slew rate, saturazione verso le
  alimentazioni, limite di corrente, corrente di alimentazione che dipende dal carico (in classe AB),
  diodi di protezione dell'uscita, perdita di guadagno dello stadio JFET vicino al V-, offset e
  rumore. La corrente assorbita fa "cedere" la linea **Low** (1 kΩ senza condensatore): è il cuore
  del caos dello Swash.
- **SwashEngine**: la netlist in tre sotto-circuiti (core, LFO, clean) e le aggiunte Eurorack.
- Frequenza interna circa 192 kHz (Standard) o 384 kHz (Alta).

Approssimazioni dichiarate:
- 4,7 nF parassiti sulla linea Low;
- le polarizzazioni prese dalla linea Low la leggono con un campione di ritardo (~5 µs);
- LFO e clean sono sotto-circuiti accoppiati con un campione di ritardo.

## Compilazione

Su GitHub: il workflow `.github/workflows/build-macos.yml` compila su un Mac Apple Silicon un
binario universale, lo firma ad-hoc, lo verifica con pluginval e auval e pubblica lo zip negli
artefatti dell'azione.

In locale:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release          # JUCE viene scaricato (8.0.9)
cmake --build build --config Release --target Risacca_VST3 Risacca_AU Risacca_Standalone
```

Con una copia locale di JUCE: `-DJUCE_DIR=/percorso/JUCE`.

Test offline del motore (WAV + statistiche, senza JUCE):

```bash
cmake --build build --target risacca_render
./build/risacca_render idle 2 4 low=0.6 gate=0.3 swash=0.4 so=1
```

## Uso con la KeyLab

Al primo avvio la mappa CC è quella di una KeyLab mkII in modalità User:
- **encoder**: Noisegate, Swash, Pregain, Postgain, Low, Self-osc, Noise vol, LFO speed, Volume;
- **fader**: Bass, Treble, Clean, Mood CV, Swash CV.

Ogni controllo (anche interruttori e pedale) si assegna con **tasto destro → MIDI Learn**.
In Ableton il MIDI della KeyLab deve arrivare da una traccia MIDI con *MIDI To* sulla traccia di RISACCA (vedi il manuale, capitolo 7).

Il manuale d'uso per musicisti è in `docs/RISACCA-Manuale-d-uso.pdf`; le note tecniche sul circuito e sul motore sono in `docs/MANUALE.md`.
