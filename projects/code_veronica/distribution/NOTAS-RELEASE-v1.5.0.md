# v1.5.0 — Smoother 60 FPS across EU/US/JP, refined inventory and more game-text languages

## English

### New and improved

- **Native 60 FPS smoothing for the European, American and Japanese editions.** Selecting **Graphics → Frame limit → 60 FPS** now automatically enables the integrated animation improvements. There is no separate EU-only smoothing switch. Cameras, supported character/attachment/enemy/object animations, item inspection and real-time scenes benefit while game logic keeps its original 30 Hz timing. **30 FPS remains the default** on a new installation.
- **Smoother inventory transitions**, including opening/closing panels, tab changes, item inspection and files. Additional fixes keep backgrounds and text associated with the correct drawing elements. **Equipping and unequipping now create a clean transition**, avoiding the brief blue-panel corruption reported during local testing.
- **FSR 3.1 Frame Generation can run alongside native 60 FPS smoothing**, with output up to **120 displayed FPS** when performance and the scene allow it. It may suspend in menus or transitions. VSync remains a separate option for 60 FPS without Frame Generation.
- **Game text and subtitle selection:** EU offers Original, Portuguese (Brazil), English, French, German, Italian and Spanish. US offers the same choices; its additional French/German/Italian/Spanish resources are adapted from **your European installation registered in the launcher**. Original American scripts, model data and internal resource names are preserved; conversion uses a local cache. The first conversion of an archive may add some loading time. **JP remains Japanese only**, with the game-text selector disabled for that edition. The launcher's 13 interface languages are separate from the game's available languages.
- **Fixed European door-animation skipping**, bringing the launcher option back into effect. The patch is verified for EU/US/JP.
- **Recomp Settings is visible again** with the new rendering path. Appearance, movement style, combat HUD and manual reload are grouped under **Gameplay**. The **3D appearance preview is lighter** and only appears while selecting an appearance; this does not lower the quality of gameplay models.
- **Depth of field stays optional**, with an explanation below the graphics settings. Its original enabled state is the default. Updated 60 FPS information replaces the old slowdown warning.
- **Confirmation/input improvements**, including retained short button presses and optional confirmation while holding Run or at the title screen; reduced polling of disconnected controllers. Optional startup save/PSN notice skipping and launcher hiding are available. Original error dialogs and explicit leaderboard requests are retained.
- **Play-time correction** for the native smoothing path, based on elapsed engine time rather than presentation-frame counts. Existing save totals are not rewritten.
- **Refined HUD ammunition readout** showing loaded rounds and compatible carried reserve when enabled.

### Existing features retained

- 4K rendering and adjustable graphics; optional modern movement in the style of **Resident Evil HD Remaster** and **Resident Evil 0 HD Remaster**. **Classic remains the initial default.**
- Keyboard and multiple controller types, remappable buttons and both analog sticks. Keyboard defaults: WASD for the left stick; arrow keys for the D-pad. Existing mappings are preserved.
- Optional combat HUD with health/condition, weapon and ammunition; manual reload by aiming and pressing your assigned command; in-game equipment shortcuts with editable slots. These features remain individually optional and disabled by default on a fresh installation.
- SRT gameplay/statistics overlay, local trophies and notifications, saves/captures, patches, mods and texture-pack support, Discord status, and DXC/FXC shader compiler selection (**DXC by default**).
- All three editions include normal **AVX2** and **SSE4.1 compatibility** executables. Open **Code Veronica.exe**; processor selection is automatic. This release retains the fixed-camera view.

### Regional support

| Feature | EU | US | JP |
|---|---|---|---|
| Integrated 60 FPS scene/inventory smoothing | ✓ | ✓ | ✓ |
| FSR 3.1 Frame Generation option | ✓ | ✓ | ✓ |
| HUD, manual reload, editable equipment shortcuts | ✓ | ✓ | ✓ |
| Classic/Modern controls and remappable sticks | ✓ | ✓ | ✓ |
| Recomp Settings and lighter appearance preview | ✓ | ✓ | ✓ |
| Door skip and graphics/shader options | ✓ | ✓ | ✓ |
| AVX2 + automatic SSE4.1 compatibility | ✓ | ✓ | ✓ |
| Game text/subtitles | EN/FR/DE/IT/ES/PT-BR | EN/PT-BR + EU-resource languages* | Japanese only |

*Additional US languages require your registered European installation. Frame Generation depends on compatible hardware and scene support.*

