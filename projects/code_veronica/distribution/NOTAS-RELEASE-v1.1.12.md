# v1.1.12 — Processors without AVX2, Discord status and the paid Japanese edition

## English

- **Processors without AVX2 now run the port.** Until now the port only opened on processors with AVX2; on older ones (for example a Core i3-2120) it showed a white window and closed with error `0xC000001D` (issue #12). The package now also carries a **compatibility version** (`CodeVeronicaPort_SSE4.exe`, `_US_SSE4.exe`, `_JP_SSE4.exe`), built for SSE4.1. You still open **Code Veronica.exe** as always: the port checks the processor when it starts and, when AVX2 is missing, opens the compatibility version by itself. There is nothing to choose.
  - Covered: Intel Core 2nd and 3rd generation (Sandy Bridge, Ivy Bridge), Core 1st generation (Nehalem, Westmere), older Xeons (X5650/X5670, E5 v1/v2), AMD FX. The minimum is **SSE4.1** (Intel since 2008); without it the port now explains this in a message instead of closing.
  - Speed depends on the processor. The game uses several cores at once in the heavy 3D cutscenes, so a 2-core processor can drop frames there.
  - **With AVX2 nothing changes:** the same executables, the same speed.
- **Discord status:** with the Discord app open on your PC, your profile shows **Playing RE Code Veronica X PS3 Recomp PC**, with the version (EU/US/JP), the play time and a button to the port's page. It can be turned off in **Advanced → Show on Discord**. Without Discord open, nothing happens.
- **Japanese version with a paid license (issue #32):** the Japanese release exists in two editions under the same NPJB00135: a free-licence one and a paid one whose EBOOT.BIN needs a `.rap`, like EU and US. The launcher assumed every Japanese copy was free, showed "Not needed" and failed with "Could not decrypt EBOOT.BIN". It now reads the license from the EBOOT.BIN: the paid edition shows the license field to pick your `.rap`; the free edition is unchanged.
- **Graphics card:** the port still needs a card with DirectX 12. The Intel HD 4600 (built into 4th-generation Core processors) freezes on the first frame; this is under investigation.
- **For people who build the port from the source code:** the repository now builds from any folder; one command, `tools/cv_make_spu.py`, generates the whole SPU folder (a regenerated folder used to end in a black screen); the FFmpeg DLLs are included; the README has `winget` commands to install the build tools; and the Portuguese text works in a build run from the repository. The same `cellPad.c` include fix was also made independently in the fork by **zedhqx4**.
- Previous improvements are retained, including the optional HD texture pack (separate download, link below).

These builds compiled successfully for EU/US/JP, normal and compatibility. All six executables (EU, US and JP, normal and compatibility) were started through the launcher and reached the game at 30 FPS, and a normal executable sent down the compatibility path started the `_SSE4` one by itself. The compatibility version was also run under Intel's SDE emulator: as a Sandy Bridge processor, the normal executable switched to `_SSE4` on its own; as a processor without AVX, `_SSE4` ran for 4 minutes with no invalid instruction. It has not yet been played on a real processor without AVX2, so reports are welcome (issue #12). The paid Japanese edition could not be tested here; the free one works as before. This remains a work in progress; a full playthrough on every setup is not verified.

### Downloads

- **CodeVeronica-PC-v1.1.12.zip** — main Windows port. Extract the complete ZIP and open **Code Veronica.exe**, keeping `bin/` and `assets/` beside it. SHA-256: `ce2106841fea272a42163f9f13bca2ac813c18363f04485d3dd5c7a199f09329`.
- **Veronica-PKG-Extractor-v1.0.1.zip** — PKG preparation tool, a **separate** ZIP (latest version, unchanged). SHA-256: `a88ddfea2b0a9079922c680abab9c6871e98f4e94f9ea252e6a928b8d44e1cc2`.
- **HD texture pack** — optional, [Google Drive](https://drive.google.com/file/d/1t6yVG1yEC5dl3Td6jT4rhFj1EEwmbsAZ/view?usp=sharing) (not attached to this release). Credits: [TEXTURAS-HD-COMUNIDADE.md](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/blob/code-veronica/projects/code_veronica/TEXTURAS-HD-COMUNIDADE.md).

No original game files, game 3D models, saves or personal settings are bundled. Supply your own original PS3 game.

## Português

- **Processadores sem AVX2 agora rodam o port.** Até aqui o port só abria em processadores com AVX2; nos mais antigos (por exemplo um Core i3-2120) aparecia uma janela branca e ele fechava com o erro `0xC000001D` (issue #12). O pacote agora traz também uma **versão de compatibilidade** (`CodeVeronicaPort_SSE4.exe`, `_US_SSE4.exe`, `_JP_SSE4.exe`), compilada para SSE4.1. Você continua abrindo o **Code Veronica.exe** de sempre: o port confere o processador ao iniciar e, quando falta AVX2, abre sozinho a versão de compatibilidade. Não é preciso escolher nada.
  - Cobertos: Intel Core de 2ª e 3ª geração (Sandy Bridge, Ivy Bridge), Core de 1ª geração (Nehalem, Westmere), Xeon antigos (X5650/X5670, E5 v1/v2), AMD FX. O mínimo é **SSE4.1** (Intel desde 2008); sem ele, o port agora explica isso numa mensagem em vez de fechar.
  - A velocidade depende do processador. O jogo usa vários núcleos ao mesmo tempo nas cutscenes 3D pesadas, então um processador de 2 núcleos pode ter quedas de FPS nelas.
  - **Com AVX2 nada muda:** os mesmos executáveis, a mesma velocidade.
- **Status no Discord:** com o Discord aberto no PC, o seu perfil mostra **Jogando RE Code Veronica X PS3 Recomp PC**, com a versão (EU/US/JP), o tempo de jogo e um botão para a página do port. Dá para desligar em **Avançado → Mostrar no Discord**. Sem o Discord aberto, nada acontece.
- **Versão japonesa com licença paga (issue #32):** a versão japonesa existe em duas edições com o mesmo NPJB00135: uma com licença grátis e uma paga, cujo EBOOT.BIN precisa de `.rap`, como a europeia e a americana. O launcher supunha que toda cópia japonesa era a grátis, mostrava "Não necessário" e falhava com "Não foi possível descriptografar o EBOOT.BIN". Agora ele lê a licença do EBOOT.BIN: a edição paga mostra o campo para escolher o seu `.rap`; a grátis continua igual.
- **Placa de vídeo:** o port continua precisando de uma placa com DirectX 12. A Intel HD 4600 (integrada aos Core de 4ª geração) trava no primeiro quadro; isso está em investigação.
- **Para quem compila o port pelo código-fonte:** o repositório agora compila em qualquer pasta; um comando, `tools/cv_make_spu.py`, gera a pasta de SPUs inteira (uma pasta gerada de novo terminava em tela preta); as DLLs do FFmpeg vêm junto; o README traz comandos `winget` para instalar as ferramentas; e o texto em português funciona numa build feita a partir do repositório. A mesma correção de include do `cellPad.c` também foi feita, de forma independente, no fork de **zedhqx4**.
- Mantidas as melhorias anteriores, incluindo o pacote opcional de texturas HD (download separado, link abaixo).

Estas builds compilaram com sucesso para EU/US/JP, normais e de compatibilidade. Os seis executáveis (EU, US e JP, normais e de compatibilidade) foram abertos pelo launcher e chegaram ao jogo a 30 FPS, e um executável normal levado ao caminho de compatibilidade abriu sozinho o `_SSE4`. A versão de compatibilidade também rodou no emulador SDE da Intel: simulando um processador Sandy Bridge, o executável normal passou sozinho para o `_SSE4`; simulando um processador sem AVX, o `_SSE4` rodou 4 minutos sem nenhuma instrução inválida. Ela ainda não foi jogada num processador real sem AVX2, então relatos são bem-vindos (issue #12). A edição japonesa paga não pôde ser testada aqui; a grátis funciona como antes. O projeto continua em desenvolvimento; uma campanha completa em cada configuração não foi verificada.

### Downloads

- **CodeVeronica-PC-v1.1.12.zip:** port principal. Extraia tudo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. SHA-256: `ce2106841fea272a42163f9f13bca2ac813c18363f04485d3dd5c7a199f09329`.
- **Veronica-PKG-Extractor-v1.0.1.zip:** ferramenta para preparar o PKG, em **ZIP separado** (última versão, sem alterações). SHA-256: `a88ddfea2b0a9079922c680abab9c6871e98f4e94f9ea252e6a928b8d44e1cc2`.
- **Pacote de texturas HD:** opcional, no [Google Drive](https://drive.google.com/file/d/1t6yVG1yEC5dl3Td6jT4rhFj1EEwmbsAZ/view?usp=sharing) (não anexado a esta release). Créditos: [TEXTURAS-HD-COMUNIDADE.md](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/blob/code-veronica/projects/code_veronica/TEXTURAS-HD-COMUNIDADE.md).

Não inclui arquivos originais do jogo, modelos 3D do jogo, saves nem configurações pessoais. É necessário fornecer sua própria cópia original do jogo de PS3.
