# RISACCA – manuale

*Collateral Effects – emulazione a livello di circuito del 4ms Noise Swash a 12 manopole,
con gli ingressi CV del Noise Swash Eurorack.*

## 1. Che cos'è

Il Noise Swash è un generatore di rumore caotico che può anche lavorare come fuzz.

Il comparatore centrale (U1B, un TL062 ad anello aperto) è circondato da stadi di guadagno, tutti
alimentati da una linea **Low**. La linea Low passa per un potenziometro da 1 kΩ senza
condensatore, quindi la tensione cede quando gli op-amp assorbono corrente. Le polarizzazioni
seguono la tensione, e il circuito si retroaziona anche attraverso l'alimentazione.

Il plugin simula proprio questo circuito, componente per componente, a circa 192 kHz. Per questo:
- ogni posizione delle manopole è un "pedale diverso";
- i punti interessanti (*sweet spot*) sono stretti;
- dopo uno spegnimento il circuito impiega qualche secondo ad assestarsi, come l'originale.

## 2. Installazione (Mac)

1. Scarica `RISACCA-macOS.zip` dagli artefatti dell'azione **Build macOS** su GitHub.
2. Copia i file:
   - `RISACCA.vst3` in `~/Library/Audio/Plug-Ins/VST3`
   - `RISACCA.component` in `~/Library/Audio/Plug-Ins/Components`
   - `RISACCA.app` (versione standalone) dove preferisci
3. I file sono firmati ad-hoc. Se macOS li blocca, esegui
   `xattr -cr ~/Library/Audio/Plug-Ins/VST3/RISACCA.vst3` (e lo stesso per `.component` e `.app`).
4. In Ableton il plugin compare sotto **Collateral Effects**. Usalo come effetto audio sulla traccia
   della chitarra (preferibilmente la versione VST3). Una traccia audio non ha ingresso MIDI: per
   la KeyLab vedi il capitolo 5.

## 3. Il frontale

Tutte le manopole vanno da 0 (tutto antiorario) a 10 (tutto orario).
- **Trascina in verticale** per girarle; tieni premuto **Shift** per la regolazione fine.
- **Doppio clic** riporta al valore iniziale.
- **Tasto destro** apre il MIDI Learn.

| Comando | Cosa fa nel circuito | Uso |
|---|---|---|
| **Noisegate** (*Noise's Gait*) | Reostato da 10 kΩ tra l'ingresso + del comparatore e massa. Con R7 (470 kΩ dalla linea Low) fissa la soglia. | Lavora insieme allo Swash. In genere è più attivo verso sinistra. |
| **Swash** | Reostato da 10 kΩ fra i due ingressi del comparatore: decide quanto segnale "vede" la soglia. | È il cuore: gira piano (Shift) per trovare gli attrattori. |
| **Pregain** | Guadagno del primo stadio, da circa ×11 a ×1000 (con Trouble). | Più guadagno, più caos. |
| **Preclip** | Due diodi 1N914 in antiparallelo sul comparatore. | Ammorbidisce e limita. |
| **Postgain** | Guadagno dello stadio U2A, da ×1 a ×100. | |
| **Postclip** | Diodi nella retroazione di U2A, tramite un condensatore da 100 nF. | Taglio simmetrico che si "carica" col segnale. |
| **Bass / Treble** | Rete di tono del pedale grande. Entrambi i potenziometri sono in parallelo a resistenze fisse (10 kΩ e 1 kΩ). | L'effetto è sottile per costruzione (il Treble quasi impercettibile), come riportano anche i costruttori del pedale. |
| **Noise vol** | Guadagno dello stadio U2B, da 0 a ×10. | Volume del ramo noise; cambia anche l'autooscillazione. |
| **Self-osc** (manopola + levetta a sinistra) | Riporta l'uscita noise all'ingresso attraverso 0–1 MΩ. **Verso sinistra la retroazione aumenta.** | È il generatore: con la levetta su, il pedale suona da solo. |
| **Low** | Potenziometro da 1 kΩ in serie all'alimentazione di TL062/TL072 (cablaggio originale, pin 2-3). Tutto a sinistra: 1 kΩ, alimentazione più "affamata". Tutto a destra: 9 V pieni. | Ogni posizione è uno Swash diverso. |
| **Clean blend** | Volume del segnale pulito (4558 invertente, fino a ×21), miscelato passivamente con il noise. | |
| **Volume** | Master: potenziometro lineare da 100 kΩ, come nel layout. | |
| **LFO** (levetta) + **LFO speed** | Accende un oscillatore a rilassamento (4558) e lo collega, attraverso 100 Ω, al nodo dopo C4. È lo stesso nodo del jack Mood dell'Eurorack. | Da circa mezzo ciclo al secondo a circa 180 Hz a fine corsa. Quasi tutta la variazione è nell'ultimo quarto della manopola, come nell'originale con potenziometro lineare. A ogni accensione l'LFO riparte da capo. |
| **Tame/trouble** | **Trouble** (levetta giù): C2 = 10 µF, alimentazione sempre presente, rumore udibile anche in bypass. **Tame** (levetta su): C2 = 10 nF (il pregain resta solo sopra ~16 kHz), alimentazione tolta in bypass: silenzio totale. | Tame + Self-osc tende al fischio acuto. |
| **Bypass** (pulsante a pedale) | True bypass a 3 vie. | |

LED (da sinistra):
- **rosso**: effetto inserito;
- **blu**: "Mood", il LED blu dell'Eurorack; si accende quando il nodo dopo C4 supera ~3,4 V;
- **rosso a destra**: l'LFO.

## 4. Pannello CV / MOD

Si apre con il pulsante **CV / MOD** in alto a destra.

### Mood CV (dal Noise Swash Eurorack)

È l'equivalente di un cavo CV (sorgente con 1 kΩ) inserito nel jack **Mood**. Il jack è collegato al
nodo dopo C4, all'ingresso della rete di tono.

- **Cavo inserito**: senza cavo il nodo è libero. Un cavo a 0 V "tiene" il nodo e smorza il circuito.
  Già inserire il cavo cambia il suono.
- **Tensione 0–15 V**: sposta la polarizzazione del comparatore e della rete di tono.
- Con l'LFO acceso, Mood e LFO si contendono lo stesso nodo.

Come usarlo:
- assegna un fader della KeyLab (di serie: fader 4, CC 72) e "suona" la polarizzazione;
- in Ableton disegna un inviluppo di automazione sul parametro *Mood CV*. Gli impulsi brevi danno i
  "triggered noisy oscillation blasts" descritti da 4ms;
- con un modulatore Max for Live (LFO, Envelope Follower, Shaper) mappato su *Mood CV* ottieni
  modulazioni ritmiche o pilotate dall'audio.

### Swash CV ± (dal Noise Swash Eurorack)

È l'ingresso bipolare -15…+15 V. Il circuito originale lo amplifica (TL072, ×-6,7) e lo usa per
accendere due vactrol:

- **positivo** (LED arancio): il vactrol 1 collega l'uscita del tono al terzo pin dello Swash, cioè
  al lato del Noise's Gait;
- **negativo** (LED verde): il vactrol 2 collega l'ingresso - del comparatore a massa (è il pin 1
  dello Swash).

