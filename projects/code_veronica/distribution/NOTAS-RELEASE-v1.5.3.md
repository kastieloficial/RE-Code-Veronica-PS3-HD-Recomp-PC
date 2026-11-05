# v1.5.3 — Sniper Rifle and Linear Launcher scope with native 60 FPS / Mira da sniper e da linear launcher no 60 FPS nativo

## English

- **Scope overlay restored with the native 60 FPS animation.** Aiming with the Sniper Rifle or the Linear Launcher showed the zoomed view without the scope and crosshair when the native 60 FPS animation was on (30 FPS was fine). The extra frame drawn between game updates rebuilds only the 3D scene, not the scope overlay. While the scope is up, the game now presents its original frames, which include the scope; interpolation resumes as soon as you lower the weapon.
- Applies to EU, US and JP. Everything from v1.5.2 is retained: one executable per region with AVX2 and SSE4.1 code, the puzzle choice fix for modern controls and the compact launcher.

Validation: the scope fix was confirmed in game (EU) with the Sniper Rifle and the Linear Launcher, with native 60 FPS animation on. The scope view itself runs at the game's original animation rate.

Downloads: extract the complete **CodeVeronica-PC-v1.5.3.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Mira restaurada com a animação nativa 60 FPS.** Ao mirar com a sniper ou com a linear launcher, a visão com zoom aparecia sem a luneta e sem a cruz quando a animação nativa 60 FPS estava ligada (em 30 FPS funcionava). A imagem extra desenhada entre as atualizações do jogo refaz só a cena 3D, não a camada da luneta. Enquanto a luneta está aberta, o jogo agora mostra as imagens originais, que trazem a mira; a interpolação volta assim que você abaixa a arma.
- Vale para EU, US e JP. Tudo da v1.5.2 continua: um executável por região com código AVX2 e SSE4.1, a correção das escolhas em puzzles no controle moderno e o launcher compacto.

Validação: a correção foi confirmada no jogo (EU) com a sniper e com a linear launcher, com a animação nativa 60 FPS ligada. A visão da luneta em si roda na taxa de animação original do jogo.

Downloads: extraia o **CodeVeronica-PC-v1.5.3.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
