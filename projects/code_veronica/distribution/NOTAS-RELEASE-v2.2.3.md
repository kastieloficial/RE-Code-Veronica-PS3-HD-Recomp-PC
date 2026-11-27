# v2.2.3 — Complete 2D fade protection when fast-skipping 3D cutscenes (60 FPS) / Proteção completa de fade ao pular cutscenes 3D rápido (60 FPS)

## English

- **Complete fix for the 3D cutscene fast-skip visual glitch at 60 FPS**:
  - Native 60 FPS smooth-animation replacement (`cv_smooth_animation.h`) now checks the engine's 2D screen fader directly (`fade_an` at `system+0x2A5D8 > 0.5f`) and applies `!skippable_cutscene` across all scripted camera modes (`0x44`, `0x46`, `0x204`, `0x2004`).
  - Even when skipping an in-engine 3D cutscene within the very first milliseconds, the black 2D fade always stays intact over character model swaps so the character never appears frozen without hands before control returns. Applies to all characters across **EU, US and JP**.
- Includes everything from **v2.2.2**, **v2.2.1**, and **v2.2.0** (F2 Return to Title dialog improvements, CGI cutscene heap & fast-skip stability fixes, expanded startup support log, One-hit kill cheat, Original Saves tab, WASD menu navigation, 60 FPS & elevator refinements).

Downloads: extract the complete **CodeVeronica-PC-v2.2.3.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Correção definitiva do problema visual ao pular cutscenes 3D muito rápido em 60 FPS**:
  - A interpolação nativa de 60 FPS (`cv_smooth_animation.h`) agora verifica diretamente o alpha do fade 2D da tela (`fade_an` em `system+0x2A5D8 > 0.5f`) e aplica `!skippable_cutscene` em todos os modos de câmera de script (`0x44`, `0x46`, `0x204`, `0x2004`).
  - Mesmo pulando uma cutscene 3D nos primeiros milissegundos, o fade preto cobre a troca de modelo por completo, sem nunca mostrar o personagem travado sem as mãos antes de devolver o controle. Vale para todos os personagens em **EU, US e JP**.
- Inclui tudo da **v2.2.2**, **v2.2.1** e **v2.2.0** (melhorias na caixa `F2` Voltar para a tela de título, estabilidade de memória e skip rápido em cutscenes CGI, log de suporte aprimorado, cheat Matar com um golpe, aba Saves Originais, navegação WASD nos menus, refinamentos de 60 FPS e elevadores).

Downloads: extraia o **CodeVeronica-PC-v2.2.3.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
