# Forslag: hvad vi kan lære af Kdenlive

**Status (27. september 2026):** Punkt 1–7 er lavet. 8 er lavet på vores egen måde: musehjulet zoomer over sporene, og Shift + hjul ruller. Tilbage er 9 (tekstspor).

Kildekoden er gennemgået fra `invent.kde.org/multimedia/kdenlive` (september 2026), især `src/timeline2/` og `src/undohelper.*`.

## Licens: inspiration eller direkte kopi?

Al relevant Kdenlive-kode er **GPL-3.0-only** (`SPDX: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL`).

- **Inspiration** (vi skriver selv koden efter samme idé) har ingen betingelser. **Det anbefales.** Stykkerne er små, og vores egne udgaver bliver kortere, fordi vi kun har ét billedspor og ét lydspor.
- **Direkte kopi** er lovligt, men så skal hele omasoundslides udgives under GPL-3. Det kan godt lade sig gøre, fordi Omacut-koden, vi har lånt, er MIT, og MIT må indgå i GPL-projekter. Det er dog en beslutning om projektets licens, som bør træffes bevidst. Omasoundslides har endnu ingen `LICENSE`-fil, så den beslutning er alligevel ikke truffet.

Nedenfor står det mest værdifulde først. "Indsats" er et groft skøn.

---

## 1. Fortryd / gentag (Ctrl+Z / Ctrl+Shift+Z)

**Kdenlive:** `src/undohelper.hpp` har en `FunctionalUndoCommand`, som tager to lambdas (undo og redo) og lægger dem på en `QUndoStack`. Alle ændringer i tidslinjen går den vej.

**Hos os:** Vi har slet ikke undo. Det er den største mangel i forhold til en "rigtig" editor, og en slettet eller flyttet billede kan ikke fortrydes.

**Forslag:** Da hele projektet er en lille værdi (`Project`), er det enkleste at gemme et *snapshot* før hver ændring i `ProjectModel` og lægge det på en `QUndoStack`. Scroll og træk samles til én ændring med `QUndoCommand::mergeWith`, så 20 hak med musehjulet kan fortrydes i ét. Det er enklere end Kdenlives lambda-model og kræver ingen GPL-kode.

**Indsats:** lille til mellem.

## 2. Markører, som man sætter mens lyden spiller: Soundslides' "præcis timing"

**Kdenlive:** "Guides" er markører på linealen (`Ruler.qml`, `MarkerListModel`) med farve og navn. De sættes ved playhead og bruges som snap-punkter.

**Hos os:** Det passer direkte til Soundslides' arbejdsgang. Man lytter til interviewet, trykker **M** ved hvert sted, hvor billedet skal skifte, og lader derefter billederne klikke på plads ved markørerne.

**Forslag:**
- **Sæt markører:** **M** sætter en markør ved playhead, også under afspilning. Den vises som en streg på linealen og gennem lydsporet og gemmes i projektfilen (`"markers": [12.4, 31.0, …]`).
- **Fordel på markørerne:** Handlingen "Fit images to markers" sætter billede n's start ved markør n. Det er en ny variant af "Fit images to the audio".
- **Hop mellem markører:** **,** og **.** hopper til forrige og næste markør (Kdenlive har det samme for guides).

**Indsats:** mellem. Det er nok den funktion, der mest gør appen til "Soundslides" og ikke bare "slideshow".

## 3. Snapping, når man trækker i et billedes kant

**Kdenlive:** `src/timeline2/model/snapmodel.cpp` har en sorteret liste af snap-punkter. `getClosestPoint()` finder det nærmeste med `lower_bound`, og `proposeSize()` snapper kun, hvis afstanden er under en grænse. Man holder Shift nede for at slå snapping fra (`Timeline.qml` omkring linje 1681).

**Hos os:** Når man trækker i højre kant, kan billedet ikke ramme lydens slutning, playhead eller en markør præcist.

**Forslag:** Snap-punkterne er playhead, markører (punkt 2), lydens slutning og de andre billeders grænser. Grænsen sættes til ca. 8 pixel, så den følger zoom, og Shift slår snapping fra. Det er omkring 30 linjer i `Timeline.qml` eller i modellen.

**Indsats:** lille.

## 4. Vis varigheden, mens man trækker eller scroller

**Kdenlive:** Mens man trimmer et klip, vises en lille boks med den nye længde og ændringen (`Clip.qml`, trim-områderne).

**Forslag:** Vis "9.32 s → 10.10 s (+0.78)" over billedet, mens man trækker i kanten eller scroller. Det er især nyttigt, når billedet er for smalt til sin egen varigheds-etiket.

**Indsats:** lille.

## 5. Overgangen som figur i billedsporet

**Kdenlive:** `MixShape.qml` tegner overlappet mellem to klip på samme spor som en halvgennemsigtig trekant (Qt Quick `Shape`). Man kan trække i den for at ændre overgangens længde.

