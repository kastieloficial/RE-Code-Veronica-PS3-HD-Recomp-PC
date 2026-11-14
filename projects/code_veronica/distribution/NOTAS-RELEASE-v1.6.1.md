# v1.6.1 — Less work per frame at 60 FPS / Menos trabalho por quadro em 60 FPS

## English

- **Lighter rendering at native 60 FPS.** In busy rooms (such as the Antarctic base rm_4000, with more than 8,000 draws a frame) the renderer looked up each draw's shader twice. It now looks it up once and reuses the answer, checking each reuse against the live shader and falling back to the old path if anything differs. In our measurements the lookups per draw went from 1.99 to 0.99 and the renderer's recording time dropped by about a third; the frame rate in that room rose by about 5%. The picture is unchanged. Turn it off with `CV_PREP_REUSE=0` if you ever see a problem.
- **Cheaper on-screen interface drawing.** The HUD, SRT and counters now compute each rectangle's colour once instead of once per corner. The result is bit-identical; the gain is small.
- **More shaders compiled before play:** 5,700 combinations (was 5,696).
- Everything from v1.6.0 is retained: the Battle Game clock at 60 FPS, self-destruct countdowns at the right speed, the SRT boss bar fix, audio following the Windows output, the 60 FPS menu input fix, the cutscene bars option, the revised Portuguese translation and the hang log. Applies to EU, US and JP.

Downloads: extract the complete **CodeVeronica-PC-v1.6.1.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Renderização mais leve em 60 FPS nativo.** Em salas pesadas (como a base da Antártida, rm_4000, com mais de 8.000 desenhos por quadro), o renderizador procurava o shader de cada desenho duas vezes. Agora procura uma vez e reaproveita a resposta, conferindo cada reaproveitamento com o shader real e voltando ao caminho antigo se algo for diferente. Nas nossas medições, as buscas por desenho caíram de 1,99 para 0,99 e o tempo de gravação do renderizador caiu cerca de um terço; o FPS nessa sala subiu uns 5%. A imagem não muda. Se algum dia aparecer problema, desligue com `CV_PREP_REUSE=0`.
- **Desenho da interface mais barato.** O HUD, o SRT e os contadores agora calculam a cor de cada retângulo uma vez, e não uma por canto. O resultado é idêntico bit a bit; o ganho é pequeno.
- **Mais shaders compilados antes de jogar:** 5.700 combinações (eram 5.696).
- Tudo da v1.6.0 continua: o cronômetro do Battle Game em 60 FPS, as contagens de autodestruição na velocidade certa, a correção da barra do chefe no SRT, o áudio que segue a saída do Windows, a correção dos menus em 60 FPS, a opção sem faixas nas cutscenes, a tradução revisada e o log de travamento. Vale para EU, US e JP.

Downloads: extraia o **CodeVeronica-PC-v1.6.1.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
