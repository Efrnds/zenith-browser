# Segurança e integridade

Esta é uma descrição de políticas e limites do host, não uma certificação do navegador ou do WebKit instalado.

## Ponte de credenciais, a partir de 0.2.5

O script e o canal de mensagens do cofre usam o mundo JavaScript nomeado `zenith-credentials`, com injeção somente no documento principal. O mundo padrão da página deixa de receber o canal do cofre. A API é a de [registro de mensagens em um mundo específico do WebKitGTK](https://webkitgtk.org/reference/webkit2gtk/stable/method.UserContentManager.register_script_message_handler_with_reply.html).

As verificações de origem HTTPS continuam no C++, antes da consulta e depois de escolher uma conta. Também se recusa preenchimento e captura quando o host tem exceção de certificado. A checagem não se limita a confiar no campo `origin` enviado pelo JavaScript. Páginas HTTP e arquivos locais não recebem credenciais dessa ponte.

Eventos `submit` sintéticos de `dispatchEvent` são ignorados. Isso não substitui validação de origem nem garante que todo envio foi iniciado por uma pessoa. A consulta ao cofre só é solicitada quando existe um campo de senha; o foco nesse campo permite tentar novamente em formulários inseridos posteriormente, sem polling ou varredura contínua de DOM.

O prompt para salvar usa texto simples, limita o nome exibido e seleciona Cancelar por padrão. O nome completo e a senha continuam sendo enviados ao cofre somente após confirmação. O conteúdo dos campos preenchidos fica acessível ao site de destino, como em qualquer preenchimento de formulário; o isolamento da ponte não protege um site cujo próprio código esteja comprometido.

## Downloads e exportação

Nomes sugeridos de download usam apenas o componente final do caminho e substituem caracteres de controle, incluindo controles bidirecionais usados para disfarçar extensões. Arquivos não são executados automaticamente. O seletor do sistema continua responsável pela confirmação de substituição do destino escolhido.

A exportação de favoritos usa consulta somente leitura, escape HTML por registro e `QSaveFile` para substituição atômica. O arquivo recebe permissão de leitura/escrita apenas para o proprietário. O destino não pode ser o banco SQLite do navegador nem seus arquivos WAL/SHM. A exportação é uma ação explícita do usuário e contém os endereços favoritos.

## Limites que continuam pendentes

Notificações em 0.2.6: o envio é assíncrono; o host da página é incluído no nome
exibido e o corpo é escapado para os servidores que interpretam markup. O caminho
usa [Notify e CloseNotification da especificação freedesktop](https://specifications.freedesktop.org/notification/latest/protocol.html).
Há limite de 16 envios pendentes, 64 notificações retidas e limpeza em 30 segundos.
Uma notificação antiga não ativa uma aba que navegou para outra origem. O ciclo
real de eventos do desktop permanece sem teste local, inclusive retirada pelo
JavaScript e servidores que não suportam markup no corpo.

- WebKitGTK 4.1 não fornece a origem do frame solicitante para todas as permissões utilizadas aqui. O prompt menciona conteúdo incorporado e as concessões são de uso único; desconhecidas são negadas.
- A revogação de exceções TLS do contexto em execução exige reiniciar o navegador.
- O teste do isolamento JavaScript e a liberação de objetos nativos de download exigem um display e ainda não rodaram neste ambiente. Não equivalem à homologação do cofre real ou à auditoria de segurança de todo o motor.
- A versão e as atualizações de segurança do WebKit são fornecidas pelo sistema operacional.
