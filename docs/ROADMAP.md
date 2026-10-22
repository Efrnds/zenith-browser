# Zenith: do protótipo ao uso diário

Plano de execução para o acabamento e lançamento da 1.0.0:
[PLANO-1.0.md](PLANO-1.0.md). O plano parte da RC2, registra problemas observados,
ordena as entregas e define critérios de aceite visual, funcional e gráfico.

Auditoria inicial: 21/10/2026. Base: fontes locais, com alterações prévias preservadas.
Este documento distingue recursos encontrados, defeitos confirmados e trabalho pendente.
Não representa certificação de segurança ou compatibilidade. Os valores de 600 MB e
2 GB foram relatados pelo autor e ainda precisam de reprodução controlada.

## Direção e limites

Manter a experiência de abas verticais, workspaces, essenciais e split view com
baixo custo de memória. A implementação usa C++20, Qt Widgets e WebKitGTK 4.1;
não é um fork do motor Gecko. Atualizações e compatibilidade dependem também do
WebKit instalado. O host atual exige X11/XWayland, GtkPlug e integração de eventos
GTK/Qt. Há trechos de credenciais multiplataforma, mas o executável depende de
WebKitGTK, X11 e DBus: não considerar Windows/macOS suportados por enquanto.

## Estado da implementação 0.2.0

### Candidata 1.0.0-rc.1

- Ferramentas internas integradas e Ctrl+F como barra junto ao conteúdo.
- Preferências de usuário e ferramentas reunidas nas configurações, com mais
  personalização de dimensões, fontes, cor, conteúdo, senhas e downloads.
- Formulários internos para favoritos, pesquisa personalizada, perfis e espaços.
- Workflow de tags com testes, pacote, hashes e rascunho de Release preparado.
- Integração gráfica, pacote do Ubuntu e medição da árvore de processos ainda
  precisam de homologação antes da estável: ver [RELEASE.md](RELEASE.md).

### Revisão 0.4.0

- Nova aba nativa com busca e acesso às bibliotecas, sem instanciar WebView para uma aba vazia nova. O motor é criado ao navegar ou usar split; popups relacionados preservam o fluxo anterior.
- Configurações em grupos por função, largura de leitura limitada, prévia de tema e telas estreitas com rolagem.
- Histórico, favoritos, credenciais e downloads com linhas detalhadas e estados vazios; downloads ativos com progresso visual.
- Moldura opaca integrada ao tema e sem recuo em maximização/tela cheia. GtkPlug sem decoração adicional, com fundo de host coerente. Preferência de tema exportada ao gerenciador X11.
- Tradução pt_BR dos diálogos Qt quando disponível no runtime.
- Evidências e limites em [APPEARANCE.md](APPEARANCE.md). Conferência gráfica do motor, da moldura externa e benchmark seguem pendentes.

### Revisão 0.3.1

- Organização aproximada do Zen: lateral dedicada a fixadas, abas, pastas e espaços no rodapé; nova aba após a lista.
- Removido o falso menu de configurações da lateral. Configurações reais no menu principal e em Ctrl+,; F10 abre o menu também a partir do WebKit.
- Barra com modo compacto, navegação, controles do site, favorito, downloads e menu. Tela dividida no contexto de abas e em Visualização.
- Menu principal agrupado em bibliotecas, Página, Visualização e Ferramentas; preferências de busca, bloqueio e navegador padrão nas configurações.
- Criação ativa o novo espaço. Controles da página desabilitados durante configurações, com retorno por Voltar e saída ao navegar.
- Mapa de funções e evidências em [APPEARANCE.md](APPEARANCE.md). Integração GTK/X11 ainda pendente.

### Revisão 0.3.0

