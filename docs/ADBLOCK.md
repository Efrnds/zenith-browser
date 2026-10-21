# Bloqueio no Zenith 0.2.3

## Falha corrigida

`decide-policy` só verificava navegações. Imagens, scripts e requisições feitas pelo documento não passavam pelo interceptor. A lista de domínios era carregada, mas não estava instalada como filtro de recursos no processo WebKit. As regras visuais das listas ABP também eram ignoradas; a única folha de bloqueio existente atendia ao YouTube.

## Implementação

O perfil compila regras JSON pelo `WebKitUserContentFilterStore`, uma vez por atualização, e compartilha o filtro com os gerenciadores de conteúdo das abas. O WebKit aplica o bloqueio aos recursos da página. Não se cria um processo auxiliar de bloqueio nem um polling por aba para o Outlook.

Novas navegações e restaurações esperam a compilação inicial. Uma geração de compilação antiga não substitui o filtro mais recente. Atualizar listas ou alternar o bloqueador também atualiza os filtros das abas existentes. Recursos já carregados exigem recarregar a página.

O cache `domains-v2.cache` guarda os nomes dos domínios para gerar os filtros nativos; o antigo cache somente de hashes não fornece esses nomes. As listas internas permanecem disponíveis sem conexão e são incorporadas às atualizações. Downloads de listas continuam em worker; o motor compila o JSON de forma assíncrona.

As regras visuais locais do Outlook usam marcadores publicitários (`data-message-ad-id`, `data-ad-slot`), frames explicitamente intitulados como publicidade e um seletor de espaço publicitário com o ícone `ads-olk-icon.png`. Ficam restritas aos três hosts Outlook. O marcador de mensagens e o seletor do ícone foram conferidos na [lista específica de ocultação do EasyList](https://github.com/easylist/easylist/blob/master/easylist/easylist_specific_hide.txt). Não se oculta texto por conter palavras como anúncio ou promoção, nem se remove a lista inteira de mensagens.

O suporte às listas continua parcial: domínios incondicionais e exceções de domínio, mais regras visuais locais para YouTube e Outlook. Regras ABP com opções ou caminhos não são convertidas em decisões globais. Isso evita que uma exceção de um único site libere um domínio em todos os sites. Não há compatibilidade completa com uBlock/ABP, scriptlets ou importação geral de filtros cosméticos nesta versão.

O contador do menu mede somente navegações rejeitadas pelo interceptor Qt. O filtro nativo não fornece contador de requisições; o rótulo foi corrigido para não apresentar esse número como total de anúncios bloqueados. Erros de compilação são registrados como `Adblock: native filter compilation failed`; um filtro anterior permanece ativo quando disponível.

## Verificação

O teste `compilesNativeAdBlock` usa o compilador real do WebKit e pode rodar sem display. Verifica que o filtro completo interno, incluindo os seletores CSS, é aceito pelo motor instalado.

`blocksSubresourcesAndOutlookSlots` usa servidor HTTP local e WebKit real: um recurso publicitário deve ser barrado antes de chegar ao servidor, um script normal deve carregar, um espaço publicitário deve sumir e uma mensagem comum deve continuar visível. Também cobre desligar/religar e isolamento das regras visuais em outro host. Esse teste depende de X11 e não foi executado no ambiente local sem display funcional.

O Outlook autenticado do usuário não foi acessado nesta execução. A compilação aprovada não confirma o layout real mostrado na captura; a validação final dessa página continua pendente.
