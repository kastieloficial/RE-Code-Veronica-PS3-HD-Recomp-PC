# v2.2.1 — CGI cutscene stability & fast-skip fixes, expanded support log / Estabilidade em cutscenes CGI ao pular rápido e log de suporte aprimorado

## English

- **Fixed crashes, black screens, and missing textures after fast-skipping CGI cutscenes** (e.g., dragonfly cutscene after saving Steve, pre-1971 PC puzzle cutscene, and long uninterrupted runs/speedruns):
  - Reduced oversized HLE buffer requests in `cellDmuxQueryEsAttr`, `cellVdecQueryAttr`, and `cellAdecQueryAttr` (sizing audio and video ES buffers per stream type), freeing **~6.3 MB** on the tight 202 MB PS3 guest heap so CGI frame ring-buffer allocations no longer fail with `NULL` on fragmented heaps.
  - Synchronized fast-skip shutdown across `cellVdecEndSeq`, `cellAdecEndSeq`/`Close`, and `cellDmuxDisableEs`/`ResetStream` so worker threads stop cleanly without concurrent guest callbacks or stalling on skip.
- **Expanded startup support log (`run-log.txt`)**:
  - Lightweight one-time startup header now includes display mode (`WINDOW_MODE`), smooth 60 FPS (`CV_SMOOTH_ANIMATION`), letterbox, DoF, SRT, active patches (`PS3_PATCHES`), and a concise `[SUPPORT-GAMEPLAY]` summary line showing `door_skip` mode (`off` / `classic` / `fast` / `supreme`), `skip_events`, `skip_logos`, and active cheats — zero runtime performance overhead.
- Includes everything from **v2.2.0** (One-hit kill cheat, dedicated Original Saves tab, WASD menu navigation, 60 FPS & elevator refinements). Applies to **EU, US and JP**.

Downloads: extract the complete **CodeVeronica-PC-v2.2.1.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Corrigidos travamentos, tela preta e texturas ausentes ao pular cutscenes em CGI rápido demais** (ex.: cena da libélula após salvar o Steve, cena antes do puzzle do PC 1971 e partidas longas/speedruns sem reiniciar):
  - Reduzido o excesso de memória pedido nos `QueryAttr` do HLE (`cellDmuxQueryEsAttr`, `cellVdecQueryAttr` e `cellAdecQueryAttr`, dimensionando vídeo e áudio separadamente), devolvendo **~6,3 MB** de folga para a heap de 202 MB do jogo e evitando falhas `NULL` na alocação dos buffers de vídeo quando a memória está fragmentada.
  - Sincronizado o encerramento rápido (skip) em `cellVdecEndSeq`, `cellAdecEndSeq`/`Close` e `cellDmuxDisableEs`/`ResetStream` para que as threads de decode/demux parem de forma limpa e instantânea, sem callbacks concorrentes nem travamentos.
- **Log de suporte aprimorado (`run-log.txt`)**:
  - O cabeçalho leve de inicialização agora registra também modo de tela (`WINDOW_MODE`), 60 FPS suave (`CV_SMOOTH_ANIMATION`), letterbox, DoF, SRT, patches ativos (`PS3_PATCHES`) e uma linha resumo `[SUPPORT-GAMEPLAY]` mostrando o modo de **Door Skip** (`off` / `classic` / `fast` / `supreme`), **Pular cenas de transição**, **Pular logos** e **cheats** ativos — sem impacto algum no desempenho.
- Inclui tudo da **v2.2.0** (cheat Matar com um golpe, aba Saves Originais, navegação WASD nos menus, refinamentos de 60 FPS e elevadores). Vale para **EU, US e JP**.

Downloads: extraia o **CodeVeronica-PC-v2.2.1.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
