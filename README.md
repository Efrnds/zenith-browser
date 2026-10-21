# Zenith Browser

Navegador Linux em C++20, Qt Widgets e WebKitGTK, com abas verticais, pastas,
workspaces e split view. Versão 0.3.1 em desenvolvimento. O host gráfico usa
X11 ou XWayland; não há port nativo Wayland, Windows ou macOS neste executável.

Atualizações recentes: controles reorganizados, menu principal agrupado e espaços
no rodapé, temas configuráveis e criação visível de até 16 espaços,
preferência de tema dos sites e suavização de texto em [Aparência](docs/APPEARANCE.md).
Também inclui compatibilidade de identificação no Outlook, descarte e
cache reduzido, filtro nativo de recursos do WebKit, sugestões assíncronas e
atualização de listas com limites e preservação das regras em caso de falha.
Detalhes e validações pendentes estão em [docs/ROADMAP.md](docs/ROADMAP.md),
[docs/MEMORY.md](docs/MEMORY.md), [docs/ADBLOCK.md](docs/ADBLOCK.md) e
[docs/SECURITY.md](docs/SECURITY.md).

## Compilar

Dependências: CMake 3.22+, compilador C++20, Qt 6.4+ (Widgets, Svg, Network,
Concurrent, DBus, Sql e Test), WebKitGTK 4.1 versão 2.40+, X11, libsecret e pkg-config.
O driver Qt SQLite precisa estar instalado. Multimídia depende dos plugins
GStreamer disponíveis na distribuição.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/zenith-browser
```

Em máquinas sem display acessível, os testes internos usam Qt offscreen e os
testes WebKit são explicitamente pulados. Para exigir integração gráfica:

```sh
dbus-run-session -- xvfb-run -a env ZENITH_REQUIRE_DISPLAY=1 ctest --test-dir build --output-on-failure
```

## Uso

```sh
./build/zenith-browser https://example.org localhost:8080
./build/zenith-browser --private
./build/zenith-browser --profile trabalho
./build/zenith-browser --set-default-profile trabalho
```

Cada perfil nomeado tem preferências, sessão, cookies, dados e namespace de
credenciais separados. Uma instância por perfil recebe URLs de novas execuções.
O perfil padrão mantém o caminho usado pelo protótipo anterior.

A janela privada usa contexto WebKit efêmero, banco de dados em memória e não
salva sessão, visitas ou credenciais. Downloads escolhidos pelo usuário continuam
gravando arquivos; preferências e cache das listas de bloqueio podem ser compartilhados.
Não confundir esse modo com anonimato na rede.

| Atalho | Ação |
|---|---|
| Ctrl+L / Ctrl+T / Ctrl+W | Endereço / nova aba / fechar |
| Ctrl+Tab / Ctrl+Shift+Tab | Próxima / anterior |
| Ctrl+Shift+T | Reabrir aba fechada |
| Ctrl+F | Buscar nesta página |
| Ctrl++ / Ctrl+- / Ctrl+0 | Zoom por site |
| Ctrl+H / Ctrl+J | Histórico / downloads |
| Ctrl+D / Ctrl+Shift+B | Salvar / listar favoritos |
| Alt+Esquerda / Alt+Direita | Voltar / avançar |
| Ctrl+R ou F5 | Recarregar |
| Ctrl+P / F11 / F12 | Imprimir / tela cheia / inspector |
| Ctrl+\ | Split view |

O menu reúne histórico, favoritos, importação/exportação HTML, downloads,
permissões, limpeza de dados e workspaces. O menu da aba permite mover,
duplicar, silenciar e fechar outras abas. Abas comuns podem ser arrastadas
para outra posição dentro do mesmo espaço. Pastas podem ser reordenadas pelo menu ou arrastadas.

## Dados e recuperação

`session.json` é versionado, tem limite de tamanho e gravação atômica;
`session.json.bak` mantém o snapshot válido anterior. A primeira abertura migra
a sessão antiga do QSettings. O banco SQLite guarda visitas, favoritos, zoom
e bloqueios de permissões. Segredos ficam no cofre do sistema.

Permissões conhecidas em HTTPS usam consentimento por solicitação; bloqueios
podem ser persistidos e removidos. As APIs WebKitGTK usadas aqui não expõem a
origem do frame em todas as solicitações; por isso não há concessões permanentes
automáticas e o diálogo menciona conteúdo incorporado. Pedidos desconhecidos são negados.
Exceções TLS novas valem pela sessão. Limpar exceções antigas exige reiniciar,
porque o WebKit não oferece revogação em um contexto já criado.

O descarte por inatividade e a meta de páginas carregadas são opcionais em
Configurações > Performance. Formulários, frames, mídia/captura e downloads
impedem descarte automático nos casos detectados. Isso não substitui salvar
trabalho no site. A meta pode ser ultrapassada por páginas protegidas.

## Instalar e empacotar

```sh
cmake --install build --prefix "$HOME/.local"
cd build
cpack -G TGZ
```

O pacote contém executável, ícones e arquivo .desktop; depende das bibliotecas do
sistema e não é um bundle autônomo. A opção de navegador padrão funciona após
a instalação e chama `xdg-settings` por ação do usuário.
Para reverter, reinstale o binário anterior e preserve um backup do perfil antes
de testar versões novas. Não rebaixe o WebKit para contornar falhas do navegador.

## Validação e limites

[Roadmap e pendências](docs/ROADMAP.md), [resultados de validação](docs/VALIDATION.md).
Há instrumentação PSS/RSS em `tools/memory-snapshot.py PID`; os números de consumo
relatados para Zenith e Zen ainda não são um benchmark controlado.

Retomada de downloads, sincronização, extensões, DRM, hierarquias completas de
favoritos importados e ports para outros sistemas não têm paridade implementada.
Importação HTML conserva favoritos existentes, permite desfazer a importação na
confirmação final e converte pastas importadas em grupos simples.
