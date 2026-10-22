# Plano de acabamento visual e usabilidade — Zenith 1.0.0

Data: 22/10/2026. Base auditada: 1.0.0-rc.2, fontes locais e capturas de widgets
Qt em claro/escuro, 1280×800 e 760×540, incluindo fonte ampliada. Os dados das
capturas são fixtures de teste. Este documento acompanha a execução. Itens
marcados têm evidência local; os demais ainda exigem implementação ou validação.
O estado detalhado está em [EXECUCAO-1.0.md](EXECUCAO-1.0.md).

## Resultado esperado

Um navegador leve, com abas verticais e organização inspirada no Zen, em que
navegar, organizar páginas e alterar preferências tenham comportamento previsível.
A versão estável precisa reunir acabamento visual e confiabilidade comprovada
em sessão gráfica. A presença de uma opção na tela só conta como concluída
quando seu efeito, persistência, feedback e navegação por teclado funcionarem.

Direção já definida pelo autor: grafite/claro, destaque configurável, ferramentas
internas e atenção ao consumo total de memória. Antislop durante a execução;
ENERGY 2 / RHYTHM 2 / MOTION 1. Qt nativo para a interface e bibliotecas.

## O que a auditoria encontrou

| ID | Evidência atual | Consequência | Prioridade |
| --- | --- | --- | --- |
| A01 | A rodada completa registra 36 aprovações e 14 testes pulados por falta de X11; ver VALIDATION.md. | Clipboard, clique do meio e foco GTK/Qt precisam passar em integração gráfica antes da estável. | P0 |
| A02 | Na captura de Aparência em 760×540, a prévia ocupa o espaço anterior aos seletores. | O usuário precisa rolar para encontrar os próprios controles de tema. | P1 |
| A03 | SettingsPage concentra pesquisa, sessão, downloads, bibliotecas e perfis em Geral. | A categoria cresce muito e mistura decisões com ferramentas. | P1 |
| A04 | Sidebar usa largura fixa de 156 px para truncar o nome do espaço. | Aumentar a lateral não recupera o espaço disponível para o título. | P1 |
| A05 | O formulário de renomeação usa o nome do item selecionado; manageWorkspace altera o espaço ativo. | Seleção e alvo da edição podem divergir. Confirmado pela leitura dos dois caminhos. | P1 |
| A06 | Exclusão de espaços aceita somente o último espaço sem abas. | O modelo é difícil de entender e pouco útil no fluxo normal, que cria uma nova aba ao ativar um espaço vazio. | P1 |
| A07 | presentInternalPage mantém apenas uma ferramenta transitória; fechar volta ao conteúdo web. | Abrir Downloads a partir das configurações não preserva uma rota de retorno às configurações. | P1 |
| A08 | Margens, espaçamentos e alturas são definidos separadamente em vários widgets. | Ajustes locais deixam diferenças de ritmo e alinhamento entre telas. | P1 |
| A09 | ZenithIcons combina símbolos do sistema com desenho próprio de fallback. | A aparência dos controles depende de quais ícones cada tema possui. | P1 |
| A10 | Criação e renomeação de pastas na Sidebar ainda usam QInputDialog. | A organização de abas ainda tem formulários fora da área interna. | P1 |

Referências de implementação: [SettingsPage](../src/ui/SettingsPage.cpp),
[Sidebar](../src/ui/Sidebar.cpp), [MainWindowTools](../src/ui/MainWindowTools.cpp),
[MainWindow](../src/ui/MainWindow.cpp), [ZenithIcons](../src/ui/ZenithIcons.cpp),
[PageWidgets](../src/ui/PageWidgets.cpp), [estilo](../resources/style.qss).

Questões que exigem sessão real: recorte externo dos cantos, nitidez em escala
fracionária, sobreposição de superfícies GTK, vídeo e foco ao alternar monitores.
Capturas offscreen não comprovam esses comportamentos.

## Prioridades e ordem de execução

