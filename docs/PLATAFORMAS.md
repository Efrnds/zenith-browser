# Build e suporte por sistema

O CMake seleciona um backend de navegação nativo por sistema e mantém o
WebKitGTK no Linux, sem compilar nem carregar os outros motores nessa
plataforma.

## Linux

- Requer Qt 6.4+, WebKitGTK 4.1 (2.40+), GTK 3, X11/XWayland e libsecret.
- CPack gera o TGZ. O workflow também valida Flatpak e AppImage antes de
  publicar a release.

## Windows

- Requer Qt 6.4+, Visual Studio 2022 x64, o pacote NuGet
  `Microsoft.Web.WebView2` e o runtime Evergreen WebView2 instalado na máquina.
- Configure com `-DWEBVIEW2_SDK_DIR=<diretório-do-pacote-NuGet>`; o caminho deve
  conter `build/native/include/WebView2.h` e
  `build/native/x64/WebView2LoaderStatic.lib`.
- O workflow baixa o SDK fixado, configura o CMake, compila, executa `--version`
  e um smoke test privado, depois empacota os binários Qt num ZIP.

## macOS

- Requer Qt 6.4+ e Xcode Command Line Tools. O CMake habilita Objective-C++20
  para o backend WKWebView e gera o bundle `.app`.
- O workflow configura, compila, executa `--version` e um smoke test privado,
  incorpora as bibliotecas Qt com `macdeployqt` e empacota um DMG.

## Limites conhecidos dos backends nativos

Os jobs de release devem compilar e iniciar cada backend antes de publicar. O
build não demonstra paridade completa de recursos: em particular, políticas de
privacidade e exceções de certificado ainda precisam ser implementadas e
validadas para Windows/macOS; o fluxo de download no WKWebView também precisa
ser concluído. Não anuncie esses sistemas como equivalentes ao Linux até cobrir
navegação, perfis privados, adblock, downloads, credenciais, áudio/vídeo,
clipboard e suspensão de abas em testes nativos.
