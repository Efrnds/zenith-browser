#include "ui/BrowserMenu.h"
#include "core/Navigation.h"
#include "ui/ZenithIcons.h"

BrowserMenu::BrowserMenu(const State &state, QWidget *parent) : QMenu(parent)
{
    setObjectName("browserMenu");
    auto add = [this](QMenu *menu, const QString &label, int command, bool enabled = true) {
        const QString shortcut = Navigation::shortcutLabel(command);
        QAction *action = menu->addAction(shortcut.isEmpty() ? label : label + '\t' + shortcut);
        action->setData(command);
        action->setObjectName("command-" + QString::number(command));
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, [this, command] { emit commandRequested(command); });
        return action;
    };
    add(this, tr("Nova aba"), Navigation::NewTab)->setIcon(zenithIcon(ZenithIcon::Add));
    add(this, tr("Nova janela privada"), Navigation::NewPrivateWindow);
    addSeparator();
    add(this, tr("Histórico"), Navigation::History);
    auto *bookmarks = addMenu(tr("Favoritos"));
    add(bookmarks, tr("Mostrar favoritos"), Navigation::Bookmarks);
    add(bookmarks, tr("Adicionar esta página"), Navigation::Bookmark, state.hasSite);
    bookmarks->addSeparator();
    add(bookmarks, tr("Importar favoritos HTML"), Navigation::ImportBookmarks);
    add(bookmarks, tr("Exportar favoritos HTML"), Navigation::ExportBookmarks);
    add(this, tr("Downloads"), Navigation::Downloads)->setIcon(zenithIcon(ZenithIcon::Download));
    add(this, tr("Senhas e credenciais"), Navigation::Credentials, !state.privateMode);
    addSeparator();
    auto *page = addMenu(tr("Página"));
    add(page, tr("Buscar nesta página"), Navigation::Find, state.hasPage);
    auto *zoom = page->addMenu(tr("Zoom · %1%").arg(state.zoom));
    add(zoom, tr("Aumentar"), Navigation::ZoomIn, state.hasPage);
    add(zoom, tr("Diminuir"), Navigation::ZoomOut, state.hasPage);
    add(zoom, tr("Restaurar 100%"), Navigation::ZoomReset, state.hasPage);
    add(page, tr("Imprimir / salvar PDF"), Navigation::Print, state.hasPage);
    page->addSeparator();
    add(page, tr("Permissões deste site"), Navigation::SiteControls, state.hasSite);
    auto *view = addMenu(tr("Visualização"));
    auto *compact = add(view, tr("Modo compacto"), Navigation::ToggleCompact);
    compact->setCheckable(true); compact->setChecked(state.compact);
    auto *split = add(view, tr("Tela dividida"), Navigation::ToggleSplit, state.hasPage);
    split->setCheckable(true); split->setChecked(state.split);
    auto *fullscreen = add(view, tr("Tela cheia"), Navigation::Fullscreen);
    fullscreen->setCheckable(true); fullscreen->setChecked(state.fullscreen);
    auto *tools = addMenu(tr("Ferramentas"));
    add(tools, tr("Limpar dados de navegação"), Navigation::ClearData);
    auto *profiles = tools->addMenu(tr("Perfis"));
    add(profiles, tr("Abrir outro perfil"), Navigation::OpenProfile);
    add(profiles, tr("Usar este perfil por padrão"), Navigation::DefaultProfile, !state.privateMode);
    add(tools, tr("Salvar sessão agora"), Navigation::SaveSession, !state.privateMode);
    add(tools, tr("Inspecionar página"), Navigation::Inspector, state.hasPage);
    addSeparator();
    add(this, tr("Configurações"), Navigation::Settings)->setIcon(zenithIcon(ZenithIcon::Settings));
    add(this, tr("Fechar janela"), Navigation::CloseWindow);
}
