# Validação do Zenith

## Verificação estável gráfica e CI — 22/10/2026

A versão `1.0.0` passou na sessão gráfica de Eduardo (`20261022-111718`):
72/72 testes sem skips, matriz de escala completa, TGZ estável empacotado e
smoke test do executável empacotado aprovados. SHA-256 do TGZ:
`8f0b24c00c929cdf129cc8cd1e17f661fcb29fc5a7fc268ccdd71f78b30be878`.
As quatro conferências manuais listadas no JSON continuam fora da validação
automatizada.

No Actions, Release e Debug compilaram no Ubuntu 24.04, mas os processos
`zenith-core-tests` terminaram com sinal 5 (`-5`) na suíte única. O workflow
passou a executar cada caso Qt num processo separado. O modo isolado passou
localmente em offscreen (56 aprovados, 0 falhas, 16 skips esperados); falta o
resultado do próximo run no Ubuntu para confirmar o contorno lá.

## Correções após a falha gráfica — 22/10/2026

A tentativa gráfica de 11:05 (`~/.cache/zenith-validation/20261022-110556`)
executou 67 testes: 62 passaram e cinco falharam. A investigação identificou
que o benchmark fechava as abas de conteúdo ao tentar fechar dez abas vazias;
outros problemas eram a tela interna não assegurar sua relação de filha com a
pilha, o teste responsivo depender de resize do gerenciador de janelas e três
asserções lerem o título antes da notificação do WebKit.

O benchmark agora seleciona a última aba vazia antes de fechá-la; a pilha
reparenta ferramentas como widgets internos; e os testes de layout e título
foram ajustados. Uma validação Release sem display passou com **56 aprovados,
zero falhas e 16 skips**; a mesma rodada passou em ASan/UBSan quando executada
sozinha. A validação gráfica seguinte está registrada abaixo.

## Candidata RC4 validada graficamente — 22/10/2026, 11:11

Eduardo executou a validação em X11 depois das correções: **72 testes passaram,
zero falhas ou skips**; as escalas 125%, 150% e 200% passaram com 4/4 testes
cada. O smoke test da janela privada passou. O TGZ `ZenithBrowser-1.0.0-rc.4-Linux.tar.gz`
foi extraído e passou na verificação de versão e no smoke test. SHA-256:
`8dd21afc3d3e60fa87eeb1e89176a6fa9f9c49c4e1d7589ba8c2db8618506088`.

O benchmark de três rodadas registrou PSS mediano de 432.341 KiB com cinco
páginas locais, 432.333 KiB após abrir dez abas vazias e 429.484 KiB depois de
usar as ferramentas internas. A latência mediana foi 2,28 ms para abrir/fechar
telas internas e 0,10 ms para trocar abas. A medição inclui o processo QtTest e
páginas locais determinísticas; serve como medida dessa carga de teste, não
como promessa de memória para sites autenticados.

A validação técnica da RC4 passou. Ainda faltam as conferências manuais de
compositor/monitores, tecnologia assistiva, reprodução de vídeo e atualização
sobre uma instalação com dados existentes. O relatório mantém
`release_ready: false` até essas conferências e a validação da versão estável.

## Build estável local — 22/10/2026

O sufixo RC foi removido do padrão e dos caches Release/ASan. Ambos os builds
reportam `Zenith Browser 1.0.0`. As validações offscreen passaram com 56 testes
aprovados, zero falhas e 16 skips em cada build. O TGZ estável foi gerado e
sua versão conferida; SHA-256 `b204180810dcfdbd06be936b8caffc28efb13659c225a69fc40dfaa5ceb75707`.
O smoke test do pacote não iniciou porque esta execução está sem display X11;
ele deve ser repetido junto à validação gráfica estável.

## Candidata 1.0.0-rc.4 — fechamento local

