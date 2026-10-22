# Release 1.0

A fonte local agora está configurada como **1.0.0**; a última candidata RC4
passou na validação gráfica e no pacote extraído.
O TGZ contém o executável e a documentação; depende de Qt, WebKitGTK, libsecret
e X11 instalados. O workflow também gera um Flatpak com o runtime GNOME 50. O
TGZ e o Flatpak do GitHub são compilados no Ubuntu 24.04 x86_64. Veja a matriz
de alvos em [PLATAFORMAS.md](PLATAFORMAS.md).

## Fluxo do GitHub

O arquivo `.github/workflows/release.yml` responde a tags `v*`:

1. Compila Release e confere se a tag coincide com `zenith-version.txt`, gerado
   pelo CMake. Uma tag diferente da versão do código interrompe o fluxo.
2. Roda testes de unidade e integração em Xvfb com `ZENITH_REQUIRE_DISPLAY=1`.
   Isso impede que a ausência de display transforme a rodada gráfica em sucesso.
3. Executa o smoke test em perfil temporário e testes com ASan/UBSan.
4. Gera o TGZ e Flatpak, calcula `SHA256SUMS` e disponibiliza os logs como
   artefato separado.
5. Confere os hashes e cria um rascunho da Release, marcado como pré-lançamento
   quando a tag contém sufixo. Apenas esse último job recebe `contents: write`.

Depois de revisar e enviar o código ao repositório, a candidata pode ser criada:

```sh
git tag -a v1.0.0-rc.4 -m 'Zenith 1.0.0-rc.4'
git push origin v1.0.0-rc.4
```

