# v2.2.2 — 3D cutscene fast-skip fade fix (60 FPS) / Correção do fade ao pular cutscenes 3D rápido (60 FPS)

## English

- **Fixed visual glitch when fast-skipping 3D cutscenes at 60 FPS**:
  - Previously, when skipping an in-engine 3D cutscene rapidly, the native 60 FPS smooth-animation pass could stay active during the skip fade-out (`camera_mode` bit `0x10000000`), drawing the 3D scene over the black 2D fade and briefly exposing the character frozen without hands before control returned.
  - Native 60 FPS scene replacement is now properly suspended during the skip fade across the entire game (Claire, Chris, Steve, Wesker; **EU, US and JP**).
- **Return to title screen dialog (`F2`)**: Swapped button order to **No** on the left (selected by default) and **Yes** on the right, matching left/right navigation.
- Includes everything from **v2.2.1** (CGI cutscene heap & fast-skip stability fixes, expanded startup support log) and **v2.2.0** (One-hit kill cheat, Original Saves tab, WASD menu navigation, 60 FPS & elevator refinements).

Downloads: extract the complete **CodeVeronica-PC-v2.2.2.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Corrigido problema visual ao pular cutscenes 3D rápido em 60 FPS**:
  - Antes, ao pular uma cutscene 3D rapidamente, o passe de 60 FPS suave podia permanecer ativo durante o fade preto do skip (bit `0x10000000` em `camera_mode`), desenhando a cena 3D por cima da tela preta e mostrando o personagem parado sem as mãos por ~1 segundo antes de liberar o controle.
  - O 60 FPS suave agora respeita o fade do skip em todo o jogo (Claire, Chris, Steve e Wesker; **EU, US e JP**), mantendo a transição limpa.
- **Caixa "Voltar para a tela de título?" (`F2`)**: Ordem dos botões invertida para **Não** à esquerda (selecionado por padrão) e **Sim** à direita, com navegação esquerda/direita acompanhando a posição.
- Inclui tudo da **v2.2.1** (estabilidade de memória e skip rápido em cutscenes CGI, log de suporte aprimorado) e da **v2.2.0** (cheat Matar com um golpe, aba Saves Originais, navegação WASD nos menus, refinamentos de 60 FPS e elevadores).

Downloads: extraia o **CodeVeronica-PC-v2.2.2.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