- Temas claro, escuro e sistema com quatro cores de destaque e paleta compartilhada entre Qt, abas e pastas.
- Espaços: criação visível na sidebar e gerenciamento nas configurações, até 16 espaços.
- Fundo padrão das páginas corrigido para branco; preferência de tema dos sites separada da interface.
- Antialiasing e hinting configuráveis pelo mecanismo GTK; ícones vetoriais na interface.
- Configurações com rolagem e layout conferido em janela estreita. Evidências e limites em [APPEARANCE.md](APPEARANCE.md).
- Pendentes: comparar nitidez de páginas e imagens na sessão gráfica, medir GPU/software e homologar atualização de tema nos sites reais.

### Revisão 0.2.6

- Sidebar: criação, fechamento e reordenação em lote passam a agendar uma única reconstrução de widgets no próximo ciclo de eventos. A seleção é reaplicada após a reconstrução. Isso elimina as reconstruções intermediárias durante a restauração ou atualizações em lote.
- Abas fechadas: reabertura preserva posição, pasta válida e áudio silenciado. O estado serializado recebe limite de 1,5 MiB por aba e 8 MiB no conjunto das até 20 abas fechadas. Ao exceder, estados antigos são retirados e os endereços continuam disponíveis para reabrir; essas abas voltam sem o histórico anterior de voltar/avançar.
- Navegação: Ctrl+1–9, Ctrl+Tab e fechamento das seguintes compartilham a ordem de essenciais, pastas e abas soltas. A ordem acompanha a reordenação das pastas.
- Busca: F3/Ctrl+G avançam e Shift+F3/Ctrl+Shift+G voltam; o último termo permanece ao fechar o diálogo, e uma busca encerrada é reiniciada corretamente. Botões ficam inativos sem termo e Enter deixa de depender do botão padrão do diálogo.
- Recarga: Ctrl+Shift+R e Ctrl+F5 solicitam recarga ignorando cache do WebKit.
- Notificações: envio ao serviço do desktop usa DBus assíncrono com timeout de três segundos, sem introspecção síncrona. Pendências e referências retidas têm limite, e há limpeza em trinta segundos se o desktop não emitir fechamento. Mensagens carregam o host da página; a ativação verifica se a aba continua na mesma origem.

Os cenários puros e de sidebar passaram localmente. Busca/recarga com foco GTK e notificações reais do desktop ainda precisam de homologação. Não se atribui ganho medido de RAM ou tempo de abertura a essas mudanças.

### Revisão 0.2.5

- Segurança: ponte do cofre em mundo JavaScript isolado, somente no frame principal; HTTPS com exceção de certificado não recebe nem captura credenciais; submit sintético é ignorado; prompt de gravação usa texto simples e Cancelar por padrão. Ver políticas e limites em [SECURITY.md](SECURITY.md).
- Desempenho: páginas sem campo de senha deixam de consultar o cofre; progresso de downloads notifica a interface no máximo a cada 150 ms, além de mudanças de estado; objetos nativos concluídos são liberados e o limite do histórico não depende mais de o primeiro item já ter terminado.
- Usabilidade: lista de downloads atualiza linhas existentes e preserva seleção/rolagem; Ctrl+1 a Ctrl+8 selecionam abas do workspace e Ctrl+9 seleciona a última; o filtro Qt de atalhos exige que sua janela esteja ativa. O endereço exibido omite usuário/senha embutidos na URL.
- Dados: exportação de favoritos persistentes em worker com gravação incremental, conclusão/erro visíveis e rejeição do destino SQLite/WAL/SHM. O perfil privado em memória usa uma consulta local síncrona para não copiar sua base para disco; exportar continua uma escolha explícita.

Oito testes gráficos anteriores e o novo teste da ponte de credenciais continuam pendentes de display. Não foi medido ganho de RAM/CPU nem declarada auditoria completa de segurança. O item de exportação da revisão anterior recebeu implementação nesta rodada; faltam cancelamento e homologação com grandes volumes no desktop.

### Revisão 0.2.4

Três gargalos corrigidos nesta rodada:

