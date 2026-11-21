# Plano de melhorias — próxima versão (depois da v1.6.1)

Aprovado pelo Gabriel em 2026-11-21. Base: tag `v1.6.1` (commit 5d5e901).
Prévias finais em `docs/previas-v1.7/`. Os arquivos de trabalho das prévias
(HTML, fontes, ícones HD recortados) ficam fora do git em
`docs/mockup-voltar-titulo/` e `docs/mockup-inventario/`.

## Estilo visual (vale para todos os menus do port)

- Moldura dourada chanfrada (gradiente #F6E7A8 → #C9A13E → #6E5014, linha interna #20180A), fundo #0A0F1E, painéis azuis #10285E → #081433, borda #3A5FA8.
- Títulos em **Cinzel 700** com gradiente dourado; textos em **Barlow Semi Condensed** 400–700. As duas fontes são SIL OFL: embutir os TTF no pacote (AddFontMemResourceEx/PrivateFontCollection). JA/ZH/KO/AR/RU continuam com `cv_srt_i18n::font_face`.
- Aba/botão ativo: placa dourada com texto e ícone escuros #1A1206. Inativo: aço escuro #1C2232 → #0D111C, borda dourada, texto claro.
- Rodapé com grupos "ícone + ação" separados por losangos dourados, usando os ícones de botão que já existem (`key_glyph`, `btn_glyph`, keycaps de `cv_keycap_style.h`). Troca sozinho entre teclado / PlayStation / Xbox pelo último dispositivo usado (`hint_kind()` / `note_input()` da biblioteca de saves).
- Todos os textos nos 13 idiomas do launcher.

## 1. Voltar para a tela de título

- **Abrir:** F2, ou L1+L2+R1+R2+Start+Select no controle (borda de subida, só com os ombros já segurados; bloquear Start/Select para o jogo enquanto o combo estiver montado).
- **Caixa:** ~900x440, pergunta "Voltar para a tela de título?" em Cinzel dourado, aviso âmbar "O progresso não salvo será perdido.", botões **Sim à esquerda, Não à direita**, **Não vem selecionado**. Prévias: `titulo-pt-ps.png`, `titulo-en-kb.png`.
- **Entradas:** setas/D-pad trocam; Enter/✕ confirma; Esc/○ ou F2 de novo = Não.
- **Sim:** fade para preto e pedido na thread do jogo, no próximo ponto seguro. Mesma limpeza que o Game Over faz (`func_001B0AC8`, EU) e depois tarefas `system+0x64 = 0x300048` (menu principal). Parar o banco de som como a biblioteca de saves faz.
- **Bloqueado** (salvando, trocando de sala, vídeo/cena, Game Over, título): não abre e mostra aviso no canto: "Voltar ao título não está disponível ao salvar, carregar ou em cenas."
- Testar: voltar de sala normal, com inimigo, inventário aberto, contagem de autodestruição e Battle Game; depois Novo Jogo e Load sem som preso nem tela preta; LiveSplit do Kapdap reinicia. EU primeiro, conferir endereços US/JP.

## 2. Notificação de conquista

- Cartão 640x112 no canto (margem 32), fundo rgba(21,17,17,.94), barra vermelha de 5px à esquerda, linha fina que encolhe mostrando o tempo restante.
- **Ícone:** a imagem original de cada troféu (TROP%03d.PNG), **quadrada com cantos levemente arredondados** (raio ≈12% do lado), imagem inteira sem corte, borda fina na cor do troféu (bronze/prata/ouro/platina). O redondo foi recusado.
- Texto: "CONQUISTA DESBLOQUEADA · BRONZE" (Barlow SC 15, grau na cor do troféu), nome em Cinzel 27, descrição em Barlow 19.
- Desenhar já na escala da tela (não esticar bitmap de 1080p). Arquivo: `cv_trophy_popup.cpp`.

## 3. Arquivos (F7)

- **Nova ordem das abas: Carregar · Salvar · Mapas.** Abre em Carregar.
- **Funcionar a partir da tela de título:** Carregar usa o mesmo caminho do *Load Game* do jogo; Mapas inicia um novo jogo por trás, pula a abertura e viaja assim que houver controle (se pular falhar, espera a cena terminar). Salvar continua bloqueado no título.
- Explicação embaixo de "Entrada": "Entrada é o ponto da sala onde o personagem aparece (cada porta tem um número). Se a sala não carregar direito, tente outro número."
- Prévias: `arquivos-carregar.png`, `arquivos-salvar.png`, `arquivos-salvar-confirmar.png`, `arquivos-mapas.png`.

## 4. Inventário (F6)

- **Janela** 1400x820 centralizada (não mais quase tela cheia). Catálogo 4x4 à esquerda, coluna "Inventário atual" de **400px** à direita, botão principal 400x42.
- **Ícones HD fixos no pacote** (`assets/`), recortados das páginas HD do pacote de texturas de fã (`80825AC3566791AE` página 0, `0970338B235A8658` página 1; células 272x192). Ícone original do jogo só como reserva. Decisão do Gabriel: pode ir no pacote.
- **Armas de 2 espaços** (confirmado em `func_001305D4`): 0x01 Lança-foguetes, 0x02 Fuzil de assalto, 0x03 Rifle de precisão, 0x21 Lugers douradas, 0x22 Submetralhadoras, 0x8E M1P. Ocupam **uma entrada e dois espaços**. Contar espaços, não entradas, em `place()`/`change()`/`first_empty()` (`cv_items.cpp`); recusar com "Não há 2 espaços livres lado a lado."; cartão largo na lista; selo "2 espaços" no catálogo.
- **Itens removidos do menu:** todo item sem ícone real nas páginas do jogo (células vazias ou com número de teste como 038/109/110/141, que o jogo também mostra bugado): documentos, mapas, Fotografia B, Relevo da rainha (completo), Controle remoto, Chaves de brasão etc. Também saem **Bolsa lateral** e **Arquivo**. Em "Arquivos e outros" ficam só: Manual de jogo, Fita de tinta, Arquivo de segurança e as 3 maletas.
- Nomes das armas = os da tradução (Pistola, Pistola aprimorada, Pistola personalizada, Glock 17, Lugers douradas, Espingarda, Magnum, Lança-granadas, Besta, Rifle de precisão, Fuzil de assalto, Submetralhadoras, M1P, Lança-foguetes, Canhão linear, Faca de combate).
- Ícone da aba Armas: pistola estilo 1911 em silhueta.
- Prévias: `inventario-a.png` (PT, teclado), `inventario-b.png` (PT, PlayStation, recusa de 2 espaços), `inventario-c.png` (EN, Xbox), `inventario-d.png`.

## 5. Correções de bugs dos menus F6/F7

- **Menu preso no meio / jogo aceitando controle com menu aberto:** um dono único para o estado modal (`cv_port_modal`: acquire/release por bits); pausa = "alguma janela aberta"; `g_pad_block` calculado num lugar só. F6 e F7 se fecham mutuamente. Só abrir com o jogo em primeiro plano; se `SetForegroundWindow` falhar, desfazer. Recuperar o foco no timer. Ignorar repetição de tecla (bit 30) e toggles em menos de 250 ms. Fechar sempre pelo mesmo caminho completo.
- **Menus na frente de outros programas:** janela dona = janela do jogo (`rsx_d3d12_window()`), sem `WS_EX_TOPMOST`/`HWND_TOPMOST`; reafirmar o dono a cada abertura.
- **Xbox LB/RB trocados no F6:** aplicar a troca de `menu_pad` (layout 360).

## Pendente (não faz parte deste pacote ainda)

- Travadas na cena em que o Steve atira com a metralhadora: capturar com `capturar-shaders-live.cmd` e analisar `perf-log.txt`, `render-profile.txt` e os shaders novos.
