# v2.2.6 — Fix for ending/credits black screen after multiple FMV cutscenes (cellPamf reader table) / Correção da tela preta no final/créditos após vários vídeos FMV (tabela do cellPamf)

## English

- **Fixed the black screen right before the ending credits / ranking screen (`mv_021.pam` & `mv_015.pam`) during long sessions or full runs**:
  - Diagnosed from a full-run `run-log.txt`: the host-side `cellPamf` reader table (`libs/codec/cellPamf.c`) only had **8 static slots** (`PAMF_MAX_READERS = 8`) and never released them (because the PS3 SDK has no `cellPamfReaderClose`).
  - Once a session played or skipped **8 FMV `.pam` movies** allocated at distinct guest heap addresses, the **9th movie** (typically the final CGI `mv_021.pam` and ending credits `mv_015.pam`) caused `cellPamfReaderInitialize` to fail silently, leaving the game stuck on a black screen with only the ending music playing instead of advancing to the ranking screen.
  - Raised `PAMF_MAX_READERS` from `8` to `64` and added automatic **LRU slot recycling**, so FMV playback and fast skipping work reliably across unlimited consecutive runs in all regions (**EU, US and JP**).
- Includes everything from **v2.2.5**, **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1**, and **v2.2.0** (SPU DMA out-of-range LSA guard fixing the random `rm_4020`/room-transition freeze, OBS Window/Game Capture for `F6`/`F7`/`F2` port panels, Antarctica `rm_705` freezer puzzle & `rm_703` 60 FPS fixes, 3D cutscene fast-skip fade protection, CGI cutscene stability fixes).

Downloads: extract the complete **CodeVeronica-PC-v2.2.6.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Correção da tela preta antes dos créditos finais / tela de tempo e nota (`mv_021.pam` e `mv_015.pam`) em runs completas ou sessões longas**:
  - Diagnosticado a partir de um `run-log.txt` de run completa: a tabela interna de leitores do `cellPamf` (`libs/codec/cellPamf.c`) tinha apenas **8 slots fixos** (`PAMF_MAX_READERS = 8`) e nunca liberava slots antigos (pois o SDK do PS3 não possui `cellPamfReaderClose`).
  - Ao assistir ou pular **8 vídeos `.pam`** alocados em endereços diferentes na mesma sessão, o **9º vídeo** (normalmente a CGI final `mv_021.pam` e os créditos `mv_015.pam`) falhava silenciosamente no `cellPamfReaderInitialize`, deixando o jogo preso em tela preta só com a música tocando antes da tela de ranking.
  - Aumentado o limite para **64 slots** com **reciclagem automática LRU** (reaproveita o slot mais antigo caso um dia encha), garantindo que todos os vídeos e o pulo rápido funcionem sem falhas por quantas runs seguidas quiser em todas as regiões (**EU, US e JP**).
- Inclui tudo das versões **v2.2.5**, **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1** e **v2.2.0** (proteção de DMA da SPU que corrigiu o travamento aleatório na troca de sala/`rm_4020`, captura no OBS para os painéis `F6`/`F7`/`F2`, correções de 60 FPS na Antártida em `rm_705` e `rm_703`, proteção de fade ao pular cutscenes 3D rápido e estabilidade em cutscenes CGI).

Downloads: extraia o **CodeVeronica-PC-v2.2.6.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