I vactrol hanno un attacco di circa 4 ms e un rilascio di circa 60 ms. Per questo le modulazioni
veloci vengono "arrotondate", come sull'hardware. Va bene per gesti con il fader (di serie: fader 5,
CC 80) o per automazioni a scalino.

### Mod (varianti dello schema Eurorack)

- **D2 = LED rosso**: nel modulo Eurorack uno dei diodi del Preclip è un LED rosso. Il taglio diventa
  asimmetrico e il LED si accende quando lavora.
- **Tone**: sostituisce la rete Bass/Treble con il Tone dell'Eurorack (simile al Big Muff, con
  passa-alto e passa-basso miscelati). C5 si sceglie tra **100 nF** (schema originale) e **10 nF**
  (layout del pedale base). In questa modalità la manopola **Treble fa da Tone** e Bass non agisce.

### Ingresso / uscita

- **Sorgente**:
  - *Chitarra* (di serie): pickup da 7 kΩ e 2,5 H con cavo da 470 pF. Lo Swash reagisce
    all'impedenza della sorgente;
  - *Linea*: 600 Ω;
  - *Scollegato*: il jack d'ingresso resta vuoto e il pedale funziona solo da generatore.
- **Ingresso**: 0 dBFS = 1 V di picco. Una chitarra registrata pulita sta di solito tra -18 e -6 dBFS.
- **Uscita**: 1 V = 0 dBFS, di serie -6 dB.

### Motore

- **Qualità**:
  - *Standard* (circa 192 kHz interni);
  - *Alta* (circa 384 kHz interni): transitori più fedeli nei fischi acuti, ma il doppio del carico.
- **Esemplare** (1–999): le tolleranze dei componenti, con la dispersione tipica delle parti vere:
  resistenze 1%; condensatori film e ceramici ±10% di targa, elettrolitici (da 1 µF in su) ±20%;
  potenziometri a carbone ±20% sul valore totale; per ogni op-amp offset, banda, slew rate e corrente
  di riposo (±20%). Ogni numero è un "pedale" diverso, sempre lo stesso a ogni apertura del progetto.
- **Nuovo esemplare**: ne estrae uno a caso.
- **Spegni e riaccendi**: stacca e riattacca l'alimentazione. Il circuito riparte da freddo e nei
  primi secondi, mentre C2 si carica, fa cose che non farà più dopo.
