# Memória no Zenith 0.2.2

## Atualização 0.4.0: nova aba nativa

Uma aba nova `about:blank`, sem histórico e sem view, usa a página inicial Qt.
A criação do WebView é adiada até navegar ou usar divisão de tela. Popups
relacionados e abas com histórico mantêm a materialização necessária ao motor.
As bibliotecas usam pintura de linhas por delegate, sem um widget por registro.
Essa alteração não foi acompanhada de benchmark de PSS/RSS; não há novo valor
de economia de RAM a declarar. O teste com WebKit real continua exigindo X11.

O orçamento deve considerar o processo principal e seus auxiliares WebKit. O RSS somado inclui páginas compartilhadas mais de uma vez; o PSS somado distribui esse custo entre processos. Ambos devem ser registrados, com o número de processos e páginas carregadas.

## Correções

Em 0.2.6, alterações estruturais da sidebar são agrupadas em uma reconstrução por
ciclo de eventos. O estado serializado de navegação é limitado a 1,5 MiB por aba;
as até vinte abas fechadas retêm no máximo 8 MiB desses estados. Estados antigos
são removidos primeiro, mantendo os endereços para reabertura. Ao reabrir uma aba
cujo estado foi removido, o histórico anterior de voltar/avançar não é restaurado.
Esse limite trata a cópia mantida pelo Zenith, não a memória do processo WebKit.

- Abas comuns criadas em segundo plano só criam WebView ao serem ativadas. Popups mantêm a relação com a página de origem.
- A superfície GTK de uma aba estacionada é ocultada, permitindo ao motor reconhecer que ela está em segundo plano.
- Cache de recursos usa `WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER`, intermediário entre o modo navegador e o modo visualizador de documentos. O Page Cache está desativado: voltar e avançar pode precisar recarregar documentos.
- O padrão passa de descarte desativado e páginas ilimitadas para 180 segundos de inatividade e meta de três páginas carregadas. A política roda a cada 15 segundos. Acima da meta, páginas ocultas há pelo menos 30 segundos podem ser descartadas. Preferências explicitamente salvas continuam valendo.
- Campos de busca e formulários intactos deixam de bloquear o descarte. Valores alterados, senhas preenchidas, arquivos selecionados, editores, frames, mídia ativa, captura e permissões pendentes bloqueiam. Downloads ativos suspendem o descarte automático de todas as abas.
- A checagem assíncrona não se sobrepõe para a mesma aba e revalida visibilidade, acesso, URL e identidade da WebView antes do descarte.

A meta não limita processos diretamente nem limita bytes. Abas visíveis e protegidas podem ultrapassá-la. WebKit também pode reter processos em seus próprios pools. Descartar recarrega a página na próxima ativação, preservando URL e histórico, não o estado JavaScript completo. A inspeção de formulários é conservadora e não consegue reconhecer todo estado não salvo de uma aplicação web.

Não se usa `webkit_web_context_set_web_process_count_limit`: a documentação GIR instalada marca essa API como obsoleta e sem efeito desde 2.26 por segurança. Também não se desativa o sandbox nem se força páginas independentes a compartilhar o processo de popups.

## Validação e medição

Build Release e testes automáticos devem passar antes da publicação. Os testes de integração novos verificam campos intactos versus alterados, cache, descarte automático, abas visíveis e meta de páginas. Dependem de X11 funcional. Sem display, são pulados e não constituem evidência de funcionamento real do motor.

Na sessão gráfica, com o PID principal:

```sh
python3 tools/memory-snapshot.py PID > /tmp/zenith-memory.json
```

Comparar as mesmas cinco URLs e mesma conta em 0.2.1 e 0.2.2, em execuções separadas: após carregar, após 60 segundos e após quatro minutos sem trocar de aba. Repetir alternando abas, tocando mídia e editando formulários. Registrar o custo de recarregamento junto da economia. Processos reparentados não entram na árvore; `complete: false` indica dados parciais por permissões ou encerramento de processos.

Nesta execução do agente não há processos Zenith/WebKit visíveis para medir, nem acesso funcional ao display. Não há resultado medido que sustente uma promessa de consumo abaixo de 600 MB ou uma porcentagem de redução.