P0: crash, perda de dados, ação no alvo errado ou impedimento de usar o navegador.
P1: acabamento e usabilidade obrigatórios da 1.0. P2: melhorias posteriores que
não devem interromper a estabilização. Todos os P0 e P1 precisam ser encerrados.
Esforço P/M/G é relativo ao projeto, sem compromisso de prazo antes da validação
gráfica inicial.

| Etapa | Entrega | Dependência | Esforço |
| --- | --- | --- | --- |
| E0 | Base gráfica confiável e regressões dos crashes | RC2 | M |
| E1 | Medidas, cores, tipografia, ícones e estados comuns | E0 | M |
| E2 | Navegação entre ferramentas, teclado e foco | E0; componentes de E1 | G |
| E3 | Moldura, barra de endereço, abas, pastas e espaços | E1 + E2 | G |
| E4 | Configurações e personalização completas e coerentes | E1 + E2 | G |
| E5 | Bibliotecas, downloads, busca e mensagens | E1 + E2 | M |
| E6 | Auditoria cruzada de acessibilidade, temas e desempenho | E3 + E4 + E5 | M |
| E7 | Candidata final, pacote e release estável | E6 | M |

Cada etapa deve chegar a uma alteração revisável com evidência antes da próxima.
Testar comportamento alterado; não criar testes que apenas repitam constantes
de estilo. Repetir a rodada completa no fechamento da candidata ou após mudanças
que atravessem a integração GTK/Qt.

## E0 — Estabilidade antes do acabamento

- [ ] Rodar a suíte gráfica com Xvfb e ZENITH_REQUIRE_DISPLAY=1; registrar quais
  testes foram executados e qualquer skip remanescente. Regressões de clipboard,
  clique do meio, ferramentas e foco precisam executar efetivamente.
- [ ] Repetir 20 ciclos de nova aba → colar texto/URL → navegar → fechar, usando
  páginas locais determinísticas; testar também clipboard vazio e troca de dono.
- [ ] Repetir 20 aberturas por botão do meio, Ctrl+clique e Shift; verificar aba
  de origem, destino, workspace, primeiro/segundo plano e fechamento do emissor.
- [ ] Conferir descarte/restauração, popups relacionados e tela dividida depois
  dessas ações. Manter o teste que confirma ausência de WebView em abas lazy.
- [ ] Registrar baseline de capturas, latência das ações e memória da árvore de
  processos em perfil de teste. Preservar a RC2 como comparação e rollback.

Aceite: nenhuma queda, erro de memória detectado ou perda de estado nos fluxos
acima. Uma falha gráfica mantém o item aberto, mesmo com a suíte offscreen verde.

## E1 — Base visual comum

- [x] Centralizar medidas compartilhadas entre QSS e código Qt: espaços 4/8/12/
  16/24/32, margens de leitura, larguras de campos e alturas de controles.
  Desvios devem servir a uma função e ficar documentados.
- [x] Definir papéis de cor: conteúdo, lateral, superfície de edição, borda,
  texto principal/secundário, seleção, foco, erro e ação destrutiva. Auditar claro,
  escuro e cada destaque; metas de contraste do projeto: 4,5:1 para texto comum
  e 3:1 para ícones funcionais e indicadores de foco.
- [ ] Consolidar tipografia: títulos, seções, campos e ajuda; respeitar 11–18 px
  na interface sem trocar fontes ou cores dos documentos web.
- [ ] Usar uma família coerente de ícones vetoriais distribuída com o aplicativo,
  com origem/licença registradas. Padronizar grade, traço, baseline e estados;
  eliminar substituições parciais que misturam estilos conforme o desktop.
- [ ] Padronizar normal, hover, pressionado, selecionado, desabilitado e foco.
  Foco deve aparecer quando usado pelo teclado, sem parecer uma seleção permanente.
  Ícone funcional sempre tem nome acessível e tooltip.
- [ ] Usar áreas clicáveis de aproximadamente 32 px nos controles compactos;
  fechar aba deve ter área maior que o desenho de 16–18 px, sem invadir o título.

