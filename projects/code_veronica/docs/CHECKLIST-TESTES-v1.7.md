# Checklist de testes no jogo — v1.7.0 (interno)

Tudo abaixo passou apenas em testes automáticos (regras, desenho e memória sintética) ou não foi exercitado no jogo de verdade. **Retorno ao título, Carregar pelo título e Mapas pelo título só foram testados por testes automáticos, ainda não no jogo.** Faça EU primeiro; US e JP depois. Anote o que falhar com o nome da região e a sala.

Como rodar: `projects\code_veronica\build-release\bin\Code Veronica.exe` (launcher do build de teste). Para o fluxo do título com sonda: abrir com `CV_TITLE_PROBE=1` definido no ambiente (por exemplo `set CV_TITLE_PROBE=1` e depois o launcher); o log mostra os campos do estado do jogo a cada mudança.

## 1. Voltar ao título (F2 e L1+L2+R1+R2+Start+Select)

- [ ] Sala normal: F2 abre a caixa, Não vem selecionado, setas/D-pad trocam, Esc/○/F2 de novo fecham; Sim volta ao título com fade, sem som preso nem tela preta.
- [ ] Mesma coisa com o combo do controle (PlayStation e Xbox; Start e Select ficam escondidos do jogo enquanto o combo está montado, sem abrir o menu de pausa do jogo).
- [ ] Com inimigo na tela; com o inventário do jogo aberto (se ficar interface sobrando, anotar); durante contagem de autodestruição; no Battle Game.
- [ ] Depois de voltar: Novo Jogo e Carregar funcionam normalmente; o LiveSplit do Kapdap reinicia.
- [ ] Bloqueios (aviso no canto, sem caixa): salvando, carregando, trocando de sala (porta), cena/vídeo, Game Over, tela de título.
- [ ] Linha F2 na página Atalhos do launcher, nos 13 idiomas (texto cabe, duas linhas de botões legíveis).
- [ ] US e JP: os endereços da tarefa de Game Over (US func_001AF058, JP func_001AF100) não foram verificados no jogo.

## 2. Arquivos F7 (Carregar / Salvar / Mapas)

- [ ] Abre em Carregar; ordem das abas Carregar, Salvar, Mapas; L1/R1 e Q/E trocam.
- [ ] Dentro do jogo: carregar um save, salvar com confirmação, viajar por Mapas (porta/Entrada diferentes); a dica de Entrada aparece sob o seletor.
- [ ] **Do título** (usar `CV_TITLE_PROBE=1`): capturar um Novo Jogo e um Carregar naturais para confirmar o valor de retorno do menu e se a dificuldade é definida antes; depois:
  - [ ] Carregar pelo título: carrega o save, sem som preso; ao voltar ao título de novo, tudo normal.
  - [ ] Mapas pelo título: começa um novo jogo, a sala escolhida carrega assim que há controle (a abertura não é pulada); aviso "Iniciando um novo jogo…"; aborta com aviso se voltar ao menu.
  - [ ] Salvar no título aparece bloqueado com a mensagem própria.
- [ ] Caixa de busca (nomes de saves/mapas): fonte Barlow aparece, digitação e acentos ok.
- [ ] Mouse: passar e pressionar mostram destaque; fechar no X; o clique só vale ao soltar dentro do mesmo controle.

## 3. Inventário F6

- [ ] Ícones HD aparecem; item sem HD cai no ícone original; nenhum item sem ícone, sem Bolsa lateral nem Arquivo.
- [ ] **Armas de 2 espaços:** com 1 espaço livre, o menu recusa lança-foguetes/fuzil/rifle/Lugers/submetralhadoras/M1P com "Não há 2 espaços livres lado a lado."; trocar item normal por arma larga só quando couber.
- [ ] **Disposição de 2 espaços contra a tela de inventário do próprio jogo** (comparar, com as mesmas armas, onde cada item cai; o layout do F6 é uma suposição ainda não conferida).
- [ ] "N de 8" (ou 10 com bolsa lateral) conta espaços, não entradas.
- [ ] Rótulos de munição/carga, nomes das armas (inclusive M1P) nos idiomas principais.
- [ ] Xbox: LB/RB trocam de aba na ordem certa; PlayStation e teclado idem; rodapé troca de ícones conforme o último dispositivo.
- [ ] Mouse: hover/pressed nos botões, abas, passos e cartões.

## 4. Correções de F6/F7 (janelas e entrada)

- [ ] Menus não ficam na frente de outro programa ao alternar com Alt+Tab; não roubam foco de outros apps.
- [ ] Segurar F6/F7 não reabre/pisca; F6 e F7 se fecham um ao outro; abrir só com o jogo em primeiro plano.
- [ ] Depois de fechar qualquer menu, o jogo volta a aceitar teclado e controle (sem entrada presa) e a pausa por perda de foco continua certa; abrir o launcher (F5) e fechar também.
- [ ] Sem travadas com os menus abertos (a janela filha de outra thread é o risco aqui; se houver engasgo, anotar).

## 5. Cartão de conquista

- [ ] Desbloquear (ou forçar) um troféu de cada tipo: bronze, prata, ouro, platina; cartão plano, ícone em quadrado arredondado com a borda da cor do tipo, linha de tempo encolhendo, som uma vez.
- [ ] Rótulo em inglês "ACHIEVEMENT UNLOCKED · BRONZE" (e os outros idiomas); nome longo corta com reticências.
- [ ] Nítido em 1080p e em 1440p/4K (desenhado na escala da tela) e na janela redimensionada.

## 6. Fontes

- [ ] Cinzel nos títulos e Barlow nos textos em PT/EN/ES/FR/DE/IT; caracteres especiais de PL e TR (ł, ś, ğ, ş, İ) aparecem corretos no Cinzel; JA/ZH/KO/AR/RU caem na fonte antiga sem quadradinhos.

## 7. US e JP

- [ ] Abrir F6, F7 e a caixa de voltar ao título nos dois; ícones HD, nomes de armas e 2 espaços (os ids largos valem para as três regiões?).
- [ ] Voltar ao título e Carregar/Mapas pelo título nos dois (endereços ainda sem confirmação em jogo).
- [ ] JP: textos nos idiomas, fonte do JA no menu.

## 8. Lista de shaders

- [ ] Cena da metralhadora do Steve (tiros): sem a travada na primeira vez que atira (cache de shaders limpo).
