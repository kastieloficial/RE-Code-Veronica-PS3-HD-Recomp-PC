# Capturar shaders na live

Igual ao port do God of War Ascension: os pipelines vistos jogando viram uma
lista que vai junto com o jogo, e o PC de cada jogador compila essa lista para
a própria placa **antes** do gameplay (nota de progresso no canto). Da segunda
vez em diante o cache deixa a abertura rápida.

A lista guarda os programas do PS3 (microcódigo + estado fixo), não HLSL
traduzido. Por isso serve em NVIDIA, AMD e Intel e continua valendo se o
tradutor de shaders mudar.

## Como capturar

1. Dê dois cliques em `capturar-shaders-live.cmd` e jogue normalmente. Passe
   por salas diferentes, atire com várias armas, abra menus, veja cutscenes,
   mapa, inventário e a tela de game over.
2. Feche o jogo normalmente. A lista é gravada a cada segundo, então fechar
   pelo X não perde nada.
3. Rode `juntar-shaders-capturados.cmd`. Ele junta
   `build-release\bin\shader_cache\*\pipelines.list` com
   `shader-seed\pipelines-seed.list` (a anterior fica como `.bak`).
   Para incluir listas de outro PC, arraste a pasta do jogo (ou o arquivo
   `pipelines.list`) em cima do `.cmd`.

## Como entregar

Envie só `shader-seed\pipelines-seed.list` (ou faça commit dele). Ele não tem
dados do jogo. O build copia esse arquivo para `assets\shader-seed\`, e o
`empacotar_release.py` o coloca no zip.

## Chaves

- `CV_SHADER_SEED=0`: ignora a lista embarcada.
- `CV_SHADER_SEED=<arquivo>`: usa outra lista.
- `CV_SHADER_SEED_WAIT=0`: compila a lista embarcada em segundo plano, sem
  fazer a primeira abertura esperar.
- `SHADER_PRELOAD=0` (launcher: pré-carregamento desligado) desliga tudo.
- `shader_cache\<pasta>\seed-built.txt` marca qual lista já foi compilada.
  Apague esse arquivo para forçar a espera de novo.

O pisca na tela vem de draws pulados porque o pipeline ainda não estava
pronto. Com `CV_STATS=1`, a linha `[STATS]` mostra `skip=N`. A meta é ficar
perto de zero depois da lista embarcada.
