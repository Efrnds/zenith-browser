#include "core/Preferences.h"
#include <QSettings>
#include <QStandardPaths>

const QList<Preferences::Entry> &Preferences::entries()
{
    static const QList<Entry> list{
        {"preferences/searchTemplate", "general", "Mecanismo de busca", "https://duckduckgo.com/?q=%1", Effect::NextNavigation},
        {"preferences/restoreSession", "general", "Restaurar sessão", true, Effect::NextStartup},
        {"appearance/theme", "appearance", "Tema do navegador", "dark"},
        {"appearance/accent", "appearance", "Cor de destaque", "violet"},
        {"appearance/fontSize", "appearance", "Fonte da interface", 13},
        {"appearance/sidebarWidth", "appearance", "Largura da lateral", 236},
        {"appearance/tabHeight", "appearance", "Altura das abas", 36},
        {"appearance/cornerRadius", "appearance", "Raio dos cantos", 10},
        {"appearance/sites", "appearance", "Tema dos sites", "system", Effect::Reload},
        {"appearance/textRendering", "appearance", "Renderização de texto", "smooth", Effect::Reload},
        {"preferences/compactMode", "appearance", "Modo compacto", false},
        {"preferences/reducedMotion", "appearance", "Reduzir movimento", false},
        {"web/defaultFontSize", "appearance", "Fonte dos sites", 16, Effect::Reload},
        {"web/defaultZoom", "appearance", "Zoom padrão", 100, Effect::NextNavigation},
        {"preferences/adblock", "privacy", "Bloquear anúncios", true, Effect::Reload},
        {"privacy/blockThirdPartyCookies", "privacy", "Cookies de terceiros", false},
        {"privacy/trackingPrevention", "privacy", "Prevenir rastreamento", false},
        {"privacy/savePasswords", "privacy", "Oferecer para salvar senhas", true, Effect::NextNavigation},
        {"privacy/fillPasswords", "privacy", "Preencher credenciais", true, Effect::NextNavigation},
        {"web/javascript", "privacy", "Permitir JavaScript", true, Effect::Reload},
        {"web/smoothScrolling", "privacy", "Rolagem suave", true},
        {"preferences/discardAfterSeconds", "performance", "Descarte por inatividade", 180, Effect::NextCycle},
        {"preferences/maxLiveTabs", "performance", "Meta de páginas carregadas", 3, Effect::NextCycle},
        {"preferences/suspendOnMinimize", "performance", "Descartar ao minimizar", true, Effect::NextCycle},
        {"downloads/askDestination", "downloads", "Perguntar onde salvar", true, Effect::NextDownload},
        {"downloads/directory", "downloads", "Pasta de downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), Effect::NextDownload}
    };
    return list;
}

QVariant Preferences::value(const QString &key)
{
    QSettings settings;
    for (const auto &entry : entries()) if (entry.key == key) return settings.value(key, entry.fallback);
    return settings.value(key);
}

void Preferences::resetCategory(const QString &category)
{
    QSettings settings;
    for (const auto &entry : entries())
        if (entry.category == category) settings.setValue(entry.key, entry.fallback);
}

QString Preferences::effectDescription(const QString &key)
{
    for (const auto &entry : entries()) {
        if (entry.key != key) continue;
        switch (entry.effect) {
        case Effect::Immediate: return QStringLiteral("Aplicado imediatamente.");
        case Effect::Reload: return QStringLiteral("Recarregue as páginas para aplicar a todos os recursos.");
        case Effect::NextNavigation:
            if (key == "web/defaultZoom") return QStringLiteral("Usado nas próximas páginas; o zoom por site já salvo é preservado.");
            if (key == "preferences/searchTemplate") return QStringLiteral("Usado na próxima pesquisa.");
            return QStringLiteral("Usado nas próximas páginas.");
        case Effect::NextDownload: return QStringLiteral("Usado nos próximos downloads.");
        case Effect::NextStartup: return QStringLiteral("Usado na próxima abertura do navegador.");
        case Effect::NextCycle: return QStringLiteral("Usado no próximo ciclo de descarte ou minimização.");
        }
    }
    return {};
}
