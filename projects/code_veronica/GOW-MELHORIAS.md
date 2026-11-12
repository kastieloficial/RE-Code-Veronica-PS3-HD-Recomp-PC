# Branch gow-melhorias (local, não publicado)

Base: v1.5.3 (`325ae74`). Cada item tem seu commit e sua chave.
As chaves continuam valendo por variável de ambiente; os atalhos de comparação foram removidos.

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
| Pool de threads das tarefas SPU (já existia, opt-in) | `SPU_TASK_POOL=1` | desligado |
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

## Medição: tarefa SPU de imagem 13 (tela inicial, 60 s)

`spu_0012_at_0098F880` roda até cerca de 1.400 vezes/s. Sem o pool, cada
tarefa ganha uma thread nova: o início leva em média 80–110 µs e a execução
110–250 µs. Com `SPU_TASK_POOL=1`, as 12 threads são reaproveitadas: o início
cai para cerca de 15 µs e a execução para 65–100 µs (cache quente).
Uma das aberturas com o pool fechou sozinha logo no começo, e a seguinte rodou
normal. Ainda falta confirmar se foi coincidência. Logs: `D:/reveronica/edge-medida*.txt`.
