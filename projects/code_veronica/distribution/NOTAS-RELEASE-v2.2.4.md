# v2.2.4 — OBS capture for port menus (F6/F7/F2) & 60 FPS Antarctica fixes / Captura no OBS para menus do port (F6/F7/F2) e correções de 60 FPS na Antártida

## English

- **Port menus (`F6` Items/Weapons, `F7` Archives/Saves/Maps, `F2` Return to Title) now visible in OBS Window Capture and Game Capture**:
  - Previously, because these menus are separate Win32 popup windows, OBS Window Capture and Game Capture of the game window only captured the paused game behind them.
  - Their painted output is now mirrored directly into the game's D3D12 backbuffer while open (`rsx_d3d12_present_modal`), keeping mouse, keyboard, gamepad, focus and layout behavior untouched with zero overhead during gameplay.
- **60 FPS fix for the Antarctica freezer thermostat puzzle (`rm_705`, `12.8 F` 2D panel)**:
  - Added `system+0x70` bit `0x40000000` to the native 60 FPS exclusion mask (`0x5C040308`) in `cv_smooth_animation.h` so 2D interactive puzzle overlays never flicker or disappear at 60 FPS.
- **60 FPS enabled in Antarctica hall `rm_703 v0` without blanking Alexia's monitor**:
  - Allowed `system+0x6C` bit `0x200` during free gameplay in `rm_703` (`!(camera_mode & 7)`) so walking runs smoothly at 60 FPS, while immediately returning to native frames when the scripted zoom onto Alexia's big monitor begins.
- All fixes apply across **EU, US and JP**, and include everything from **v2.2.3**, **v2.2.2**, **v2.2.1**, and **v2.2.0**.

Downloads: extract the complete **CodeVeronica-PC-v2.2.4.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Painéis do port (`F6` Itens/Armas, `F7` Arquivos/Saves/Mapas e `F2` Voltar ao Título) agora aparecem na Captura de Janela e Captura de Jogo do OBS**:
  - Antes, por serem janelas popup separadas do Windows, o OBS capturando apenas a janela do jogo mostrava só o jogo pausado atrás deles.
  - Agora a imagem desses painéis é espelhada diretamente no backbuffer D3D12 do jogo enquanto estão abertos (`rsx_d3d12_present_modal`), mantendo mouse, teclado, controle, foco e layout exatamente iguais e sem nenhum custo de desempenho na gameplay.
- **Correção em 60 FPS do puzzle da geladeira na Antártida (`rm_705`, painel 2D de temperatura `12.8 F`)**:
  - Adicionado o bit `0x40000000` de `system+0x70` na máscara de exclusão da interpolação (`0x5C040308` em `cv_smooth_animation.h`), impedindo que o painel 2D pisque ou suma em 60 FPS.
- **60 FPS liberados na sala da Antártida `rm_703 v0` sem apagar o telão da Alexia**:
  - O bit `0x200` de `system+0x6C` agora é liberado em `rm_703` apenas durante a gameplay livre (`!(camera_mode & 7)`), mantendo 60 FPS ao andar pela sala e voltando automaticamente ao frame nativo no início do zoom scriptado no telão da Alexia.
- Todas as correções valem para **EU, US e JP** e incluem tudo das versões **v2.2.3**, **v2.2.2**, **v2.2.1** e **v2.2.0**.

Downloads: extraia o **CodeVeronica-PC-v2.2.4.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
