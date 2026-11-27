# v2.2.0 — One-hit kill, Original Saves tab, WASD menu navigation, 60 FPS & elevator refinements / Matar com um golpe, aba Saves Originais, navegação WASD e refinamentos de 60 FPS e elevadores

## English

- **New cheat: "One-hit kill"** (launcher, **Inventory → Cheats** tab, off by default). Defeats enemies and bosses with a single hit from any weapon or knife. Can be toggled live during gameplay and is translated in all **13 launcher languages**.
- **Archives (`F7`) — Dedicated "ORIGINAL SAVES" tab**: Native in-game saves (`DATA0.DAT`) are now separated into their own read-only tab (**ORIGINAL SAVES**), keeping your custom `.cvsav` saves clean and organized in **LOAD** and **SAVE**. Translated in all **13 launcher languages**.
- **`WASD` keyboard navigation in menus**: Both **Archives (`F7`)** and **Inventory (`F6`)** now support `W`, `A`, `S`, `D` alongside the arrow keys (preserving letter typing in the Maps search box), with updated keyboard footer hints. Controller bindings remain unchanged.
- **Skip transition scenes refinements**:
  - Fixed Chris 1F elevator (`rm_701` → `rm_713`) floor flag polarity so the destination always matches the chosen floor.
  - Refined the Area 9 lift first-ride check so the transition cut triggers cleanly without skipping scripted events.
- **60 FPS & visual fixes**:
  - Unified 60 FPS smooth animation interpolation across unskippable non-dialogue cutscenes and inspection/item transitions.
  - Fixed missing Albinoid water lightning ribbons in the D3D12 renderer.
- Includes everything from **v2.1.0** (Skip transition scenes, 6,294 pre-seeded shaders, paid Japanese PSN edition support). Applies to **EU, US and JP**.

Downloads: extract the complete **CodeVeronica-PC-v2.2.0.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Novo cheat: "Matar com um golpe"** (launcher, aba **Inventário → Cheats**, desligado por padrão). Derrota inimigos e chefes com um único ataque de qualquer arma ou faca. Pode ser ligado e desligado ao vivo durante a partida, traduzido nos **13 idiomas** do launcher.
- **Arquivos (`F7`) — Aba dedicada "SAVES ORIGINAIS"**: Os saves nativos do jogo (`DATA0.DAT`) agora ficam separados em uma aba própria somente leitura (**SAVES ORIGINAIS**), mantendo seus saves livres (`.cvsav`) organizados em **CARREGAR** e **SALVAR**. Traduzido nos **13 idiomas** do launcher.
- **Navegação por `WASD` no teclado**: Os menus **Arquivos (`F7`)** e **Inventário (`F6`)** agora aceitam `W`, `A`, `S`, `D` junto com as setas (mantendo a digitação normal na busca de mapas), com as dicas de teclado atualizadas no rodapé. O controle não foi alterado.
- **Refinamentos em "Pular cenas de transição"**:
  - Corrigida a polaridade da flag do elevador 1F do Chris (`rm_701` → `rm_713`) para sempre levar ao andar correto.
  - Refinada a condição da primeira viagem no elevador da Área 9 para cortar a transição sem interferir em eventos de roteiro.
- **Correções de 60 FPS e visuais**:
  - Interpolação suave de 60 FPS unificada nas cutscenes não puláveis sem diálogo e nas transições de itens/inspeção.
  - Corrigidos os raios na água do Albinoid no renderizador D3D12.
- Inclui tudo da **v2.1.0** (Pular cenas de transição, 6.294 shaders pré-compilados, suporte à edição japonesa paga da PSN). Vale para **EU, US e JP**.

Downloads: extraia o **CodeVeronica-PC-v2.2.0.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
