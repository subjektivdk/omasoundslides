# TODO / noter

## Brug standarder så vidt muligt

Programmet skal bruge etablerede standarder frem for egne formater og løsninger, hvor der findes en. Det gælder formater, konventioner og biblioteker, fx:

- **Filformater:**
  - JSON til projektet
  - Audacitys label-format
  - SRT/WebVTT til undertekster
  - MP4/H.264 til video
- **Omarchy- og freedesktop-konventioner:**
  - xdg-desktop-portal til filvalg
  - XDG-mapper (`~/.config`, `~/.cache`)
  - `.desktop`-fil
  - Omarchys tema og editor
- **Qt's og ffmpeg's egne mekanismer** frem for hjemmelavede.

Spørgsmålet skal stilles ved hver ny funktion: *findes der allerede en standard for det her?*

## Er programmet AI-agent-venligt?

Undersøg og beslut: skal man kunne skrive en prompt til en kodeagent (fx Claude Code eller Pi i Omarchy) og få den til at styre programmet?

**Sådan er det i dag:**
- ✅ **Projektfilen** er ren, dokumenteret JSON, som en agent kan læse og skrive direkte.
- ✅ **Kommandolinjen** kan rendere (`render`), vise tidslinjen (`info`) og liste overgangene (`transitions`), og den giver tydelige fejlbeskeder og exit-koder.
- ❌ **Redigering fra kommandolinjen:** Der er ingen kommandoer til at tilføje billeder, sætte varighed, sætte markører og lignende. En agent må redigere JSON'en selv.
- ❌ **Maskinlæsbart output:** `info` skriver kun tekst, ikke JSON.
- ❌ **Det kørende vindue** kan ikke styres udefra (ingen D-Bus, socket eller MCP), og det ser ikke ændringer i projektfilen, mens det er åbent.

**Mulige skridt** (i stigende omfang):
1. **`info --json`:** tidslinje, varigheder, markører og problemer som JSON.
2. **Redigeringskommandoer på kommandolinjen:** fx `add-images`, `set`, `markers import`, `fit audio|markers`. De virker direkte på projektfilen og kan køres uden vindue.
3. **Genindlæsning:** Vinduet genindlæser projektfilen, når den ændres udefra, så en agent og brugeren kan arbejde samtidig.
4. **D-Bus-interface til det kørende vindue** (freedesktop-standarden): afspil, gå til tidspunkt, vælg billede, eksportér.
5. **MCP-server** (Model Context Protocol, standarden for AI-værktøjer): kan bygges oven på 2 og 4, så agenter får programmets funktioner som værktøjer.