- **Carico**: percentuale di un core usata dal plugin.

## 5. MIDI e KeyLab

**Tasto destro** su qualunque comando (anche levette, pulsante a pedale e comandi del pannello CV)
→ **MIDI Learn** → muovi un controllo della KeyLab. Le assegnazioni si salvano nel progetto.

**In Ableton Live** il MIDI deve arrivare da una traccia MIDI:

1. Preferenze → Link, Tempo e MIDI: attiva **Track** sulla porta MIDI della KeyLab (non su quella DAW).
2. Crea una traccia MIDI con *MIDI From* = KeyLab e *Monitor* = **In**.
3. In *MIDI To* scegli la traccia audio con RISACCA e, nel menu sotto, **RISACCA**.

In alternativa si può usare la mappatura MIDI di Live (Cmd+M, con *Remote* attivo sulla porta
della KeyLab), che però esclude la mappa e il learn del plugin.

Mappa iniziale (KeyLab mkII, modalità User):

| Controllo KeyLab | CC | Parametro |
|---|---|---|
| Encoder 1–9 | 74, 71, 76, 77, 93, 18, 19, 16, 17 | Noisegate, Swash, Pregain, Postgain, Low, Self-osc, Noise vol, LFO speed, Volume |
| Fader 1–5 | 73, 75, 79, 72, 80 | Bass, Treble, Clean blend, Mood CV, Swash CV |

Le levette seguono il valore del CC: da 64 in su = acceso. Imposta i pulsanti della KeyLab in
modalità **Toggle** nel MIDI Control Center di Arturia.

**Mappa KeyLab** ripristina la mappa iniziale; **Cancella CC** toglie tutte le assegnazioni.

## 6. Per cominciare

- **Generatore**:
  1. Sorgente *Scollegato*, levetta Self-osc su;
  2. Noisegate verso destra (7–10): il circuito trova una voce che pulsa; verso sinistra fischia;
  3. gira lo Swash con Shift per spostare l'altezza della voce;
  4. poi prova il Low (verso sinistra la voce diventa un drone stabile) e l'LFO.
  Il manuale d'uso in PDF ha la mappa completa delle zone e le ricette.
- **Fuzz**: Sorgente *Chitarra*, Self-osc giù, Pregain 5, Postgain 5, Noise vol 6. Il Noisegate e lo
  Swash decidono quanto del suono "si spezza".
- **Entrambi**: Self-osc su, con la manopola Self-osc verso destra (poca retroazione). La chitarra
  innesca le oscillazioni.

## 7. Carico della CPU

Il plugin risolve a ogni campione interno un sistema di circa 25 equazioni non lineari. Il carico
dipende da cosa fa il circuito. Misure del motore a 192 kHz interni su un core Xeon a 2,3 GHz
(un Mac con Apple Silicon è in genere più veloce), in multipli del tempo reale:

| Regime | Tempo reale |
|---|---|
| Fuzz con chitarra | circa 3× |
| Raffiche e droni gravi | circa 2,5× |
| Tame + Self-osc | circa 1,3× |
| Rumore pieno con Self-osc (Noisegate e Swash a 5) | circa 0,8× |
| Fischio altissimo (Noisegate a 0) | circa 0,4× |

Nei regimi più pesanti il circuito commuta da un'alimentazione all'altra più volte per campione, e il
passo d'integrazione viene suddiviso fino a 1/16. Se serve, congela la traccia (*Freeze*). La qualità
*Alta* costa il doppio nei regimi leggeri ma circa lo stesso in quelli pesanti, dove è anche più precisa.

Versione 0.2: sono state provate e scartate un'eliminazione statica del blocco lineare (complemento di
Schur), varianti del predittore, delle tolleranze di Newton e del numero di iterazioni, e
micro-ottimizzazioni del modello degli op-amp: nessuna dava un guadagno misurabile. Il passo minimo è
sceso da 1/8 a 1/16: i passi non convergenti (prima circa 200 al secondo nel fischio) sono spariti a
parità di costo.

## 8. Limiti noti

- Lo schema del Max Tweaker non è pubblico: la netlist viene da un layout veroboard basato sullo
  Swash 2.2, verificato componente per componente sullo schema Eurorack ufficiale.
- Il confronto con un pedale reale non è stato possibile.
- Approssimazioni:
  - 4,7 nF parassiti sulla linea Low;
  - le polarizzazioni leggono la linea Low con ~5 µs di ritardo;
  - LFO e ramo clean sono accoppiati al nucleo con un campione di ritardo;
  - il comportamento dei TL062/TL072 vicino al V- (fuori dal modo comune garantito) è modellato in
    modo semplificato.