Release e ASan/UBSan: **56 aprovados, 0 falhas, 16 pulados** em cada rodada
offscreen; a validação gráfica mais recente passou com 72/72, sem skips.
Além da suíte base a 100%, a matriz Qt passou a 125%, 150% e 200%, com quatro
aprovações e nenhum skip em cada escala adicional e em cada compilação.
Detecção de vazamentos desativada (`ASAN_OPTIONS=detect_leaks=0`).

As capturas cobrem oito categorias em claro/escuro, 760×520, 1024×768,
1280×800 e 1920×1080, além de fonte de 18 px e lateral de 320 px. Bibliotecas
foram conferidas em 428 px de conteúdo com fonte ampliada, com ações em mais
de uma linha. São testes de geometria e capturas Qt; não homologam o compositor.

Novos testes verificam paginação e seleção de bibliotecas, resposta antiga de
busca, abrir em segundo plano, edição por Enter, falha de leitura, cancelar e
confirmar a limpeza de todo o histórico com busca ativa; foco entre rotas;
busca de preferências com retorno à posição anterior; inventário e reset pela
interface; contraste dos papéis de cor; ações acessíveis das abas e tooltips
que exibem títulos/nomes como texto em vez de interpretar HTML.

O teste de importação também confere que desfazer remove apenas os favoritos
adicionados, mantendo os anteriores. O resultado visual da importação e os
avisos temporários do host ainda dependem da integração gráfica da janela.

Os quinze skips incluem o novo benchmark gráfico. Ele prepara três rodadas
de memória da árvore inteira e latência com cinco páginas locais, dez abas
vazias e vinte ciclos de ferramentas; não há números de desempenho novos
aprovados neste ambiente. Uma tentativa com display obrigatório falhou com
`ZENITH_REQUIRE_DISPLAY: X11 unavailable`, conforme esperado para este sandbox.

Evidências finais: `~/.cache/zenith-build/rc4-release-validation/` e
`~/.cache/zenith-build/rc4-sanitize-validation/`. Cada pasta contém JSON, JUnit,
logs e capturas por escala. O procedimento está em [RELEASE.md](RELEASE.md).
O conector GitHub recusou criar a branch com HTTP 403; sockets e DNS locais
continuam bloqueados. Não houve envio nem publicação remota.

## Execução do acabamento — 22/10/2026

Release e ASan/UBSan: **44 aprovados, 0 falhas, 14 pulados** na rodada local
após a navegação interna e reorganização das preferências. Os testes novos
cobrem retorno sem perder campos, deduplicação de rotas, limite de seis telas,
liberação de widgets, restauração restrita à categoria e transferência de
abas/pastas/abas fechadas ao excluir um espaço intermediário.

As oito categorias foram capturadas nos dois temas, em 1280×800 e 760×540,
mais fonte de 18 px com lateral de 320 px. A inspeção conferiu o primeiro plano
de Aparência e a presença dos controles antes da prévia recolhida.

O ambiente local nega sockets gráficos: KWin virtual isolado falhou com
`bind(): Operation not permitted`, inclusive com runtime privado em /tmp.
Isso não é aprovação dos testes GTK. A integração adiciona vinte ciclos de
nova aba/clipboard/fechamento e retorno Configurações → Downloads → Voltar.
O CI guarda logs e capturas por configuração para conferir o motor real.

Não há resultado novo de memória de toda a árvore, monitores/escala fracionária,
vídeo, instalação Ubuntu ou uso assistivo. Esses critérios continuam abertos.

A RC3 também cobre contraste de cores personalizadas saturadas nos dois temas,
nomes de espaços em diferentes larguras, arquivo de download removido e
coerência entre os atalhos exibidos no menu e o despacho de comandos.

## Candidata 1.0.0-rc.2

Revalidação em 22/10/2026: Release e Debug com ASan/UBSan passaram com
**36 aprovados, 0 falhas e 14 pulados** em cada rodada. Detecção de vazamentos
desativada (`ASAN_OPTIONS=detect_leaks=0`). Os testes gráficos dependem de X11
e não foram executados neste ambiente, incluindo o novo teste de clique do meio.
O símbolo de proteção também foi conferido na tabela dinâmica do executável.

