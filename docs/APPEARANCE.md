# Aparência e renderização — candidata 1.0

## Acabamento da candidata

Configurações têm oito destinos: Geral, Aparência, Espaços, Privacidade,
Desempenho, Downloads, Perfis e dados e Sobre. Tema, destaque e densidade
precedem as opções recolhidas de cor personalizada e prévia. O retorno entre
ferramentas preserva os widgets de origem, limitado a seis rotas nativas;
nenhuma dessas telas cria uma página WebKit.

As medidas compartilhadas estão em `src/ui/UiMetrics.h`. A família de ícones
funcionais é desenhada pelo próprio projeto em `ZenithIcons.cpp`, sem importar
desenhos de terceiros nem substituir partes pelo tema do desktop. Não há uma
licença de terceiros a atribuir a esses desenhos; a licença de distribuição do
projeto ainda precisa ser definida pelo titular. Favicons são conteúdo dos sites.

Espaçamentos do QSS são resolvidos pelos mesmos tokens 4/8/12/16/24/32 usados
pelos layouts nativos. Altura da barra, área de toque e largura dos campos também
têm definições comuns. Recuos ópticos pequenos de ícones, traços de 1–2 px e
raios particulares dos componentes permanecem locais. Papéis de cor estão em
`ThemeManager::Role`, com testes de contraste para texto, foco e erro nos temas
e destaques disponíveis. Ações de bibliotecas refluem em linhas em vez de
comprimir os rótulos quando há pouco espaço ou fonte ampliada.

Abas expõem papel, seleção e ações de ativar/fechar pela acessibilidade do Qt.
Cabeçalhos de pasta usam QAbstractButton para expor uma ação nativa. Isso ainda
exige conferência com tecnologia assistiva conectada à engine.

O título da aba reserva a área de fechar mesmo sem hover, evitando deslocamento
ao mover o mouse. O desenho do botão ocupa 16 px dentro de uma área de 28 px;
o foco por teclado também torna a ação visível. Nomes de espaços usam a largura
real da lateral para truncamento.

Direção: manter o navegador de abas verticais inspirado no Zen, conforme o
pedido do autor. Grafite ou claro, uma cor de destaque por vez, controles
compactos e conteúdo da página como centro da janela. Noto Sans prioriza
legibilidade em português na interface Linux; fontes dos documentos continuam
sob controle dos sites. ENERGY 2 / RHYTHM 2 / MOTION 1, sem animações novas.

## Ferramentas integradas — 1.0.0-rc.1

Downloads, histórico, favoritos, credenciais, permissões, limpeza, edição de
favoritos e abertura de perfis ocupam a área de conteúdo do navegador. O retorno,
Esc e Ctrl+W recuperam a página anterior. Essas ferramentas não criam WebViews.
As consultas assíncronas das bibliotecas respeitam a vida da página e usam estado
próprio de paginação, sem referências a variáveis de uma função já encerrada.

Ctrl+F abre uma barra junto ao conteúdo, com resultado, anterior, próxima e fechar.
Enter, Shift+Enter, F3 e Shift+F3 preservam a navegação pelas ocorrências. Fechar
cancela a busca pendente e os destaques, mantendo o termo para retomar depois.
O foco entre GTK e Qt precisa de homologação gráfica, conforme RELEASE.md.

As ações de registros ficam próximas à lista; retorno e paginação ficam no rodapé.
Essa separação reduz a largura exigida pelas barras de botões. Configurações usam
24 px de margem de leitura e mantêm o seletor de categorias em janelas estreitas.
Os ícones principais usam os símbolos do tema do sistema quando disponíveis;
o fallback monocromático mantém a mesma função, com configuração representada
por controles deslizantes e suspensão por pausa. Não foram usados bitmaps novos.

