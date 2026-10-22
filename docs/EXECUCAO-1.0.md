# Execução da candidata 1.0 — 22/10/2026

O roadmap completo continua em [PLANO-1.0.md](PLANO-1.0.md). A RC4 passou na
validação gráfica; o código e os builds locais agora estão em 1.0.0. A release
estável ainda não foi publicada.

Release e ASan/UBSan são verificados separadamente; resultados finais em
[VALIDATION.md](VALIDATION.md). As capturas usam fixtures de teste.

## Alterações implementadas

### Fechamento nativo da RC4

- Bibliotecas foram extraídas para `LibraryPage`, sem dependência de WebKit.
  Testes cobrem paginação de 160 registros, preservação de seleção e rolagem,
  respostas antigas de busca, abrir em primeiro/segundo plano, edição por Enter,
  falha com nova tentativa e exclusão de todo o histórico com busca ativa.
- A busca de preferências aceita múltiplas palavras e aliases, leva ao controle
  correspondente e restaura a posição anterior ao limpar. Cada preferência
  registrada tem um único controle principal e informa o momento do efeito.
  O reset foi exercitado pela confirmação da interface, preservando favoritos.
- O retorno entre ferramentas restaura foco e invalida callbacks antigos. Abas
  expõem papel, seleção e ações ao Qt Accessible; pastas têm botão nativo.
- Medidas de espaçamento do QSS e da barra usam `UiMetrics`; papéis de cor têm
  testes de contraste. O fallback de favicon também usa a família de ícones do
  projeto. Ações de bibliotecas refluem com fonte de 18 px em 428 px de conteúdo.
- `tools/validate-release.py` é o procedimento único do CI e da validação local:
  suíte, escalas, capturas, benchmark, smoke e pacote, com perfil de teste isolado.
  A ausência de display faz a rodada gráfica falhar. `--offscreen` se identifica
  como validação apenas de widgets e não aprova lançamento.
- O benchmark preparado mede três rodadas, árvore de processos e latência com
  cinco páginas locais, dez abas vazias e vinte ciclos de ferramentas. Ainda não
  executou neste ambiente. O custo do host de testes é identificado no relatório.
- Tooltips de títulos, favoritos, espaços e arquivos escapam marcação recebida;
  texto como `<img>` aparece literalmente. Abas essenciais também possuem nome
  acessível, mesmo quando a apresentação visual usa só o favicon.
- Exportação e alterações de padrão usam mensagens temporárias, sem janela de
  sucesso. O resultado da importação é interno; desfazer fica disponível antes
  de sair dessa tela, preservando favoritos anteriores. A área de mensagens se
  oculta quando vazia, evitando uma faixa permanente no rodapé.

Resultado final local: **56 aprovados, zero falhas e 15 skips** em Release e
ASan/UBSan. A matriz de widgets passou nas escalas 100/125/150/200%, sem aprovar
a integração GTK nem a decoração do compositor.

| Frente | Resultado | Evidência |
| --- | --- | --- |
| Rotas internas | Retorno preserva tela, campos e rolagem; deduplicação por rota; cache limitado a seis telas | preservesInternalNavigation; integração embedsBrowserTools preparada |
| Espaços | Renomeação aponta ao selecionado; exclusão intermediária transfere abas, pastas e referências de abas fechadas | editsSelectedWorkspace; migratesWorkspaceContents |
| Pastas | Criação e renomeação dentro do navegador | Formulário nativo editFolder; integração gráfica pendente |
| Configurações | Oito categorias, opções avançadas recolhidas, busca restaura categoria e abre grupos correspondentes | searchesAndRefreshesPreferences; capturas de laysOutInternalScreens |
| Personalização | Presets de densidade, dimensões individuais e reset por categoria com confirmação | appliesDensityPresets; restoresOnlyCategoryPreferences |
| Preferências | Registro central de padrões; atualização ao restaurar; controles especiais persistem antes de atualizar o tema | src/core/Preferences.cpp; PREFERENCES.md |
| Lateral | Nome do espaço usa largura disponível; título da aba reserva área de fechar | fitsWorkspaceHeadingToSidebar; capturas |
| Nova aba e compacto | Nome do espaço sem prefixo repetido; lateral permanece acessível durante arraste, menu ou foco de teclado | Capturas Qt; uso real em modo compacto pendente |
| Componentes | Medidas comuns, ícones próprios consistentes e distinção de foco por teclado nos botões | UiMetrics; ThemeManager; inspeção das capturas |
| Downloads | Abrir arquivo/pasta, erro inline para arquivo removido e indicador de atividade | reportsMissingDownload; keepsDownloadSelectionDuringRefresh |
| Bibliotecas | Abrir em segundo plano, preservar seleção/rolagem em atualização e explicitar alcance da limpeza | Leitura do fluxo; integração gráfica pendente |
| Busca na página | Estado de busca em andamento e cancelamento de timer; retorno de foco mantido | embedsFindBar; integração WebKit pendente |
| Atalhos | Registro comum para Qt, GTK e menu; remoção de registros Qt duplicados | organizesBrowserCommands; resolvesNavigation |
| Contraste | Tom da cor personalizada se adapta para contraste mínimo de 4,5:1 | keepsCustomAccentReadable |
| CI/release | Logs e capturas como artefatos; integração exige display; pacote e hashes antes do rascunho | Workflows locais; execução remota pendente |

## Bloqueios verificados

1. Não há Xvfb instalado nem display acessível ao processo. KWin virtual em
   runtime isolado falha ao criar socket com `Operation not permitted`.
2. O conector GitHub lê o repositório, mas criar a árvore de arquivos retorna
   HTTP 403 `Resource not accessible by integration`. Nenhuma branch ou tag foi
   criada pelo conector.
3. O acesso Git por SSH, sem alterar a configuração do sistema, falha na
   resolução DNS de github.com. Não houve envio nem execução nova do CI.

## Critérios ainda abertos

Os 72 testes gráficos passaram, incluindo o benchmark e os ciclos de
clipboard/nova aba, gestos de abrir links e retorno entre ferramentas. A matriz
de escalas de 125%, 150% e 200% também passou. Ainda faltam a conferência manual
de compositor/monitores, tecnologia assistiva, reprodução de vídeo e atualização
com dados existentes. O pacote estável local já foi gerado e a versão confere;
o smoke test precisa ser repetido em uma sessão gráfica.

O atalho local aponta para `dist/zenith-browser`, com ação de janela privada.
O TGZ estável local fica em `~/.cache/zenith-build/1.0.0-packages/`. A
validação gráfica RC4 e o benchmark ficam em `~/.cache/zenith-validation/20261022-111132/`;
as suítes estáveis sem display ficam em `~/.cache/zenith-build/1.0.0-*-validation/`.
Qt local: 6.12.0; WebKitGTK: 2.54.1.

O benchmark mediu a árvore inteira e as latências para a carga local registrada
em `VALIDATION.md`; não transforma essa carga em promessa de consumo em sites
reais. Não transformar os testes offscreen em aprovação gráfica da versão
estável.
