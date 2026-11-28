# v2.2.5 — Fix for random black-screen room-transition freeze (SPU DMA guard) / Correção do travamento aleatório em tela preta na troca de sala (proteção de DMA da SPU)

## English

- **Fixed the random black-screen freeze during fast room transitions (such as `rm_4020` with Door Skip Supreme)**:
  - Diagnosed from a captured `hang-log.txt` + `run-log.txt`: during fast room transitions, a background SPU job (`img=10`) could occasionally issue a runaway `cellDmaLargePut` with a corrupted Local Store address (`lsa >= 256 KB`, e.g. `0x3D39C000`), sweeping across guest RAM and zeroing out engine pointers and the RSX FIFO.
  - `runtime/spu/spu_dma.h` now rejects any MFC DMA transfer with `lsa >= SPU_LS_SIZE` or `lsa + size > SPU_LS_SIZE` (256 KB hardware Local Store limit) before masking or copying, preventing any memory corruption across all rooms and regions (**EU, US and JP**) with zero impact on valid transfers.
- Includes everything from **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1**, and **v2.2.0** (OBS Window/Game Capture for `F6`/`F7`/`F2` port panels, Antarctica `rm_705` freezer puzzle & `rm_703` 60 FPS fixes, 3D cutscene fast-skip fade protection, CGI cutscene stability fixes).

Downloads: extract the complete **CodeVeronica-PC-v2.2.5.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Correção do travamento aleatório em tela preta na troca rápida de sala (como na `rm_4020` com Door Skip Supreme)**:
  - Diagnosticado via `hang-log.txt` + `run-log.txt`: em trocas rápidas de sala, uma tarefa paralela da SPU (`img=10`) podia ocasionalmente disparar uma cópia DMA com endereço interno corrompido (`lsa >= 256 KB`, ex.: `0x3D39C000`), sobrescrevendo a memória do jogo e zerando o FIFO da GPU e os ponteiros do motor.
  - O `runtime/spu/spu_dma.h` agora rejeita imediatamente qualquer transferência DMA com `lsa >= 256 KB` (limite físico da Local Store da SPU do PS3) antes de qualquer escrita na RAM, protegendo todas as salas e regiões (**EU, US e JP**) sem nenhum impacto em transferências válidas.
- Inclui tudo das versões **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1** e **v2.2.0** (captura no OBS para os painéis `F6`/`F7`/`F2`, correções de 60 FPS na Antártida em `rm_705` e `rm_703`, proteção de fade ao pular cutscenes 3D rápido e estabilidade em cutscenes CGI).

Downloads: extraia o **CodeVeronica-PC-v2.2.5.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
