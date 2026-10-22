# Empacotamento e suporte por sistema

O backend atual combina Qt 6, WebKitGTK 4.1, GTK 3 e X11/XWayland. O alvo
mantido hoje é Linux x86_64. A condição `WIN32`/`APPLE` no CMake não torna
esses alvos portáveis: a integração do navegador e as chamadas GTK/X11 ainda
são usadas diretamente pelo código compartilhado.

## Linux

- **TGZ:** pacote atual do CPack; usa as dependências instaladas pelo sistema.
- **Flatpak:** o workflow de release constrói e testa um bundle usando
  `org.gnome.Platform` 50. Como esse runtime inclui WebKitGTK e o app depende
  de Qt, Qt Base e Qt SVG 6.10.2 são compilados dentro do Flatpak. O bundle
  ainda depende de baixar o runtime GNOME na instalação.
- **AppImage:** não faz parte dos artefatos de release. O WebKitGTK procura
  seus processos auxiliares em caminhos definidos na compilação. Relocar esses
  arquivos para um AppDir sem quebrar o sandbox e sem depender do WebKitGTK do
  host ainda precisa de uma estratégia testada em mais de uma distribuição.

## Windows e macOS

Ainda não há `.exe` nem `.dmg`. O CMake exige `webkit2gtk-4.1` e X11 sem
condicioná-los ao sistema; `WebView`, áreas GTK incorporadas, clipboard e
atalhos também usam APIs WebKitGTK diretamente. Criar jobs de CI para Windows
ou macOS agora produziria builds que falham ou binários sem navegador.

O port deve introduzir uma interface comum para navegação, downloads, sessão,
perfis, permissões, busca na página e bloqueio de conteúdo, mantendo
implementações específicas por sistema:

- Linux: backend WebKitGTK atual, preservando consumo e arquitetura de
  processos.
- Windows: backend WebView2, com verificação do runtime Evergreen e um pacote
  instalável que não distribua um segundo motor Chromium completo.
- macOS: backend WKWebView via Objective-C++ e integração de `NSView` com Qt.

Cada backend precisa passar navegação, formulários, downloads, adblock,
cookies, credenciais, áudio/vídeo, clipboard, janelas privadas e suspensão de
abas antes de ser anunciado como compatível. O CI só deve publicar `.exe` e
`.dmg` depois que esses testes rodarem nos respectivos runners.