**Hos os:** Vi tegner i dag en mørk gradient, som er svær at se, når man er zoomet ud.

**Forslag:**
- **Tydeligere figur:** Tegn overlappet som en diagonal. Crossfade bliver en trekant, og fade out/in to trekanter, der mødes i sort. Så kan man se typen direkte i sporet.
- **Skift hastighed:** Et klik på figuren skifter Fast → Medium → Slow, eller åbner en lille menu med de syv valg.

**Indsats:** lille til mellem.

## 6. Fade-håndtag direkte på lydsporet

**Kdenlive:** Klip har små håndtag i øverste hjørner. Man trækker dem ind for at lave fade ind/ud og ser kurven tegnet ovenpå (`Clip.qml`: `fadeInMouseArea`, `fadeOutMouseArea`, `fadeInTriangle`). Man kan også vælge kurvetype (`fadeInMethod`).

**Hos os:** Fade ind/ud på lyden sættes kun med talfelter i projekt-fanen.

**Forslag:**
- **Håndtag:** Hjørnehåndtag på lydsporet, som skriver i de samme `audio_fade`-værdier, og som snapper til hele eller halve sekunder.
- **Kurvetype:** ffmpeg's `afade` understøtter også kurver (`curve=qsin`, `log` osv.). En kurvetype kunne være en senere mulighed, men lineær er fint til at starte med.

**Indsats:** lille til mellem.

## 7. Bedre waveform, når man zoomer ind

**Kdenlive:** `timelinewaveform.cpp` tegner lodrette linjer, når der er flere målepunkter pr. pixel, og en **udfyldt kurve** (path), når man er zoomet så langt ind, at der er under ét punkt pr. pixel. Den tegner også en midterlinje og kan vise kanalerne hver for sig (L/R).

**Hos os:** Vi har 100 målepunkter pr. sekund og tegner altid linjer. Ved høj zoom (400 px/s) bliver waveformen "trappet".

**Forslag:**
- **Tæthed:** Gem 400 målepunkter pr. sekund i stedet for 100. Det koster stadig kun få MB for en time.
- **Tegning:** Brug en udfyldt kurve, når der er under ét punkt pr. pixel, og tegn en svag midterlinje.

**Indsats:** lille.

## 8. Musehjul og zoom som i Kdenlive

**Kdenlive** (`Timeline.qml` omkring linje 228–290):
- **Ctrl+hjul:** zoom. Den lægger små trin sammen (`wheelAccumulatedDelta`), præcis som vi nu også gør.
- **Shift+hjul:** vandret scroll.
- **Hjul over linealen:** flytter playhead et billede ad gangen.
- **Zoom-bar:** Der er en scrollbar, man kan trække i enderne af for at zoome (`horZoomBar`).

**Hos os:** Shift+hjul over et billede betyder "×5 varighed", hvilket strider mod Kdenlives Shift = scroll.

**Forslag:**
- **Behold** den nuværende hjul-adfærd, men lad hjul over linealen flytte playhead i små trin, ligesom Kdenlive.
- **Overvej** at lade Shift+hjul scrolle vandret og lægge "×5" på Alt+hjul. Det er brugerens valg, og det kan sættes i `keybindings.conf`.
- **Zoom-bar:** Gør vores scrollbar trækbar i enderne.

**Indsats:** lille.

## 9. Undertekster og tekst på billedet (senere)

**Kdenlive:** Et separat undertekst-spor (`SubTitle.qml`, `SubtitleTrackHead.qml`) med tekstblokke, man kan trække og trimme, og import/eksport af SRT.

**Hos os:** Svarer til Soundslides' "lower third"-tekst (se rapporten, afsnit 1).

**Forslag:** Et tredje spor til tekst med start og længde, renderet med ffmpeg's `drawtext` eller `subtitles`-filter, og eksport af en `.srt` ved siden af videoen. Det er et større stykke arbejde og hører til senere.

**Indsats:** stor.

---

## Hvad vi *ikke* skal tage fra Kdenlive

- **MLT som motor:** Vores ffmpeg-kommando er enklere og allerede testet. Kdenlive har brug for MLT, fordi den har mange spor, effekter og keyframes.
- **Gruppering, flere spor, keyframes, effekt-stakke:** Det er uden for, hvad en Soundslides-klon skal kunne.
- **Kdenlives QML i sin helhed:** `Timeline.qml` er 2.700 linjer og `Clip.qml` 2.000, tæt vævet sammen med KDE-biblioteker og Kdenlives C++-modeller. Små idéer kan løftes, hele filer kan ikke.

## Forslag til rækkefølge

1. **Undo/redo (1):** Det er fundamentet, før der kommer flere redigeringsmåder.
2. **Markører + snapping (2 + 3):** Det er Soundslides' kernefunktion.
3. **Småting i tidslinjen (4, 5, 6, 7, 8):** Hver er lille og kan tages én ad gangen.
4. **Tekstspor (9):** Når resten sidder.