Aceite: componentes comuns têm o mesmo alinhamento e linguagem em todas as telas;
nenhum controle depende só de cor para comunicar seu estado.

## E2 — Navegação interna e foco

- [x] Dar identidade às rotas internas e guardar estado leve de origem, seleção,
  busca e rolagem. Configurações → Downloads → Voltar deve recuperar configurações
  no mesmo ponto, seguido de retorno à página web de origem.
- [ ] Definir contrato único para Voltar, Esc, Ctrl+W e botão de retorno. Fechar
  uma ferramenta não pode fechar ou navegar uma aba web escondida.
- [ ] Preservar foco de origem ao fechar um formulário, busca ou menu. Invalidar
  callbacks de foco atrasados ao trocar de rota, aba, janela ou perfil.
- [ ] Centralizar a definição dos comandos para menu, atalhos e botões; impedir
  duplo disparo entre Qt e GTK. Garantir Ctrl+C/V/X/A/Z, Enter e Tab nos campos.
- [x] Limitar a vida/cache das páginas internas. Preservar estado não pode
  acumular widgets, consultas pendentes, conexões ou WebViews escondidas.

Aceite: completar o fluxo de ferramentas só com teclado, incluindo retorno;
abrir/fechar as telas 20 vezes não deixa janelas extras nem crescimento contínuo
de objetos depois do processamento das exclusões pendentes.

## E3 — Janela, navegação e organização

- [ ] Unificar cantos, recuo e bordas do frame com a superfície de conteúdo.
  Conferir normal/maximizada/tela cheia e transições de tema sem faixas claras,
  cortes de conteúdo ou moldura duplicada. Respeitar a decoração do compositor.
- [ ] Alinhar verticalmente os ícones e o texto da barra de endereço. Reorganizar
  controles secundários em largura reduzida, preservando o campo de endereço.
  Indicador de conexão deve explicar TLS/exceção em linguagem clara, sem afirmar
  que um site é confiável apenas por usar HTTPS.
- [ ] Nova aba com um caminho de entrada claro: foco inicial na barra de endereço;
  campo da página pode receber foco por clique, sem disputa com timers. Enter,
  colagem e sugestões precisam seguir a mesma resolução de URL/pesquisa.
- [ ] Ajustar centro óptico e largura útil da nova aba; remover repetição como
  “Espaço · Espaço 1”; preservar a busca útil em janelas pequenas.
- [ ] Uniformizar densidade das linhas de abas, pastas e essenciais. Reservar
  posição para favicon, texto, áudio/descarte e fechar; manter título legível.
  Nomes de espaços devem usar a largura real da lateral, sem o corte fixo de 156 px.
- [ ] Tornar o alvo de ações explícito: renomear, mover e excluir devem operar
  sobre o item selecionado, com identidade preservada até a confirmação.
- [ ] Permitir excluir qualquer espaço vazio selecionado, preservando ao menos
  um espaço. Para espaço ocupado, oferecer transferência de abas ou cancelar;
  nunca descartá-las implicitamente. Migrar referências de abas/pastas/sessão e
  testar reordenação, exclusão intermediária e restauração.
- [ ] Levar criação/renomeação de pastas para formulário interno; revisar drag
  and drop, rolagem até o alvo e alternativa por teclado/menu.
- [ ] Fazer o modo compacto preservar acesso previsível a endereço, abas e saída;
  conferir que a lateral não fecha durante uma operação de arraste ou edição.

Aceite: 1/2/16 espaços, nomes longos e acentuados, 0/1/30 abas, pastas vazias e
cheias, estados de áudio/descarte e split funcionam sem alvo ambíguo ou perda de abas.

## E4 — Configurações e personalização

- [x] Montar inventário das preferências de usuário, com chave, padrão, controle,
  validação, persistência e momento do efeito. Toda preferência implementada deve
  ter localização única; ferramentas relacionadas podem ser atalhos para a rota.
