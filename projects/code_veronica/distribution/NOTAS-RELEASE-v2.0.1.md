# v2.0.1 — Disc 1 → Disc 2 Swap Fix for Fast / Supreme Door Skip / Correção na Troca de Disco (Parte 1 → Parte 2) com Fast / Supreme Door Skip

## English

- **Fixed Disc 1 → Disc 2 swap hang with Fast / Supreme Door Skip.** When reaching the *"Save the end of the first part? Yes / No"* prompt after passing through doors with **Fast** or **Supreme Door Skip** enabled, the door-acceleration state could remain active across the prompt and stall the Disc 2 transition on a dark screen. Door-skip acceleration now detects the Disc 1 ↔ Disc 2 swap state (`is_disc_swap`, plus a 5-second safety timeout on active door state) and steps aside cleanly so the Disc 2 transition always proceeds normally, while keeping all normal doors just as fast.
- Includes everything from **v2.0.0** (40 native x64 engine hooks, Fast / Supreme Door Skip, RPCS3 save support, 60 FPS Game Over & Inventory/Item Box fixes, Combat HUD & SRT Poison status fix, and 6,182 precompiled shaders). Applies to **EU, US and JP**.

Downloads: extract the complete **CodeVeronica-PC-v2.0.1.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- **Corrigido travamento na troca de disco (Parte 1 → Parte 2) com Fast / Supreme Door Skip.** Ao chegar na tela *"Salvar o fim da primeira parte? Sim / Não"* após passar por portas com o **Door Skip Rápido** ou **Supremo** ativado, o estado de aceleração da porta podia continuar ativo durante o aviso e prender a transição para o Disco 2 em uma tela escura. Agora o acelerador detecta exatamente a troca de disco (`is_disc_swap`, além de um timeout de segurança de 5s) e sai de cena apenas durante a troca de disco, mantendo todas as portas normais igualmente rápidas.
- Inclui todas as novidades da **v2.0.0** (40 hooks nativos x64, Fast / Supreme Door Skip, suporte a saves do RPCS3, correções de Game Over e Inventário/Item Box em 60 FPS, status Envenenado no HUD de Combate e SRT, e 6.182 shaders pré-compilados). Vale para **EU, US e JP**.

Downloads: extraia o **CodeVeronica-PC-v2.0.1.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