Personalização acrescenta cor hexadecimal (com tons ajustados para contraste),
fonte da interface entre 11 e 18 px, lateral entre 180 e 320 px, abas entre 32 e
48 px e raio interno entre 0 e 20 px. Fonte e zoom dos sites ficam separados da
interface. Preferências se aplicam a views existentes e às próximas views; zoom
salvo por origem tem prioridade sobre o padrão. Busca personalizada e edição
de espaços usam formulários no próprio conteúdo.

ENERGY 2 / RHYTHM 2 / MOTION 1. A hierarquia serve às tarefas: localizar uma
preferência, editar um grupo e voltar à navegação, com a direção Zen preservada.

## Telas internas e moldura — 0.4.0

Direção preservada: navegação de abas verticais inspirada no Zen, tema grafite
ou claro e uma cor de destaque. ENERGY 2 / RHYTHM 2 / MOTION 1. A busca da nova
aba é o foco dessa tela; nas configurações, o título e os grupos de controles
organizam a leitura. Os grupos têm borda discreta para separar assuntos, sem
transformar cada opção em um cartão. A prévia serve para conferir a paleta antes
de continuar navegando e é identificada como prévia da interface.

- Nova aba em Qt: busca/endereço, identificação do espaço, mecanismo de busca
  atual, atalhos para favoritos, histórico e downloads. Abas `about:blank`
  novas, sem histórico e sem view, permanecem sem `WebView` até navegar ou entrar
  em split. Popups relacionados continuam criando a view necessária ao motor.
- As seis páginas de configurações têm largura de leitura limitada, seções por
  função e rolagem. Aparência inclui prévia nativa; Espaços aceita ativação por
  Enter; Sobre permite copiar as versões para relatar problemas.
- O cabeçalho das configurações reúne retorno e busca por preferência, sem
  exigir acentos. A busca filtra seções e categorias e oferece limpeza quando
  não encontra resultados. Abaixo de 820 px de conteúdo, um seletor substitui
  a navegação lateral. Controles de aparência acompanham o tema atual; ao abrir
  a tela, privacidade, animações e limites de abas são relidos sem emitir alterações.
- Favoritos, histórico, downloads e credenciais usam linhas com título e detalhe.
  Um delegate desenha as linhas; não é criado um widget por registro. Downloads
  ativos também exibem uma barra proporcional ao progresso real. Seleção e IDs
  são preservados nas atualizações, assim como antes.
- Estados vazios orientam a próxima ação. Resultados de busca vazios e erros de
  consulta são distintos da ausência inicial de registros.
- A janela recebe fundo opaco e uma moldura de 6 px com contorno arredondado;
  ao maximizar ou entrar em tela cheia, o recuo é removido. O tema é informado
  ao gerenciador X11 por `_GTK_THEME_VARIANT`, conforme a
  [integração documentada pelo GTK](https://docs.gtk.org/gdk3-x11/method.X11Window.set_theme_variant.html).
- O `GtkPlug` embutido fica explicitamente sem decoração e sem borda. Seu fundo
  acompanha o tema da interface, sem impor esse fundo ao documento web. Isso
  evita uma segunda moldura GTK dentro da janela e reduz superfícies claras
  expostas durante a integração do host.
- Textos padrão do Qt usam a tradução `qtbase_pt_BR` quando instalada no runtime.

A geometria externa da janela e o recorte final dos cantos continuam sob controle
do compositor. Não foram alteradas configurações do desktop, e não foi aplicada
uma máscara que corte o conteúdo nativo do WebKit. A inspeção offscreen cobre os
fundos e contornos Qt; ainda é necessária uma conferência X11/XWayland para a
decoração externa, maximização, vídeo e troca de abas com o motor embutido.

Foram geradas capturas de nova aba e das seis páginas de configurações em claro
e escuro, em 1280×800 e 760×540. A inspeção detectou e corrigiu uma busca estreita,
o corte de texto nas linhas e controles numéricos sem acabamento. Os dados das
capturas são fixtures identificadas como teste. Não houve benchmark novo de RAM.

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