- [x] Repartir Geral: separar navegação/pesquisa, downloads, aparência, espaços,
  privacidade/segurança, desempenho e perfis. Bibliotecas e ações avulsas devem
  ficar contextualizadas, evitando uma coluna extensa de botões em Geral.
- [x] Exibir tema, destaque e densidade antes da prévia em telas pequenas;
  torná-la secundária/expansível. Em 760×520, os seletores de tema e destaque
  precisam estar acessíveis sem rolar a página inteira.
- [x] Aplicar divulgação progressiva a busca personalizada e opções avançadas.
  Reduzir espaços de labels de status vazios e alinhar rótulos/campos em colunas
  quando couber; empilhar quando necessário.
- [x] Busca nas configurações deve encontrar rótulos, sinônimos e preferências
  recolhidas, abrir o grupo correto e levar foco/rolagem ao resultado. Limpar a
  busca preserva a categoria e não modifica valores.
- [ ] Oferecer presets de densidade e restauração por seção. Personalizar mantém
  valores atuais e persiste após reiniciar. Restaurar preferências deve indicar
  o escopo e preservar favoritos, credenciais e dados de navegação.
- [ ] Explicar efeitos imediatos, após recarregar ou após reiniciar. Tema dos
  sites é separado do tema do navegador; cor personalizada informa ajustes de
  tom necessários ao contraste. Prévia deve corresponder ao resultado aplicado.
- [ ] Atualizar todos os controles a partir do estado vigente, inclusive perfil,
  tema do sistema e listas de bloqueio. Mudança programática não pode emitir
  uma nova alteração do usuário nem escrever valores antigos.

Aceite: alterar → conferir efeito → trocar de tela → voltar → reiniciar preserva
o valor e o efeito. Cada controle passa por teclado, validação, busca e reset.

## E5 — Bibliotecas, downloads, busca e feedback

- [ ] Dar às páginas internas cabeçalho, busca, ações de seleção, conteúdo e
  retorno consistentes. Ações indisponíveis devem explicar o motivo quando útil.
- [x] Histórico/favoritos: preservar seleção/rolagem durante atualização, mostrar
  endereço e contexto necessários à identificação, abrir no destino escolhido,
  editar inline e distinguir lista vazia, busca vazia, carregamento e falha.
  “Limpar histórico” deve deixar explícito seu alcance, mesmo com busca ativa.
- [ ] Downloads: mostrar progresso real, estado e arquivo; permitir abrir arquivo
  concluído e pasta, cancelar o ativo e reconhecer arquivo removido ou falha.
  Transferências que continuam após fechar a tela precisam de indicador discreto.
- [ ] Ctrl+F: foco correto, retorno à página, busca sem resultado, resultado em
  andamento, Enter/Shift+Enter/F3, troca de aba e navegação durante busca. Avaliar
  índice/total e opções de correspondência apenas se o motor fornecer dados corretos.
- [ ] Formulários: Enter confirma a ação esperada, Esc cancela a edição local,
  erro aparece junto ao campo, sucesso não exige fechar uma janela informativa.
  Confirmações destrutivas devem mencionar o alvo e manter cancelamento seguro.
- [ ] Credenciais/permissões: origem e perfil explícitos; estado privado visível;
  senha não aparece em listas, tooltips ou logs. Preservar as proteções existentes.

Aceite: caminhos de sucesso, cancelamento, falha e ausência de dados têm próximo
passo claro; nenhuma biblioteca ou formulário comum vira janela de navegação extra.

## E6 — Matriz de validação e proteção de desempenho

| Dimensão | Casos obrigatórios |
| --- | --- |
| Janela | 760×520, 1024×768, 1280×800, 1920×1080; normal, maximizada, tela cheia |
| Escala | 100%, 125%, 150%, 200%; mudança de monitor quando disponível |
| Aparência | Claro/escuro/sistema; quatro destaques e cor personalizada; fonte 11/13/18; lateral 180/236/320 |
| Conteúdo | Títulos longos, português acentuado, URL longa, vazio, carregando, erro, seleção e desabilitado |
| Entrada | Mouse, botão do meio, teclado completo, clipboard vazio/texto/URL; foco vindo do GTK |
| Organização | 1/2/16 espaços; 0/1/30 abas; pastas, essenciais, split e abas descartadas |
| Persistência | Reinício, sessão restaurada, perfil privado e outro perfil; migração da RC2 |
| Plataforma | CI Xvfb para integração; sessão real X11/XWayland para compositor, escala, vídeo e monitores |

