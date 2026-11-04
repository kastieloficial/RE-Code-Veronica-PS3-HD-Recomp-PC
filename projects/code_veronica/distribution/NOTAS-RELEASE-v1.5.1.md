# v1.5.1 — Character interpolation fix / Correção de interpolação dos personagens

## English

- Fixes a Claire-only eligibility check that prevented native 60 FPS interpolation for Chris, Steve and Wesker. Each playable character now uses its own live-room signature; Claire's alternate outfit follows Claire's character identity.
- Prevents interpolation across character changes, including when the game reuses actor/model memory. Existing loading, camera-cut, resource and health guards remain in place.
- Applies to EU, US and JP, with AVX2 and SSE4 builds. The existing 30 FPS default and all other v1.5.0 features and settings are retained.

This is a focused follow-up to the native animation work contributed by [Lyxodius in PR #36](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/pull/36). It does not add performance optimizations or further visual fixes. Local diagnostics and profiling are not included.

Validation: regional automated interpolation regressions cover the four character IDs, their real signatures, invalid combinations, pose/camera interpolation, exact state restoration and character transitions. The user confirmed improved Steve smoothness in the local US test; Chris and Wesker/Battle Game have not yet received a full visual gameplay validation.

Downloads: extract the complete **CodeVeronica-PC-v1.5.1.zip** and open **Code Veronica.exe**. Keep `bin/` and `assets/` alongside it. **Veronica-PKG-Extractor-v1.0.1.zip** is supplied separately and is unchanged. Supply your own original PS3 game files; no original game archives, 3D models, saves or personal settings are included.

## Português

- Corrige uma verificação exclusiva da Claire que impedia a interpolação nativa a 60 FPS com Chris, Steve e Wesker. Cada personagem jogável agora utiliza seu próprio marcador de sala ativa; a roupa alternativa da Claire segue a identidade da Claire.
- Impede misturar poses ao trocar de personagem, inclusive quando o jogo reutiliza a memória do ator/modelo. Mantém as proteções existentes de carregamento, corte de câmera, recursos e vida.
- Correção para EU, US e JP, nas builds AVX2 e SSE4. O padrão de 30 FPS e os demais recursos e configurações da v1.5.0 são mantidos.

Esta atualização pontual complementa o trabalho de animações nativas contribuído por [Lyxodius na PR #36](https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC/pull/36). Não acrescenta otimizações de desempenho nem outras correções visuais. Os instrumentos de diagnóstico e medição locais não estão incluídos.

Validação: os testes automatizados regionais cobrem os quatro identificadores, seus marcadores reais, combinações inválidas, interpolação de poses/câmera, restauração exata do estado e trocas de personagem. O usuário confirmou maior fluidez com Steve no teste local US; Chris e Wesker/Battle Game ainda não receberam validação visual completa em gameplay.

Downloads: extraia o **CodeVeronica-PC-v1.5.1.zip** completo e abra **Code Veronica.exe**, mantendo `bin/` e `assets/` ao lado. **Veronica-PKG-Extractor-v1.0.1.zip** acompanha em arquivo separado, sem alterações. Forneça seus arquivos originais do jogo PS3; nenhum arquivo original do jogo, modelo 3D, save ou configuração pessoal acompanha o pacote.