Validation includes automated EU/US/JP checks, real GPU Frame Generation tests, original-file language/preview checks and compatibility startup under Westmere/Sandy Bridge CPU emulation. A full playthrough on real non-AVX2 hardware has not been verified for this update.

### Credits

**Special thanks to [Lyxodius](https://github.com/Lyxodius) for [PR #36](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/pull/36).** Their contribution provided the foundation for native 60 FPS animation improvements, play-time correction, original-language selection and several input/usability enhancements. **Kastiel reviewed and extended that work** for this release, including the US/JP adaptation, compatibility with FSR Frame Generation, additional inventory smoothing and rendering fixes, expanded game-text choices and menu/preview refinements. Thank you for helping make the port smoother and more comfortable to play.

### Downloads and compatibility

- **CodeVeronica-PC-v1.5.0.zip** — complete Windows distribution. Extract everything and open **Code Veronica.exe**, keeping `bin/` and `assets/` alongside it. Use the full package for a consistent update; keep backups of your personal saves/settings before replacing an older installation.
- **Veronica-PKG-Extractor-v1.0.1.zip** — PKG preparation tool, supplied in its own ZIP, unchanged from the previous release.
- Optional community HD texture pack remains a separate download; see the project's texture-pack credits.

Requires your own original PS3 game, DirectX 12 graphics support and a compatible processor (SSE4.1 minimum). No original game archives, 3D models, saves, licenses or personal settings are bundled. EU/US/JP AVX2 and compatibility builds are reviewed separately; automated checks and local gameplay testing do not constitute a complete playthrough on every PC. Please report issues with the game region, PC specifications and `run-log.txt`.

## Português

### Novidades e melhorias

- **Suavização nativa a 60 FPS nas versões europeia, americana e japonesa.** Selecionar **Gráficos → Limite de quadros → 60 FPS** ativa automaticamente as melhorias integradas de animação. O interruptor separado exclusivo da europeia foi removido. Câmeras, animações compatíveis de personagens/acessórios/inimigos/objetos, inspeção de itens e cenas em tempo real recebem a melhoria, mantendo a lógica original do jogo a 30 Hz. **30 FPS continua sendo o padrão** em uma instalação nova.
- **Inventário mais fluido**, incluindo abertura/fechamento dos painéis, troca de abas, inspeção de itens e arquivos. Correções adicionais mantêm fundos e textos associados aos elementos corretos do desenho. **Equipar e desequipar agora têm uma transição protegida**, evitando a falha breve no painel azul relatada durante os testes locais.
- **Geração de quadros FSR 3.1 compatível com a suavização nativa a 60 FPS**, podendo chegar a **120 FPS apresentados**, conforme o desempenho e a cena. A geração pode ser suspensa em menus e transições. VSync continua como opção separada para 60 FPS sem geração de quadros.
- **Escolha dos textos e legendas do jogo:** a europeia oferece Original, Português (Brasil), inglês, francês, alemão, italiano e espanhol. A americana oferece as mesmas escolhas; os recursos adicionais de francês/alemão/italiano/espanhol são adaptados a partir da **sua instalação europeia cadastrada no launcher**. Scripts, dados de modelos e nomes internos da edição americana são preservados; a conversão usa um cache local. A primeira conversão de um arquivo pode acrescentar um tempo ao carregamento. **A japonesa permanece somente em japonês**, com o seletor de textos desativado nessa edição. Os 13 idiomas da interface do launcher são independentes dos idiomas disponíveis nos textos do jogo.
- **Corrigida a opção de pular a animação das portas na europeia**, que volta a aplicar a escolha do launcher. O patch foi verificado para EU/US/JP.
- **Configurações do Recomp voltam a aparecer** no novo caminho de renderização. Aparência, estilo de movimento, HUD de combate e recarga manual ficam em **Jogabilidade**. A **prévia 3D de aparência está mais leve** e aparece ao selecionar uma aparência; a qualidade dos modelos durante a gameplay não foi reduzida.
- **Profundidade de campo continua opcional**, com uma explicação abaixo das configurações gráficas. O efeito original ligado é o padrão. A informação de 60 FPS foi atualizada, removendo o aviso antigo de lentidão.
- **Melhorias de confirmação e comandos**, incluindo preservação de toques curtos e confirmação opcional enquanto segura Correr ou na tela inicial; controles desconectados são consultados com menos frequência. Há opções para pular os avisos iniciais de salvamento/PSN e ocultar o launcher ao jogar. Diálogos de erro e consultas explícitas ao placar são preservados.
- **Correção do tempo de jogo** no caminho de suavização nativa, usando o tempo decorrido do motor em vez da quantidade de quadros apresentados. Os totais já salvos não são reescritos.
- **Leitura de munição do HUD aprimorada**, mostrando balas carregadas e a reserva compatível no inventário quando ativada.

### Recursos anteriores mantidos

- Renderização em 4K e gráficos ajustáveis; movimento moderno opcional no estilo de **Resident Evil HD Remaster** e **Resident Evil 0 HD Remaster**. **Clássico continua sendo o padrão inicial.**
- Teclado e vários tipos de controle, botões e os dois analógicos configuráveis. Padrão do teclado: WASD para o analógico esquerdo; setas para o D-pad. Mapeamentos existentes são preservados.
- HUD de combate opcional com vida/condição, arma e munição; recarga manual mirando e apertando o comando escolhido; atalhos de equipamento com slots editáveis durante a partida. Continuam sendo opções independentes, desligadas por padrão numa instalação nova.
- SRT com dados/estatísticas da partida, troféus locais e notificações, saves/capturas, patches, mods e suporte a pacotes de texturas, status no Discord e seleção DXC/FXC (**DXC como padrão**).
- As três edições incluem executáveis **AVX2** e de **compatibilidade SSE4.1**. Abra **Code Veronica.exe**; a escolha conforme o processador é automática. Esta versão mantém a câmera fixa.

### Suporte por região

| Recurso | EU | US | JP |
|---|---|---|---|
| Suavização integrada de cenas/inventário a 60 FPS | ✓ | ✓ | ✓ |
| Opção de geração de quadros FSR 3.1 | ✓ | ✓ | ✓ |
| HUD, recarga manual, atalhos de equipamento editáveis | ✓ | ✓ | ✓ |
| Controles Clássico/Moderno e analógicos configuráveis | ✓ | ✓ | ✓ |
| Configurações do Recomp e prévia de aparência leve | ✓ | ✓ | ✓ |
| Pular portas e opções gráficas/de shaders | ✓ | ✓ | ✓ |
| AVX2 + compatibilidade SSE4.1 automática | ✓ | ✓ | ✓ |
| Textos/legendas do jogo | EN/FR/DE/IT/ES/PT-BR | EN/PT-BR + idiomas com recursos EU* | Somente japonês |

*Os idiomas adicionais da americana usam sua instalação europeia cadastrada. A geração de quadros depende do hardware compatível e da cena.*

A validação inclui testes automáticos EU/US/JP, geração de quadros real na GPU, testes de idioma/prévia com arquivos originais e inicialização de compatibilidade em emulação de CPUs Westmere/Sandy Bridge. Uma campanha completa em hardware real sem AVX2 não foi verificada nesta atualização.

### Créditos

**Agradecimento especial a [Lyxodius](https://github.com/Lyxodius) pela [PR #36](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/pull/36).** Sua contribuição trouxe a base das melhorias de animação nativa a 60 FPS, correção do tempo de jogo, escolha de idioma original e diversos ajustes de comandos/usabilidade. **Kastiel revisou e ampliou esse trabalho** para esta versão, incluindo adaptação para US/JP, compatibilidade com a geração de quadros FSR, suavização adicional e correções do inventário, mais opções de textos e ajustes de menus/prévia. Obrigado por ajudar a deixar o port mais fluido e agradável de jogar.

### Downloads e compatibilidade

- **CodeVeronica-PC-v1.5.0.zip:** distribuição completa para Windows. Extraia tudo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. Use o pacote completo para atualizar de forma consistente; faça backup dos seus saves/configurações pessoais antes de substituir uma instalação antiga.
- **Veronica-PKG-Extractor-v1.0.1.zip:** ferramenta para preparar o PKG, em ZIP separado, sem alterações em relação à versão anterior.
- O pacote opcional de texturas HD da comunidade continua sendo um download separado; veja os créditos no projeto.

É necessário fornecer sua própria cópia original do jogo de PS3, placa com DirectX 12 e processador compatível (SSE4.1 como mínimo). Não inclui arquivos originais do jogo, modelos 3D, saves, licenças nem configurações pessoais. Builds EU/US/JP AVX2 e de compatibilidade são revisadas separadamente; testes automáticos e gameplay local não representam uma campanha completa em todos os PCs. Relate problemas com a região do jogo, as especificações do PC e o `run-log.txt`.