Executar uma combinação base em todas as telas; nos fluxos de risco, cruzar
janela mínima + fonte máxima + lateral máxima e tema/escala. Não é necessário
executar o produto cartesiano completo, mas cada caso precisa ter evidência e
responsável pela conferência. Registrar N/A com motivo e impacto.

- [ ] Tab/Shift+Tab, foco visível, nomes acessíveis, seleção e erros sem depender
  só de cor; conferir navegação com tecnologia assistiva no ambiente suportado.
- [ ] Capturas de referência por tela e tema, mais uma rodada de uso real. Um
  teste geométrico verde não substitui a inspeção de composição e leitura.
- [ ] Verificar temas de páginas e renderização em diferentes tipos de site,
  formulários, PDFs, imagens e vídeo, sem impor filtros CSS globais ao conteúdo.
- [ ] Medir abertura de menu/rota, troca de aba/espaço e busca da biblioteca no
  mesmo equipamento e perfil da baseline. Registrar mediana e dispersão; revisar
  qualquer regressão reprodutível antes de fechar a candidata.
- [ ] Medir memória da árvore inteira em três rodadas iguais: iniciar, cinco abas
  de referência, dez abas vazias, abrir/fechar ferramentas 20 vezes e retornar ao
  mesmo estado. Separar custo da interface, motor, caches e processos auxiliares.
  Não usar somente o RSS do processo principal nem prometer um total sem medição.
- [ ] Confirmar invariantes: telas internas não criam WebViews; abas vazias/lazy
  não materializam o motor antes da necessidade; listas não criam um widget por
  registro; objetos fechados não crescem continuamente entre ciclos.

## E7 — Fechamento e publicação

- [ ] Encerrar todos os P0/P1 com evidência; descrever limitações remanescentes.
- [ ] Gerar candidata final e testar instalação/atualização, atalho e abertura de
  URLs no pacote Ubuntu 24.04 x86_64. Preservar dados e um caminho de rollback.
- [ ] Rodar testes de unidade, gráficos, smoke e sanitizadores no commit final;
  guardar logs e capturas correspondentes à versão do pacote.
- [ ] Atualizar README, screenshots reais, dependências, atalhos, notas de versão,
  licenças de ícones e instruções de instalação/desinstalação.
- [ ] Alterar o sufixo de versão para vazio somente ao fechar a estável; conferir
  que executável, CPack, documentação e tag dizem 1.0.0.
- [ ] Enviar o commit validado e a tag v1.0.0; o workflow deve produzir TGZ,
  SHA256SUMS e rascunho. Conferir instalação do artefato baixado antes de publicar.

Procedimento detalhado: [RELEASE.md](RELEASE.md). Estado atual dos testes:
[VALIDATION.md](VALIDATION.md). A existência do workflow local não significa que
o GitHub já compilou ou publicou essa versão.

## O que define “pronto”

O lançamento fica liberado quando E0–E7 estiverem concluídas, o pacote final
tiver sido conferido, não houver crash/perda de dados/foco bloqueado nos fluxos
essenciais, todos os P1 visuais estiverem resolvidos e os resultados gráficos
estiverem registrados. A base atual de 36 testes aprovados e 14 pulados é um
ponto de partida, não o aceite da versão estável.

P2 após estabilização: temas compartilháveis, identidade visual por espaço,
customização avançada da nova aba e remapeamento amplo de atalhos. Sincronização,
WebExtensions e novos backends gráficos exigem projetos próprios; não entram
como dependência de acabamento da 1.0.