Esses comandos não foram executados nesta revisão. O workflow foi preparado
localmente; seu resultado só pode ser confirmado depois da execução no GitHub.
O rascunho fica disponível em Releases para conferir os arquivos e publicar.
O comportamento de `--draft`, `--verify-tag` e `--prerelease` segue o
[manual do GitHub CLI](https://cli.github.com/manual/gh_release_create).
As permissões seguem a
[documentação do GITHUB_TOKEN](https://docs.github.com/en/actions/tutorials/authenticate-with-github_token).

## Antes de marcar como estável

O procedimento comum ao CI e à máquina de validação é:

```sh
python3 tools/validate-release.py --build build --package
```

Ele exige os executáveis compilados, preserva DISPLAY/XAUTHORITY e isola
preferências, dados e caches em uma pasta temporária. Executa a suíte sem
aceitar skips gráficos, repete a matriz Qt nas escalas 125/150/200%, faz smoke
test do executável e do TGZ extraído e confere a versão empacotada. O relatório
inclui logs, JUnit, capturas e benchmark de três rodadas com páginas locais.
Memória incompleta não é apresentada como total. O custo do host QtTest está
incluído; esses números não representam cinco aplicações web autenticadas.

O JSON mantém `release_ready: false`: compositor, monitores, uso assistivo,
vídeo e atualização com dados existentes continuam exigindo conferência real.
`--offscreen` é uma rodada de widgets e não substitui esse procedimento.

Executar o [plano de acabamento da 1.0](PLANO-1.0.md), encerrando os itens P0/P1
e registrando as evidências de E0–E7 antes de fechar o commit da versão estável.

- Passar os testes gráficos e smoke test no workflow.
- Conferir ferramentas internas, Ctrl+F, troca de abas e espaços, tela dividida,
  janela pequena, maximizada e tela cheia em uma sessão real X11/XWayland.
- Conferir foco do teclado entre o conteúdo GTK e os formulários Qt.
- Validar sites reais, incluindo Outlook, downloads, logins e restauração de sessão.
- Medir memória do conjunto de processos na mesma carga de abas; não usar
  apenas o RSS do processo principal para afirmar economia.
- Conferir o pacote em Ubuntu 24.04 sem dependências de desenvolvimento.

O valor padrão de `ZENITH_VERSION_SUFFIX` já foi removido e os builds locais
foram reconfigurados para `1.0.0`. Antes da tag, rode a validação gráfica do
build estável abaixo e termine as quatro conferências manuais pendentes.

### Validação local de 22/10/2026

A rodada `~/.cache/zenith-validation/20261022-105440` **falhou**: três
asserções (benchmark de ferramentas, título após restauração, geometria no
compositor) e SIGSEGV na finalização assíncrona do gerenciador de cookies.
Ela não autoriza criar a tag estável.

Foi adicionada retenção do gerenciador de dados até terminar a consulta de
cookies e a liberação de seu GTask, com teste de destruição de oito perfis
durante consultas pendentes. O teste de restauração aguarda a notificação do
título; a janela de teste de layout ignora o gerenciamento externo de janelas.
Na rodada seguinte (`20261022-110556`), o SIGSEGV não se repetiu, mas cinco
asserções falharam. O benchmark estava fechando páginas de conteúdo ao fechar
dez abas vazias: agora seleciona a última aba vazia antes de cada fechamento.
A pilha também força ferramentas a serem widgets internos; o teste responsivo
usa um host controlado e os testes de título aguardam as notificações do WebKit.
Release e ASan/UBSan passaram depois dessas correções em modo sem display
(56 aprovados, zero falhas, 16 skips). A execução ASan que coincidiu com outra
rodada falhou ao comparar uma preferência; a repetição serial passou.
Na validação gráfica RC4 de 11:11 (`~/.cache/zenith-validation/20261022-111132`),
os 72 testes passaram sem falhas ou skips, as três escalas passaram, os dois
smoke tests passaram e o TGZ foi gerado, extraído e conferido. O SHA-256 do
TGZ é `8dd21afc3d3e60fa87eeb1e89176a6fa9f9c49c4e1d7589ba8c2db8618506088`.
O benchmark mediu PSS mediano de 432.341 KiB com cinco páginas locais. Restam
as verificações manuais de compositor/monitores, tecnologia assistiva, vídeo
e atualização sobre uma instalação com dados existentes, além de validar a
versão estável antes de criar a tag.

O build estável local já reporta `Zenith Browser 1.0.0`; a suíte offscreen e
ASan/UBSan foi repetida. O pacote `ZenithBrowser-1.0.0-Linux.tar.gz` foi
gerado e sua versão conferida. O smoke test desse pacote ainda precisa rodar
em X11, porque este ambiente bloqueia o display. Na sessão gráfica, rode:

```sh
python3 tools/validate-release.py \
  --build ~/.cache/zenith-build/rc2-release --package
```

Quando essa rodada e as conferências manuais passarem, salve as alterações em
um commit e envie esse commit antes da tag:

```sh
git push origin main
git tag -a v1.0.0 -m 'Zenith 1.0.0'
git push origin v1.0.0
```

O Actions cria um rascunho somente após os testes. Conferir os artefatos e o
pacote gerado no Ubuntu antes de clicar em **Publish release** no GitHub.

## Escopo desta candidata

Configurações cobrem as preferências de usuário implementadas: pesquisa e sessão,
downloads, integração padrão, perfis e bibliotecas; temas, cor personalizada,
dimensões e fontes; espaços; bloqueio de anúncios, cookies, rastreamento, senhas,
JavaScript, rolagem suave, permissões e limpeza; descarte de abas e minimização.
Decisões internas como isolamento do motor e rejeição padrão de certificados
inválidos continuam sendo proteções do navegador.

Seletores de arquivos, confirmações destrutivas, impressão e ferramentas de
desenvolvedor ainda podem usar janelas próprias. Novas janelas privadas e perfis
são janelas de navegação intencionais. Bibliotecas, configurações e busca ficam
dentro da janela atual. Não há sincronização, extensões WebExtensions,
atualizador automático nem port nativo Wayland nesta versão.
