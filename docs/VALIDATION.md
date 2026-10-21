# Validação 0.2.0

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