- Sugestões da barra consultavam histórico/favoritos na thread da interface a cada foco. Agora uma consulta em worker retorna no máximo 100 URLs únicas, com favoritos primeiro e visitas recentes depois. Resultados de um foco anterior são ignorados. O perfil privado continua sem consultar o histórico normal.
- Cada varredura de descarte disparava JavaScript para todos os candidatos de uma vez. Agora consulta um candidato por vez, respeita a ordem de acesso e para quando a meta de páginas carregadas foi atendida. Permanecem as verificações de mídia, edição, visibilidade e downloads.
- A atualização do adblock podia acumular respostas sem limite e aplicar somente uma das listas após falha parcial. Agora há limite de 8 MiB por lista, 10 segundos sem progresso e 30 segundos totais; respostas incompletas ou sem domínios válidos preservam as regras anteriores. O botão não inicia outra atualização durante a compilação, e falhas ao gravar cache aparecem no estado.

Prioridades seguintes, ainda pendentes:

| Prioridade | Melhoria | Evidência e critério de aceite |
|---|---|---|
| 1 | Homologação gráfica da base | Oito testes do motor continuam pulados localmente. Executar CI com Xvfb e testar split, minimizar, retomada de abas, drafts e chamadas no desktop antes de fechar estabilidade. |
| 1 | Consumo total reproduzível | Ainda não há PSS medido antes/depois. Registrar árvore de processos, CPU ociosa e tempo de retorno às mesmas cinco abas em três rodadas. O script e o protocolo estão em MEMORY.md. |
| 2 | Cobertura do adblock | O parser aceita um subconjunto de ABP e não importa filtros cosméticos gerais. Ampliar por tipo de regra com fixtures e exceções por site, sem transformar regras condicionais em bloqueios globais. |
| 2 | Operações grandes da biblioteca | A exportação de favoritos ainda consulta páginas SQLite na interface. Exportar em worker e validar milhares de itens, cancelamento e erro de disco. |
| 2 | Mídia, login e dispositivos | Confirmar Outlook, OAuth, uploads, microfone/câmera, WebRTC e codecs com sessões reais. Isolar limitações do motor de defeitos no host. |

Oito frentes originais e seu inventário seguem abaixo. A versão 0.2.4 não declara essas pendências concluídas nem ganho de RAM medido.

As oito frentes receberam implementação, mas não estão todas homologadas.
Os critérios de aceite abaixo continuam sendo a referência; código compilado
não equivale a comportamento gráfico confirmado.

