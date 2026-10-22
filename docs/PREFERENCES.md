# Preferências da candidata 1.0

O registro em `src/core/Preferences.cpp` define chave, categoria, padrão e
momento de aplicação. QSettings persiste os valores no perfil corrente.
Restaurar uma categoria altera somente as chaves registradas nela; não apaga
arquivos, sessão, credenciais, espaços, favoritos ou histórico. A confirmação
fica na própria seção e o efeito é aplicado à janela aberta.

Cada chave tem um controle principal identificado por `preferenceKey`; a suíte
confere que o inventário inteiro está representado uma única vez. O tooltip
informa quando o efeito será usado. A busca aceita termos sem acento, múltiplas
palavras e aliases como dark/light, adblock, performance e workspace. Enter
leva ao controle encontrado, e limpar a busca restaura categoria e rolagem.

| Categoria | Chave | Padrão | Controle / aplicação |
| --- | --- | --- | --- |
| Geral | preferences/searchTemplate | DuckDuckGo | Seletor ou URL HTTPS com %1; próxima busca |
| Geral | preferences/restoreSession | true | Checkbox; próxima abertura |
| Aparência | appearance/theme | dark | Sistema/claro/escuro; imediato |
| Aparência | appearance/accent | violet | Seletor ou #RRGGBB; imediato |
| Aparência | appearance/fontSize | 13 | Número limitado; imediato |
| Aparência | appearance/sidebarWidth | 236 | Número limitado; imediato |
| Aparência | appearance/tabHeight | 36 | Número limitado; imediato |
| Aparência | appearance/cornerRadius | 10 | Número limitado; imediato |
| Aparência | appearance/sites | system | Seletor; recarregar sites |
| Aparência | appearance/textRendering | smooth | Seletor; recarregar sites |
| Aparência | preferences/compactMode | false | Checkbox; imediato |
| Aparência | preferences/reducedMotion | false | Checkbox; imediato |
| Aparência | web/defaultFontSize | 16 | Número limitado; recarregar sites |
| Aparência | web/defaultZoom | 100 | Número limitado; preserva zoom salvo por origem |
| Privacidade | preferences/adblock | true | Checkbox; recursos já carregados exigem reload |
| Privacidade | privacy/blockThirdPartyCookies | false | Checkbox; política imediata |
| Privacidade | privacy/trackingPrevention | false | Checkbox; política imediata |
| Privacidade | privacy/savePasswords | true | Checkbox; próximas solicitações |
| Privacidade | privacy/fillPasswords | true | Checkbox; próximas páginas |
| Privacidade | web/javascript | true | Checkbox; recarregar sites |
| Privacidade | web/smoothScrolling | true | Checkbox; imediato |
| Desempenho | preferences/discardAfterSeconds | 180 | Número limitado; próximo ciclo do descarte |
| Desempenho | preferences/maxLiveTabs | 3 | Número limitado; meta sujeita à proteção de páginas |
| Desempenho | preferences/suspendOnMinimize | true | Checkbox; próxima minimização |
| Downloads | downloads/askDestination | true | Checkbox; próximo download |
| Downloads | downloads/directory | pasta Downloads do sistema | Escolha de diretório; próximo download |

Dimensões personalizadas são preservadas ao selecionar “Personalizada”. Os
presets de densidade alteram apenas fonte, largura da lateral e altura de abas.
As listas de espaços e os perfis são dados de organização, fora da restauração
de preferências. Permissões e zoom por origem ficam no banco de navegação.
