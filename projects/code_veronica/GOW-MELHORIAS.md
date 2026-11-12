# Branch gow-melhorias (local, não publicado)

Base: v1.5.3 (`325ae74`). Cada item tem seu commit e sua chave.
`comparar-ab.cmd` abre o jogo com tudo desligado (A) ou tudo ligado (B).

| Item | Chave | Padrão |
|---|---|---|
| Lista de shaders embarcada, compilada antes do gameplay na 1ª abertura | `CV_SHADER_SEED=0` desliga, `CV_SHADER_SEED_WAIT=0` compila em segundo plano | ligado |
| Linha leve `[STATS]` a cada 5 s | `CV_STATS=1` | desligado |
| Threads em núcleos próprios (só em CPU de 6 núcleos) | `CV_PIN=auto` (`1` força em 5+ núcleos) | desligado |
| Prioridade acima do normal | `CV_PRIORITY=1` | desligado |
| Cadência de quadros estilo RTSS (sem vsync e sem FG) | `CV_FRAME_PACE=1`, `CV_FRAME_PACE_HZ` | desligado |
| Áudio: fade em vez de corte seco quando o som atrasa | `CV_AUDIO_SMOOTH=1` | desligado |
| Áudio: thread com prioridade alta (MMCSS) | `CV_AUDIO_PRIO=1` | desligado |
| Áudio segue a troca de saída do Windows (fone ↔ caixa) | `CV_AUDIO_FOLLOW_DEVICE=0` desliga | ligado |
| Sem "Não está respondendo" | `CV_NO_GHOSTING=1` | desligado |
| Frame generation desliga em GPU anterior a RX 5000 / RTX 20 | `CV_FG_GPU_CHECK=1` | desligado |

Captura de shaders: veja `CAPTURAR-SHADERS.md`.

## Não feito

- **Job Edge nativo (item 4).** O job `0DA78AE320040FBA` não faz geometria:
  não tem nenhuma instrução de ponto flutuante. São 3,7 KB de controle e DMA
  (80 escritas e 11 leituras de canal MFC). Reescrevê-lo à mão só tiraria os
  trampolins, e cada DMA continuaria passando pelo mesmo runtime. O ganho de
  33→47 FPS do GoW veio de jobs de geometria pesados, que o Code Veronica
  não tem nesse formato. Antes de qualquer reescrita, o certo é medir o custo
  real desse job com `PERF_THREADS`.
- **Reconexão de controle.** Já existe: o XInput tenta de novo os slots
  vazios a cada 500 ms.