| Frente | Implementado nesta execução | Ainda falta para fechar a frente |
|---|---|---|
| 0. Estabilidade | Ciclos de descarte, restauração com navegação explícita, histórico em cache, estado de crash e recarga; testes headless e integração separados | Rodar os quatro testes WebKit e validar split/minimizar/restaurar no desktop |
| 1. Segurança | Consentimento por solicitação, bloqueios persistidos, desconhecidos negados, TLS temporário com indicador EXC, limpeza de exceções antigas, sandbox explicitamente habilitado, HTTP recusado no cofre, múltiplas contas e namespaces por perfil, sessão atômica com backup | Homologar dispositivos e revogação no desktop; validar cofre real e migração de contas antigas; auditoria de segurança aprofundada |
| 2. Navegação | Busca com contagem, zoom por origem, reabertura limitada a 20 abas, duplicação, fechar outras/seguintes, atalhos compartilhados Qt/GTK, CLI, instância única, erros de rede e crash com recarga; menu WebKit nativo | Validar todos os atalhos dentro do GTK, incluindo foco em split; testes completos do menu de contexto |
| 3. Arquivos | Downloads com destino, nome saneado, progresso/taxa, cancelamento, erro, pasta e lista recente persistida; confirmação ao sair; impressão pelo WebKit | Rodar integração de download; validar disco cheio/overwrite/cancelamento em rede real; retomada não implementada; PDF depende do backend de impressão |
| 4. Dados | SQLite, histórico com filtro/paginação/limpeza por intervalo e consulta em worker com conexão somente leitura, favoritos com pasta e edição, sugestões, HTML com prévia/transação/deduplicação/desfazer, limpeza separada, perfil privado, perfis nomeados e padrão configurável | Hierarquias de favoritos importados são simplificadas; sugestões/exportação ainda usam consultas síncronas limitadas; validar ausência de resíduos do WebKit no modo privado |
| 5. Desempenho | Inatividade e meta de páginas carregadas configuráveis, candidatos ordenados por último acesso, verificações de formulário/frame/mídia/captura/download, menos serialização ao salvar, timer YouTube reduzido e pausado em documento oculto, ferramenta PSS/RSS | Benchmark Zen/Zenith, CPU/bateria, pressão real de memória, orçamento em MiB e avaliação de aceleração; nenhuma economia adicional medida |
| 6. Experiência | Workspaces de 1 a 16 com nomes, mover abas entre espaços, reordenar abas e pastas por drag/menu, áudio/mudo/crash, alvo ativo no split, proporção persistida, animações reduzidas, foco/contraste dos novos diálogos | Validar drag no desktop; isolamento de perfil por workspace, leitores de tela, DPI/múltiplos monitores e auditoria gráfica completa |
| 7. Distribuição | .desktop, ícones, integração de navegador padrão após instalar, pacote TGZ, README, CI Debug/Release com Xvfb e smoke, sanitizers; versões Qt/WebKit na tela Sobre | Executar CI remoto e instalar em sistema limpo; homologar login/OAuth/WebRTC/codecs/DRM; política operacional de releases e rollback; Wayland nativo continua estudo separado |

Nenhuma fase foi marcada como concluída por falta de validação gráfica.
Sync, extensões e ports continuam sendo estudos posteriores, como definido no escopo original.

## Inventário inicial

| Área | Encontrado | Lacuna ou risco |
|---|---|---|
| Navegação | URL/busca, voltar, avançar, recarregar, parar | Busca na página, zoom e atalhos completos |
| Abas | Verticais, essenciais, pastas, dois workspaces, split | Reabrir fechadas, duplicar, ordenar, silenciar, indicador de áudio |
| Sessão | URLs, títulos, ícones, histórico por aba, restauração lazy | Recuperação de crash, migração, gravação resistente a falhas |
| Memória | Descarte e suspensão ao minimizar | Proteção a mídia/formulários/downloads, orçamento e medição por árvore |
| Identidade | Cookies persistentes, cofre do sistema, validação de origem | Múltiplas contas, falhas do cofre, importação e privacidade |
| Proteção | TLS rejeita erros por padrão, adblock e atualização de listas | Exceções TLS persistentes sem gestão; controles por site ausentes |
| Web | Popups, diálogos JS, upload, fullscreen, notificações | Downloads sem fluxo próprio; permissões incompletas |
| Preferências | Busca, adblock, compacto, sessão, minimizar | Limpeza de dados, perfis, acessibilidade e integrações do sistema |
| Qualidade | CMake/CTest e testes de regras/credenciais/histórico | CI, testes de integração, empacotamento e release reproduzível |

## Ordem de execução

### 0. Base confiável e regressões (primeira entrega)

- [x] Corrigir descarte: reiniciar `loadStarted` e `historyApplied` na recriação.
- [x] Preservar URL de aba ainda não carregada e histórico salvo quando o motor não fornece estado.
- [x] Remover concessão automática de notificações. Ficam negadas até existir consentimento por site.
- [x] Acrescentar regressão de três ciclos de descarte/restauração com página local.
- [x] Tratar morte do processo WebKit com estado de erro e recuperação explícita.
- [ ] Testar voltar/avançar após descarte, split, troca de workspace e minimizar/restaurar.
- [x] Separar perfil de teste dos dados reais e configurar matriz em CI com X11 virtual (execução remota pendente).

