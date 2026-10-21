# Outlook: patch 0.2.1

A captura fornecida mostrou `OwaBasicUnsupportedException` em `outlook.live.com`.
Isso indica que o serviço recusou a identificação do navegador e tentou o cliente
básico. O teste de uma sessão autenticada não está disponível no ambiente do agente;
o diagnóstico de identificação é uma hipótese apoiada pelo erro e pelo upstream.

O WebKit mantém uma exceção de plataforma macOS para `outlook.office.com` e
`outlook.office365.com`, mas sua lista consultada não inclui `outlook.live.com`.
Referências: https://bugs.webkit.org/show_bug.cgi?id=282624 e
https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/glib/UserAgentQuirks.cpp.

O Zenith agora usa uma identificação Safari 18.5/macOS nesses três hosts HTTPS
exatos. Cada view recebe seu próprio WebKitSettings: mudar a identificação de uma
aba não altera as demais. O padrão é restaurado ao navegar para outro host;
restauração de sessão, voltar e avançar aplicam a seleção antes do carregamento.
Subframes pertencem ao contexto da página; não há identificação independente por frame.
Nenhum cookie, cache de login ou credencial foi excluído. Não foi desativado adblock
nem relaxado TLS como tentativa de contornar a falha.

Validação: build Release e CTest passaram. Foi adicionado teste de seleção dos
hosts e rejeição de domínios semelhantes, além de teste gráfico de isolamento e
restauração das configurações. O teste gráfico depende de X11 e foi pulado aqui.
A confirmação final exige reabrir o Zenith e acessar o Outlook na sessão do usuário.
