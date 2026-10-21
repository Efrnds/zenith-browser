# Aparência e renderização — 0.3.1

Direção: manter o navegador de abas verticais inspirado no Zen, conforme o
pedido do autor. Grafite ou claro, uma cor de destaque por vez, controles
compactos e conteúdo da página como centro da janela. Noto Sans prioriza
legibilidade em português na interface Linux; fontes dos documentos continuam
sob controle dos sites. ENERGY 2 / RHYTHM 2 / MOTION 1, sem animações novas.

## Organização 0.3.1

A revisão segue o pedido de aproximar a organização do Zen, preservando a
paleta configurável. A sidebar serve à organização de páginas; a barra superior
serve à navegação; preferências persistentes ficam nas configurações. ENERGY 2 /
RHYTHM 2 / MOTION 1. Sem animações ou superfícies web adicionais.

| Função | Acesso principal |
| --- | --- |
| Voltar, avançar, recarregar | Barra superior, junto ao endereço |
| Modo compacto | Primeiro botão da barra; também em Visualização |
| Permissões do site | Indicador à esquerda do endereço |
| Adicionar favorito | Ícone dentro do endereço, à direita |
| Downloads | Barra superior e menu principal |
| Histórico, biblioteca de favoritos e senhas | Menu principal |
| Configurações | Menu principal ou Ctrl+, |
| Abrir menu pelo teclado | F10, inclusive quando o foco está no WebKit |
| Fixar, duplicar, silenciar, mover e dividir abas | Menu de contexto da aba |
| Criar pasta, renomear espaço, reabrir aba | Menu junto ao nome do espaço ou contexto da área de abas |
| Alternar e criar espaços | Faixa numerada no rodapé; nomes em tooltip e acessibilidade |
| Busca, pesquisa personalizada e navegador padrão | Configurações → Geral |
| Bloqueador, listas, cookies e rastreamento | Configurações → Privacidade |
| Zoom e impressão | Menu → Página |
| Tela dividida e tela cheia | Menu → Visualização |
| Perfis, limpeza de dados, sessão e inspeção | Menu → Ferramentas |

O antigo botão lateral de configurações e seu menu duplicado foram removidos.
O botão de nova aba acompanha o fim da lista e continua disponível ao rolar.
Criar um espaço passa a ativá-lo. Abas fixadas continuam acima das abas do espaço.
Durante as configurações, ações específicas da página ficam desabilitadas;
Voltar retorna ao conteúdo, e navegar pelo endereço encerra as configurações.

A referência funcional é a documentação oficial do Zen sobre
[espaços no rodapé](https://docs.zen-browser.app/user-manual/workspaces) e
[menu da barra](https://docs.zen-browser.app/user-manual/bookmarks).
Isso não implica suporte aos demais recursos do Zen, como extensões Firefox
ou sincronização.

Testes de widgets verificam comandos sem duplicação no menu, agrupamento,
restrições de janela privada, seleção de Configurações por Enter, ações da
barra, nomes acessíveis, faixa de 16 espaços e renomeação pelo menu contextual.
Capturas claras e escuras inspecionadas em 1280×800 e 760×540, incluindo menu
principal. A integração com a sessão GTK real continua pendente de X11.

## Alterações 0.3.0

- Paleta central aplicada a menus, diálogos, campos, configurações, abas e
  pastas desenhadas manualmente. A troca de tema dispensa reiniciar abas.
- Claro, escuro ou sistema, com violeta, azul, verde ou âmbar para foco e seleção.
- Seletor de espaços perto do topo da sidebar, botão de criação visível e página
  Espaços nas configurações. O modelo suporta até 16 espaços; a interface agora
  expõe essa capacidade. Exclusão continua limitada ao último espaço vazio.
- Configurações com rolagem para janelas menores. Ícones da interface desenhados
  em vetor para acompanhar a densidade da tela e a paleta atual.
- Canvas padrão do WebKit opaco branco, em vez do grafite imposto anteriormente.
  Páginas com fundo transparente deixam de receber um fundo escuro do navegador.
  O CSS do documento continua responsável pelo tema da página.
- Preferência de claro/escuro dos sites separada do tema do navegador. O ajuste
  usa GtkSettings e o mecanismo nativo do WebKit, sem inverter imagens, injetar
  cores globais em páginas ou substituir `matchMedia` com JavaScript.
- Texto suave: antialiasing em tons de cinza com hinting leve. Mais definido:
  mesmo antialiasing com hinting completo. Sistema: valores GTK capturados no
  início da execução. Não modifica Fontconfig nem as configurações do desktop.

## Evidência e limites

Capturas de widgets reais inspecionadas em 1280×800 e 760×540 nos dois temas.
As abas nas capturas são identificadas como páginas de teste. Testes verificam
persistência da aparência, ações dos seletores, quatro cores de destaque com
contraste mínimo de 4,5:1 nos campos e criação de espaços até o limite do modelo.

A suíte inclui um teste GTK de antialiasing e de `prefers-color-scheme` em ambos
os modos. Ele é pulado quando não há sessão X11 acessível. O gate visual integral
continua pendente de executar páginas reais com GTK: captura Qt offscreen não
prova nitidez do texto web, qualidade de imagens, mídia ou composição na GPU.
Não foi medido ganho de memória nem alterada a política de aceleração nesta
entrega. Nenhum novo WebProcess é criado pelo gerenciador de temas.

O Fontconfig local já informa antialiasing ativo e os monitores configurados
usam escala 1. Isso não sustenta atribuir o problema à escala fracionária nem
ao WebKit ser uma engine de origem Apple. Ainda é necessário comparar o mesmo
documento e imagens, no mesmo monitor e zoom, na sessão gráfica do Zenith.

O WebKitGTK lê tema, antialiasing, hinting e DPI no seu
[adaptador GTK de configurações](https://github.com/WebKit/WebKit/blob/main/Source/WebKit/UIProcess/gtk/SystemSettingsManagerProxyGtk.cpp).
O [MiniBrowser oficial](https://github.com/WebKit/WebKit/blob/main/Tools/MiniBrowser/gtk/main.c)
também aplica a preferência escura por `gtk-application-prefer-dark-theme`.
Uma página pode escolher ignorar essa preferência ou manter tema próprio.
Um `GTK_THEME` externo é convertido em configuração GTK na inicialização do
processo, preservando o tema recebido sem impedir a escolha clara posterior.
Esse cenário ainda requer execução na sessão gráfica.