Aceite: build e CTest passam; ciclos não perdem URL/histórico nem deixam aba vazia.
Os itens marcados indicam implementação; o resultado da execução fica em VALIDATION.md.

### 1. Segurança e integridade antes de ampliar uso

Depende da fase 0. Implementar serviço de permissões por origem (esquema, host,
porta), com permitir uma vez, negar e revogar. Cobrir notificações, câmera,
microfone, localização, clipboard e captura quando disponíveis no motor.
Negar solicitações desconhecidas. Consulta de permissão deve refletir a decisão real.

- Auditar ponte de credenciais contra navegação durante resposta, iframes,
  preenchimento automático, mensagens forjadas e múltiplas contas no mesmo domínio.
- Fazer exceção TLS temporária por padrão; listar/remover exceções persistentes.
- Revisar popups automáticos, autoplay, cookies de terceiros, ITP e esquemas externos.
  A configuração atual permite cookies e autoplay de forma ampla; medir compatibilidade
  antes de mudar padrões e oferecer exceções explícitas.
- Confirmar sandbox efetivo e reduzir permissões de filesystem/dispositivos ao necessário.
- Evitar registrar senhas, tokens e URLs privadas em diagnósticos.
- Gravar sessão versionada com backup e recuperação de arquivo inválido/disco cheio;
  não executar restauração de estado não confiável sem limites de tamanho.

Aceite: um site não acessa dispositivo sem decisão; revogação funciona após reinício;
falha de escrita mantém sessão anterior; TLS inválido continua bloqueado por padrão.

### 2. Navegação essencial

Depende da fase 0; interface mantém a linguagem visual existente.

- Buscar na página com Ctrl+F, próximo/anterior, contagem e Escape.
- Zoom por origem com Ctrl+/Ctrl-/Ctrl+0, limites e persistência.
- Ctrl+Shift+T com pilha limitada de abas fechadas, histórico e workspace;
  duplicar aba, fechar outras/à direita e confirmar fechamento em massa.
- Atalhos de recarga, histórico, downloads, fullscreen e Alt+setas devem
  funcionar tanto com foco no widget Qt quanto na página GTK.
- Menu de contexto: link em nova aba/janela, copiar URL, salvar imagem, inspecionar.
- Abrir URLs pela linha de comando; instância única com encaminhamento de argumentos.
- Exibir erro de rede com tentar novamente e diferenciar cancelamento de falha.

Aceite: todos os fluxos por teclado com foco dentro/fora da página e em split;
nenhum atalho dispara duas vezes; fechar/reabrir preserva o destino e o workspace.

### 3. Downloads e arquivos

Depende de integridade/permissões da fase 1.

- Gerenciador ligado ao contexto WebKit: destino, progresso, taxa, cancelar,
  erro, abrir pasta e lista recente. Retomar apenas se o servidor/motor suportar.
- Sanitizar nome sugerido, impedir sobrescrita silenciosa e tratar disco cheio.
- Anexos e tipos não renderizáveis devem chegar ao gerenciador; não abrir
  executáveis automaticamente. Downloads ativos não podem morrer com descarte da aba.
- Implementar imprimir/salvar PDF e decidir visualização de PDF conforme motor disponível.

Aceite: servidor local testa redirecionamento, Content-Disposition, nome malicioso,
cancelamento, fechamento de aba e falhas; arquivos íntegros e destino previsível.

### 4. Histórico, favoritos e dados pessoais

- SQLite para visitas/favoritos com consultas fora do caminho de pintura,
  paginação, busca, edição, pastas e exclusão por intervalo.
- Autocomplete da barra com favoritos/histórico, distinguindo texto de URL;
  testar localhost, IP, IPv6, porta, Unicode, espaços e esquemas não permitidos.
- Importar/exportar favoritos HTML com prévia e deduplicação; importação de
  outros navegadores nunca altera arquivos da origem.