---

# Eksportformater (undersøgt 27. september 2026)

I dag eksporterer vi altid **H.264 + AAC i MP4** (libx264, CRF 20, preset medium, 192 kbit/s lyd).

## Måling

Målt på 40 s af testprojektet i 1080p30, med en tabsfri reference. Kvaliteten er målt med **VMAF** (0–100; over ca. 93 kan man ikke se forskel på originalen). Tiden inkluderer læsning af en stor tabsfri referencefil, så tallene kan kun sammenlignes indbyrdes.

| Encoder | Tid | Størrelse | VMAF | Kommentar |
|---|---|---|---|---|
| **x264 CRF 20 medium (i dag)** | 18,7 s | 6,4 MB/min | 95,8 | God balance, spiller overalt |
| x264 CRF 20 `-tune stillimage` | 18,2 s | 9,6 MB/min | 96,5 | 50 % større for næsten ingen gevinst. **Nej** |
| x264 CRF 23 fast | 15,0 s | 5,0 MB/min | 94,4 | Lille fil, stadig fin kvalitet |
| x264 CRF 18 slow | 20,1 s | 7,8 MB/min | 96,4 | Høj kvalitet |
| x265 (HEVC) CRF 24 | 22,4 s | 3,1 MB/min | 94,2 | Halv størrelse. Spiller ikke alle steder (ældre Windows, nogle browsere) |
| **SVT-AV1 CRF 32 preset 8** | 16,0 s | **4,0 MB/min** | **96,7** | 37 % mindre end i dag og *bedre* kvalitet, lige så hurtig |
| VP9 CRF 32 (WebM) | 53,3 s | 4,0 MB/min | 94,3 | Tre gange så langsom som AV1 og ikke bedre. **Nej** |
| H.264 via GPU (VAAPI, Radeon 680M) | 10,7 s | 7,0 MB/min | 93,6 | Hurtigst, men større og ringere |
| HEVC via GPU (VAAPI) | 10,7 s | 5,7 MB/min | 0,3 | **Fejlagtigt output** på denne driver. **Nej** |
| ProRes 422 HQ (MOV) | 39,2 s | 1.500 MB/min | 97,1 | Til videre redigering i Resolve/Premiere |

**Konklusioner:**
- H.264 er det rigtige standardvalg, fordi det spiller alle steder.
- AV1 giver de mindste filer med den bedste kvalitet. Det kan afspilles af YouTube, Vimeo, alle moderne browsere og nyere telefoner, men ikke af ældre enheder og nogle tv'er.
- Diasshows er mest stillbilleder, så de fylder lidt i forvejen. En tuning som `stillimage` betaler sig ikke.
- GPU-kodning sparer ikke meget, fordi eksporten allerede er hurtig (ca. 30 % af realtid), og den giver ringere kvalitet.

## Beslutning (27. september 2026)

**Kun MP4 med H.264**, med kvaliteten **Standard** (CRF 20, medium) eller **High** (CRF 18, slow). Valget ligger i gem-dialogen og i Project-fanen og gemmes i projektet. Det er lavet. AV1, ProRes, opløsning ved eksport og ekstra filer (punkterne nedenfor) er fravalgt for nu.

## Forslag (til senere)

1. **Format- og kvalitetsvalg i eksport-dialogen.** Omarchys filvælger (portalen) understøtter drop-down-menuer i selve gem-dialogen. Det bruger Omacut allerede til "Quality". Valgene huskes i projektfilen (`"export": {"format": …, "quality": …}`).
   - **Format:**
     - *MP4 – H.264* (standard, spiller overalt)
     - *MP4 – AV1* (mindre filer, moderne afspillere)
     - *MOV – ProRes* (til videoredigering)
   - **Kvalitet:**
     - *Lille*: x264 CRF 23 fast / AV1 CRF 36
     - *Standard*: CRF 20 medium / AV1 CRF 32
     - *Høj*: CRF 18 slow / AV1 CRF 28
2. **Opløsningen i eksporten** i stedet for kun i projekt-fanen, fx 1080p, 720p og "som projektet". Det er praktisk til en hurtig lille version til mail eller en preview.
3. **Ekstra filer ved siden af videoen** (afkrydsning):
   - **Plakatbillede (JPEG):** første billede, til web-indlejring og thumbnails.
   - **Lydsporet alene (M4A):** med fades, som det lyder i videoen.
   - **Undertekster/kapitler (SRT/WebVTT)** fra markørerne. Markørernes tekst mangler dog stadig (se "Tekstspor" ovenfor).
4. **Ikke med:** VP9/WebM, HEVC og GPU-kodning. Målingerne viser ingen gevinst, eller at de ikke virker på denne maskine.
