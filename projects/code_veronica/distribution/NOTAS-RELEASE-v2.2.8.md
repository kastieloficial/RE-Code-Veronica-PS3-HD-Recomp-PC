# v2.2.8 — V-Sync option now available in 30 FPS mode / Opção de V-Sync agora disponível também no modo 30 FPS

## English

- **Enabled the V-Sync toggle in 30 FPS mode (`projects/code_veronica/launcher.cpp`)**:
  - Previously, the V-Sync option in the launcher was only shown and applied when the frame limit was set to **60 FPS**, leaving **30 FPS** sessions always running with tearing allowed (`Present(0, ALLOW_TEARING)`), which caused screen tearing on non-VRR / standard monitors in fullscreen.
  - V-Sync is now available and switches live in **both 30 FPS and 60 FPS modes** across **EU, US and JP** (kept off by default; remains disabled only while FSR 3.1 Frame Generation is active, since its swapchain paces itself).
- Includes everything from **v2.2.7**, **v2.2.6**, **v2.2.5**, **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1**, and **v2.2.0** (Windows Per-Monitor V2 HiDPI scaling fix for 4K TVs/monitors, fix for opaque square particles after closing the in-game OPTION menu, D3D12 `wait_for_gpu` fence reuse fix for elevator/room GPU freezes, `cellPamf` 64-slot LRU ending/credits black-screen fix, SPU DMA guard for `rm_4020`, OBS capture for `F6`/`F7`/`F2` modals, Antarctica 60 FPS fixes).

Downloads: extract the complete **CodeVeronica-PC-v2.2.8.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Opção de V-Sync liberada também no modo 30 FPS (`projects/code_veronica/launcher.cpp`)**:
  - Antes, o botão de V-Sync no launcher só aparecia e só era aplicado quando o limite estava em **60 FPS**, deixando o modo **30 FPS** sempre sem sincronização vertical (`Present(0, ALLOW_TEARING)`), o que causava tela dividida/rasgada (*screen tearing*) em monitores comuns em tela cheia.
  - Agora o V-Sync pode ser ativado/desativado ao vivo tanto em **30 FPS** quanto em **60 FPS** em **EU, US e JP** (continua desligado por padrão; fica bloqueado apenas enquanto o FSR 3.1 Frame Generation estiver ativo, pois ele controla a própria sincronização).
- Inclui tudo das versões **v2.2.7**, **v2.2.6**, **v2.2.5**, **v2.2.4**, **v2.2.3**, **v2.2.2**, **v2.2.1** e **v2.2.0** (correção de escala HiDPI Per-Monitor V2 para TVs/monitores 4K, correção das partículas quadradas após o menu OPTION, correção do congelamento da GPU DirectX 12 em elevadores/salas, tabela LRU de 64 slots do `cellPamf` para tela preta antes dos créditos, proteção de DMA da SPU em `rm_4020`, captura no OBS para painéis `F6`/`F7`/`F2` e correções de 60 FPS na Antártida).

Downloads: extraia o **CodeVeronica-PC-v2.2.8.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