O core local do crash de 21/10/2026 mostrou uma desreferência nula no callback
de formatos de clipboard do WebKitGTK 2.54.1: GTK entregou ponteiro nulo e
contagem `-1`. O endereço da chamada seguinte corresponde a `gdk_atom_name`.
O [código da versão instalada](https://github.com/WebKit/WebKit/blob/webkitgtk-2.54.1/Source/WebKit/UIProcess/gtk/ClipboardGtk3.cpp)
percorre essa contagem sem tratar a falha antes de construir o span.

`GtkClipboardGuard` normaliza respostas nulas ou com contagem não positiva para
uma lista vazia, preservando respostas válidas. A proteção é exportada pelo
executável Linux para envolver `gtk_clipboard_request_targets`; resolve a função
original com `RTLD_NEXT`. Nenhuma biblioteca do sistema é alterada.

Clique do meio e Ctrl+clique em links passam por uma solicitação Qt enfileirada,
criando uma aba em segundo plano sem WebView até a ativação. Shift solicita
primeiro plano. Popups relacionados mantêm o fluxo próprio do WebKit.
O encaminhamento de teclas também conserva uma cópia do callback durante sua
execução, pois mudanças de foco podem substituir o callback global.

Os testes de regressão cobrem resposta inválida síncrona e assíncrona do GTK,
preservação dos formatos válidos, exportação do símbolo e Ctrl+V nos campos
nativos. A integração gráfica acrescenta abertura repetida de nova aba com
colagem e clique do meio com preservação da página de origem e aba lazy.

## Candidata 1.0.0-rc.1

Release e Debug com ASan/UBSan: **34 aprovados, 0 falhas, 13 pulados**.
Os 13 testes pulados exigem X11/WebKit. Incluem a nova integração da janela
principal, que abre e fecha bibliotecas repetidamente e verifica Ctrl+F em uma
página local, sem janela modal. Esse teste foi adicionado ao CI gráfico, mas não
foi executado neste ambiente sem display acessível.

Os testes executados verificam que a página de downloads é filha do navegador,
preserva seleção durante atualização e mantém nomes de arquivos existentes.
Também verificam a barra de busca embutida, cancelamento de seu timer, fonte e
dimensões configuráveis, cor personalizada, zoom padrão com preservação do zoom
por origem, edição interna de espaços e validação da busca personalizada.
As capturas Qt cobrem as seis páginas em claro/escuro e janelas estreitas.
Também foi verificada a janela de 760×540 com fonte de 18 px e lateral de
320 px, sem corte dos rótulos de checkboxes nas seis categorias.

O YAML do workflow de release e a sintaxe Bash dos seus passos foram conferidos
localmente. O workflow e a integração gráfica ainda precisam passar no GitHub.
Não foi criada tag, enviada alteração remota nem publicada Release nesta revisão.
Critérios e limitações da candidata em [RELEASE.md](RELEASE.md).

## Atualização 0.4.0

Release e Debug com ASan/UBSan: **31 aprovados, 0 falhas, 12 pulados**.
Detecção de vazamentos desativada (`ASAN_OPTIONS=detect_leaks=0`).
Os testes pulados exigem X11/WebKit, incluindo a nova verificação de adiamento
de WebViews nas abas vazias e da ausência de decoração e borda no GtkPlug.

Testes Qt verificam navegação e pesquisa da nova aba, rejeição de URL de script,
acesso às bibliotecas, ativação de espaços por Enter e geometria dos controles
e busca por preferências, ausência de resultados, navegação compacta e atualização
de controles sem emitir mudanças nas preferências. A geometria é conferida
nas seis páginas de configurações. Foram capturados os temas claro e escuro em
1280×800 e 760×540; o teste verifica ausência de corte horizontal dos seletores,
largura dos checkboxes e cor do canto da moldura Qt. A seleção persistente no
gerenciador de downloads continua preservada após atualizações.

A inspeção visual corrigiu largura da busca, corte e junção das linhas de
detalhes, setas dos campos numéricos e acabamento das seções. Nova aba, páginas
de configurações e downloads foram conferidos em capturas de widgets reais.
As capturas usam fixtures de teste. Detalhes e direção antislop em
[APPEARANCE.md](APPEARANCE.md).

Não foi possível homologar a decoração externa, a maximização nem o motor
embutido em uma sessão gráfica. A preferência de moldura enviada ao gerenciador
de janelas é uma solicitação; o compositor decide o recorte externo dos cantos.
Não há nova medição de RAM ou de qualidade de renderização das páginas web.

## Atualização 0.3.1

Release e Debug com ASan/UBSan: **28 aprovados, 0 falhas, 11 pulados**.
Detecção de vazamentos desativada (`ASAN_OPTIONS=detect_leaks=0`).
Os testes pulados continuam exigindo uma sessão GTK/X11 acessível.

Novo teste cobre organização e emissão dos comandos do menu principal,
Configurações selecionada por teclado, restrições em janela privada, ações da
barra de endereço, restauração de busca personalizada, nomes acessíveis dos
espaços, navegação até o 16º espaço, renomeação contextual e nova aba após
reconstruir a lista. Foram verificadas as traduções de Ctrl+, e F10 no despacho
de comandos; o caminho nativo GDK de F10 foi implementado, mas não exercitado
sem X11. Foram inspecionadas capturas claras/escuras em 1280×800 e 760×540,
do menu principal e do menu do espaço.

O botão lateral e o menu de preferências duplicado foram removidos. O mapa de
funções e a avaliação visual estão em [APPEARANCE.md](APPEARANCE.md).
Não houve medição nova de RAM, nem alteração do renderizador das páginas.
Não se declara homologação integral GTK a partir dos testes Qt offscreen.

## Atualização 0.3.0

Release e Debug com ASan/UBSan: **27 aprovados, 0 falhas, 11 pulados**.
Detecção de vazamentos desativada (`ASAN_OPTIONS=detect_leaks=0`). Os testes
pulados exigem X11/WebKit, incluindo a nova verificação de preferência de tema
e antialiasing. Build e testes não comprovam correção da nitidez das imagens web.

Capturas dos widgets Qt foram inspecionadas em 1280×800 e 760×540, nos temas
claro e escuro. Seletores de tema e quatro cores, persistência, botão de criar
espaço e limite de 16 espaços foram exercitados. Contraste de texto secundário
≥4,5:1 sobre canvas, sidebar e seleção; destaque ≥4,5:1 nos campos e bordas
de controles ≥3:1 foram verificados automaticamente nos dois temas.

Direção, decisões visuais e limitações do gate antislop estão em
[APPEARANCE.md](APPEARANCE.md). A conferência de páginas reais e o gate gráfico
integral seguem pendentes por indisponibilidade de X11 nesta execução.

## Atualização 0.2.6

Release e Debug com ASan/UBSan: **26 aprovados, 0 falhas, 10 pulados**.
A detecção de vazamentos continua desativada (`ASAN_OPTIONS=detect_leaks=0`).
Cobertura nova executada: reabertura na posição original com pasta/mudo, fallback
para raiz após excluir pasta, limite de 8 MiB sem perder os endereços das doze
abas de teste, recusa de estado individual acima de 1,5 MiB, agrupamento de 50
mudanças na sidebar, correspondência entre a ordem do modelo e as linhas reais
da sidebar após reordenar pastas e mapeamento dos novos atalhos.

O teste novo de retomada de busca após fechar é WebKit/X11 e foi pulado. Não há
teste local de recarga contra servidor com cache, do ciclo de notificações DBus
do desktop ou do foco GTK nesses novos atalhos. A implementação assíncrona de
notificações foi compilada; isso não equivale à validação ponta a ponta.
Não foram medidos consumo da árvore de processos nem tempos comparativos.

## Atualização 0.2.5

Release: **22 aprovados, 0 falhas, 9 pulados**. Os novos testes executados cobrem:
recusa de credenciais em HTTP/arquivo/exceção TLS, mapeamento dos atalhos de abas,
remoção de controles bidirecionais em nomes de arquivos, omissão de userinfo na
barra, persistência da seleção do download durante refresh e exportação de 1.200
favoritos com pastas, HTML escapado e proteção contra sobrescrever o SQLite.

O teste de isolamento real da ponte e a asserção de liberação do objeto nativo do
download foram adicionados à integração WebKit, que exige X11. Foram pulados no
ambiente local. A política C++ passou, mas isso não comprova o comportamento do
canal isolado em uma sessão gráfica nem o funcionamento do cofre real.

O caso de seleção de downloads foi executado com Qt offscreen e um gerenciador
somente de histórico, sem contexto nativo. A troca de abas entre janelas e o
fluxo completo da exportação pelo menu ainda precisam de teste no desktop.

A mesma suíte passou em Debug com ASan/UBSan, usando
`ASAN_OPTIONS=detect_leaks=0`. A verificação de vazamentos não foi executada e os
nove casos WebKit continuam pulados também nessa configuração.

## Atualização 0.2.4

Build Release e QtTest: **18 aprovados, 0 falhas, 8 pulados**.
Os testes novos verificaram sugestões assíncronas, limite e deduplicação com 112
registros e prioridade de favoritos; também preservação de regras após falha
parcial de leitura e conteúdo HTML inválido, seguida de atualização válida.
As fontes das listas desse teste são arquivos temporários via QNetworkAccessManager:
não constitui teste de rede HTTP, timeout ou resposta acima de 8 MiB.
O teste de descarte automático continua dependendo de X11 e foi pulado.
Nenhuma medição de redução de consumo ou latência foi feita nesta rodada.

## Atualização adblock 0.2.3

Build Release e CTest passaram: **16 aprovados, 0 falhas, 8 pulados**.
O teste novo executado compilou as regras internas com o compilador nativo do
WebKitGTK 2.54.1, incluindo os seletores do Outlook. O teste de recursos HTTP,
visibilidade de anúncios/mensagens e alternância do bloqueador exige X11 e foi
pulado neste ambiente. A página autenticada do usuário continua sem validação
direta. Detalhes e limites em [ADBLOCK.md](ADBLOCK.md).

## Atualização Outlook 0.2.1

Build Release e CTest passaram: **15 aprovados, 0 falhas, 5 pulados**.
O teste novo executado cobre seleção exata dos hosts Outlook e rejeição de nomes
semelhantes. O novo teste de configurações separadas por view foi pulado por falta
de X11. A sessão autenticada do Outlook ainda precisa ser confirmada no desktop.
Detalhes em [OUTLOOK-COMPATIBILITY.md](OUTLOOK-COMPATIBILITY.md).
A atualização do executável em `dist/` usa substituição atômica, preservando o
binário 0.2.0 para rollback e os dados do perfil. O atalho já aponta para esse caminho.

Os resultados abaixo correspondem à rodada original 0.2.0.

## Build

Executado com sucesso em 21/10/2026:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j 2
```

Inclui navegador e zenith-core-tests. Qt local: 6.12.0; WebKitGTK: 2.54.1.

## Testes

`ctest --test-dir build --output-on-failure` passou. Execução QtTest direta:
**14 aprovados, 0 falhas, 4 pulados**, incluindo inicialização e encerramento da suíte.

Cobertura executada: parser/adblock, comparação de origens HTTPS, resolver de URL,
persistência SQLite, isolamento das permissões por origem/porta, limite de zoom,
backup de sessão corrompida, erro de gravação, ordem/reabertura de abas lazy,
nome de download, barra de endereço, teclado dos workspaces e HTML de favoritos
com pastas, deduplicação e importação transacional sem substituir favoritos existentes;
consulta da biblioteca em thread com conexão SQLite própria e somente leitura.

Os quatro testes WebKit pulados cobrem histórico com voltar/avançar, três ciclos
de descarte, download redirecionado após fechar a view e contexto privado efêmero.
O ambiente anuncia DISPLAY=:0, mas Qt não consegue conectar ao display e Xvfb
não está instalado. O abort inicial era causado por forçar o plugin xcb.
Os testes agora usam offscreen para os componentes independentes do WebKit.
`ZENITH_REQUIRE_DISPLAY=1` faz a execução falhar quando X11 não está disponível;
o workflow gráfico define essa variável e não aceita pular integração silenciosamente.

CI com Debug/Release, Xvfb, smoke de janela e ASan/UBSan foi escrito; não foi
executado remotamente nesta sessão. Testes do cofre real, câmera/microfone,
TLS/consentimento por site, disco cheio e navegação de sites reais permanecem pendentes.

O smoke local de linha de comando confirmou `--help` e versão `0.2.0` sem exigir
display. O perfil padrão e a rejeição de nome inválido foram exercitados com
XDG_CONFIG_HOME temporário, sem alterar preferências reais. Isso não confirma o
funcionamento de uma janela WebKit.

## Release, sanitizers e pacote

Build Release em `/tmp/zenith-release` e CTest passaram. Build ASan/UBSan em
`/tmp/zenith-sanitize` e CTest passaram com `ASAN_OPTIONS=detect_leaks=0`:
detecção de acessos inválidos/UB ativa; detecção de vazamentos não foi executada.
Os quatro testes gráficos continuam pulados nesses resultados locais.

CPack gerou `ZenithBrowser-0.2.0-Linux.tar.gz`; `desktop-file-validate` aprovou o
launcher do repositório e `git diff --check` passou. O pacote depende das bibliotecas
do sistema e não foi instalado como substituto da versão em uso.
O pacote foi extraído em diretório temporário: executável respondeu `0.2.0`, e
launcher, README e documentos de validação/roadmap foram conferidos.
O executável Release e o TGZ foram copiados para `dist/` (ignorado pelo Git).
O atalho existente continua apontando para `/home/eduardo/.local/bin/zenith-browser`,
que não foi substituído durante a homologação pendente.

## Medição

```sh
python3 tools/memory-snapshot.py PID_DO_NAVEGADOR
```

Retorna JSON com RSS/PSS em KiB, processos descendentes e indicador `complete`.
Quando algum processo termina ou /proc nega leitura, os totais são parciais e
`complete` fica falso. A enumeração não é atômica; processos reparentados e serviços
externos não entram. Não usar um snapshot isolado como benchmark.
Não foi medido ganho de RAM nesta entrega.

## Escopo

Atualização 0.2.2 de memória: build Release e CTest passaram em 21/10/2026.
QtTest registrou 15 passes, zero falhas e sete testes pulados por falta de X11,
incluindo os novos testes de proteção de formulários e descarte automático.
Política, concessões e protocolo de medição estão em [MEMORY.md](MEMORY.md).
Não há processos Zenith/WebKit visíveis nesta execução do agente e não foi
possível medir consumo antes/depois. A ocultação GTK e o retorno às abas
precisam de validação na sessão gráfica, assim como os demais testes do motor.

As alterações anteriores do workspace foram preservadas; não foi feito commit,
publicação nem alteração do navegador padrão da máquina.

Antislop aplicado durante o trabalho, com direção baseada na interface existente:
listas utilitárias, cinzas escuros e o mesmo violeta de foco, sem novos assets.
ENERGY 1 / RHYTHM 1 / MOTION 1; opção de reduzir animações disponível.
A sidebar foi renderizada em offscreen, inspecionada visualmente e exercitada por
teclado. Diálogos receberam fundos/campos/foco coerentes com o tema para evitar
texto claro em fundo nativo claro. Os novos menus foram agrupados para limitar altura.
Contraste calculado para os textos explícitos novos: #f4f4f5 sobre #18181b,
16,12:1; #a1a1aa sobre #18181b, 6,91:1; #f4f4f5 sobre #202023, 14,78:1.
A aprovação visual integral está pendente da execução do navegador com GTK;
este registro não declara o gate completo aprovado nem um release homologado.