- Limpar cookies, cache, histórico, permissões e credenciais separadamente.
- Perfil privado com armazenamento efêmero separado; nenhum registro no perfil normal.
- Perfis persistentes isolados e configuração do perfil padrão.

Aceite: reinício preserva dados normais; sessão privada não deixa visitas/cookies
no perfil comum; limpeza tem escopo verificável e importação é reversível.

### 5. Memória, CPU e autonomia

Instrumentar antes de escolher limite fixo. Registrar PSS/RSS da árvore inteira,
picos, CPU ociosa, inicialização e latência ao voltar para aba descartada.

- Suspensão por inatividade com LRU e orçamento configurável; proteger abas visíveis,
  áudio/vídeo, chamadas, formulários alterados e tarefas ativas. Se não for possível
  detectar um estado com confiança, não descartar automaticamente.
- Rever suspensão total ao minimizar: atualmente pode interromper tarefas legítimas.
- Reduzir serialização de todas as abas em cada salvamento e escritas redundantes.
- Auditar timer de 400 ms injetado no YouTube e custo de listas de bloqueio.
- Medir renderização acelerada versus software, glitches e vídeo antes de alterar
  as chamadas contraditórias NEVER/ALWAYS de aceleração em WebProfile.
- Não limitar processos nem desligar isolamento como atalho para reduzir RAM.

Protocolo: mesma máquina, versões registradas, perfis novos, extensões equivalentes,
mesmas cinco URLs e roteiro; medir após 60 s, 5 min, troca de abas e 10 ciclos de
minimizar/restaurar. Separar carga ativa e abas descartadas. Fazer três repetições
e comparar mediana e pico; RSS soma páginas compartilhadas, preferir PSS.
Meta inicial: estabelecer baseline reproduzível; só então fixar orçamento por cenário.

### 6. Experiência Zenith e acessibilidade

- Reordenar abas/pastas com drag-and-drop e alternativa por teclado.
- Workspaces configuráveis, renomeáveis e com isolamento opcional de perfil;
  substituir constantes de dois workspaces antes de expandir.
- Split com foco ativo claro, proporção persistida e ações dirigidas ao painel correto.
- Mostrar abas com áudio e permitir silenciar; indicadores de carregamento/crash/descarte.
- Revisar contraste, foco visível, escala/DPI, leitores de tela e movimento reduzido.
- Validar compacto, fullscreen e restauração com múltiplos monitores.

Aceite: fluxos sem mouse, escala 100/150/200%, duas telas e foco Qt/GTK previsível.
Mudanças visuais dependem da escolha de aplicação do antislop e preservam a identidade existente.

### 7. Compatibilidade e distribuição

- Matriz de login/OAuth, uploads, WebRTC, câmera, áudio, codecs e streaming.
  DRM e extensões dependem das capacidades reais do motor, não prometer paridade.
- Inspector de desenvolvimento e relatório local de versão/diagnóstico sem segredos.
- Arquivo .desktop, ícones, handlers http/https e opção de navegador padrão.
- Pacote Linux com dependências e atualização documentadas; acompanhar correções do
  WebKit da distribuição. Validar release em ambiente limpo e rollback.
- CI: build Debug/Release, CTest, sanitizers em job separado e smoke com display virtual.
- Documentar limites XWayland; avaliar integração nativa Wayland como projeto separado.
- Sincronização, extensões e ports para outros sistemas ficam depois da base estável,
  com estudo próprio de compatibilidade, criptografia e custo de manutenção.

Aceite: instalar em sistema limpo, abrir links externos, atualizar e reverter sem
perder perfil; falhas conhecidas e dependências publicadas no release.

## Regra de conclusão

Uma fase só fecha com implementação, testes dos cenários de aceite e registro de
limitações. Não substituir o motor, reescrever a interface ou perseguir uma cifra
de RAM sem medição. Próxima etapa de homologação: executar a matriz gráfica e de
compatibilidade em desktop Linux antes de declarar a versão pronta para uso diário.
