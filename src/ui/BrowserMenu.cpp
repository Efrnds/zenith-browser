#include "ui/BrowserMenu.h"
#include "core/Navigation.h"
#include "ui/ZenithIcons.h"

BrowserMenu::BrowserMenu(const State &state, QWidget *parent) : QMenu(parent)
{
    setObjectName("browserMenu");
    auto add = [this](QMenu *menu, const QString &label, int command, const QString &shortcut = QString(), bool enabled = true) {
        QAction *action = menu->addAction(shortcut.isEmpty() ? label : label + '\t' + shortcut);
        action->setData(command);
        action->setObjectName("command-" + QString::number(command));
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, [this, command] { emit commandRequested(command); });
        return action;
    };
    add(this, tr("Nova aba"), Navigation::NewTab, "Ctrl+T")->setIcon(zenithIcon(ZenithIcon::Add));
    add(this, tr("Nova janela privada"), Navigation::NewPrivateWindow, "Ctrl+Shift+P");
    addSeparator();
    add(this, tr("Histórico"), Navigation::History, "Ctrl+H");
    auto *bookmarks = addMenu(tr("Favoritos"));
    add(bookmarks, tr("Mostrar favoritos"), Navigation::Bookmarks, "Ctrl+Shift+B");
    add(bookmarks, tr("Adicionar esta página"), Navigation::Bookmark, "Ctrl+D", state.hasSite);
    bookmarks->addSeparator();
    add(bookmarks, tr("Importar favoritos HTML"), Navigation::ImportBookmarks);
    add(bookmarks, tr("Exportar favoritos HTML"), Navigation::ExportBookmarks);
    add(this, tr("Downloads"), Navigation::Downloads, "Ctrl+J")->setIcon(zenithIcon(ZenithIcon::Download));
    add(this, tr("Senhas e credenciais"), Navigation::Credentials, {}, !state.privateMode);
    addSeparator();
    auto *page = addMenu(tr("Página"));
    add(page, tr("Buscar nesta página"), Navigation::Find, "Ctrl+F", state.hasPage);
    auto *zoom = page->addMenu(tr("Zoom · %1%").arg(state.zoom));
    add(zoom, tr("Aumentar"), Navigation::ZoomIn, "Ctrl++", state.hasPage);
    add(zoom, tr("Diminuir"), Navigation::ZoomOut, "Ctrl+-", state.hasPage);
    add(zoom, tr("Restaurar 100%"), Navigation::ZoomReset, "Ctrl+0", state.hasPage);
    add(page, tr("Imprimir / salvar PDF"), Navigation::Print, "Ctrl+P", state.hasPage);
    page->addSeparator();
    add(page, tr("Permissões deste site"), Navigation::SiteControls, {}, state.hasSite);
    auto *view = addMenu(tr("Visualização"));
    auto *compact = add(view, tr("Modo compacto"), Navigation::ToggleCompact);
    compact->setCheckable(true); compact->setChecked(state.compact);
    auto *split = add(view, tr("Tela dividida"), Navigation::ToggleSplit, "Ctrl+\\", state.hasPage);
    split->setCheckable(true); split->setChecked(state.split);
    auto *fullscreen = add(view, tr("Tela cheia"), Navigation::Fullscreen, "F11");
    fullscreen->setCheckable(true); fullscreen->setChecked(state.fullscreen);
    auto *tools = addMenu(tr("Ferramentas"));
    add(tools, tr("Limpar dados de navegação"), Navigation::ClearData);
    auto *profiles = tools->addMenu(tr("Perfis"));
    add(profiles, tr("Abrir outro perfil"), Navigation::OpenProfile);
    add(profiles, tr("Usar este perfil por padrão"), Navigation::DefaultProfile, {}, !state.privateMode);
    add(tools, tr("Salvar sessão agora"), Navigation::SaveSession, {}, !state.privateMode);
    add(tools, tr("Inspecionar página"), Navigation::Inspector, "F12", state.hasPage);
    addSeparator();
    add(this, tr("Configurações"), Navigation::Settings, "Ctrl+,")->setIcon(zenithIcon(ZenithIcon::Settings));
    add(this, tr("Fechar janela"), Navigation::CloseWindow);
}
