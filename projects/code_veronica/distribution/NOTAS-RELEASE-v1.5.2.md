# v1.5.2 — One executable per region, puzzle choices, compact launcher / Um executável por região, escolhas em puzzles, launcher compacto

## English

- **One executable per region for every processor.** The separate `_SSE4.exe` compatibility executables are gone: each regional executable now carries the SPU code twice (AVX2 and SSE4.1) and picks the right one at start. Processors with AVX2 keep the AVX2 code; processors without it (Sandy/Ivy Bridge, AMD FX, first-generation Core i) run the SSE4.1 copy in the same executable. The package drops from six executables to three.
- **Modern controls: puzzle and text choices.** With modern controls, choice boxes such as "Push which button?" and the elevator floors only moved in one direction. The stick and d-pad now reach the game unchanged while a room text or choice box is open ([issue #42](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/issues/42)).
- **Compact launcher.** Options take one row instead of a tall card on the HUD, SRT, Cheats, Performance, Mods and Advanced pages; the SRT page uses three columns. Settings and their behaviour are unchanged.
- Applies to EU, US and JP. All v1.5.1 features, settings and the native 60 FPS animation package are retained.

Pushing objects in modern controls still works with the d-pad, which does not run; the analog stick runs, as in the original game.

Validation: the three regional executables were built with both SPU copies. On an AVX2 processor the normal path ran with correct gameplay and audio, and the SSE4.1 path (forced with `CV_CPU_FORCE_COMPAT=1`) started and ran the game's SPU work. The puzzle choice fix was confirmed in game with modern controls. The SSE4.1 path has not yet been tested on a processor without AVX2; very old processors may still run below full speed.

Downloads: extract the complete **CodeVeronica-PC-v1.5.2.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. Old `_SSE4.exe` files left from a previous version are no longer used and can be deleted. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Um executável por região para qualquer processador.** Os executáveis de compatibilidade `_SSE4.exe` saíram: cada executável regional agora traz o código SPU em duas versões (AVX2 e SSE4.1) e escolhe a certa ao abrir. Processadores com AVX2 continuam usando AVX2; os sem AVX2 (Sandy/Ivy Bridge, AMD FX, Core i de primeira geração) usam a cópia SSE4.1 no mesmo executável. O pacote passa de seis executáveis para três.
- **Controle moderno: escolhas em puzzles e textos.** Com o controle moderno, caixas de escolha como "Push which button?" e os andares do elevador só andavam para um lado. Agora o analógico e o direcional chegam ao jogo sem conversão enquanto uma caixa de texto ou de escolha está aberta ([issue #42](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/issues/42)).
- **Launcher compacto.** As opções ocupam uma linha em vez de um cartão alto nas páginas HUD, SRT, Cheats, Desempenho, Mods e Avançado; a página SRT usa três colunas. As configurações e o comportamento delas não mudaram.
- Vale para EU, US e JP. Todos os recursos e configurações da v1.5.1 e o pacote de animações nativas a 60 FPS são mantidos.

Empurrar objetos no controle moderno continua funcionando com o direcional, que não corre; o analógico corre, como no jogo original.

Validação: os três executáveis regionais foram compilados com as duas cópias do código SPU. Num processador com AVX2, o caminho normal rodou com jogabilidade e áudio corretos, e o caminho SSE4.1 (forçado com `CV_CPU_FORCE_COMPAT=1`) abriu e rodou as tarefas SPU do jogo. A correção das escolhas em puzzles foi confirmada no jogo com o controle moderno. O caminho SSE4.1 ainda não foi testado num processador sem AVX2; processadores muito antigos podem continuar abaixo da velocidade normal.

Downloads: extraia o **CodeVeronica-PC-v1.5.2.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. Arquivos `_SSE4.exe` que sobraram de uma versão anterior não são mais usados e podem ser apagados. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
