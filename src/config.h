#pragma once

#include <QString>
#include <QStringList>
#include <QColor>
#include <QList>
#include <KSharedConfig>
#include <KConfigGroup>

class Config {
public:
    static bool    useSystemTheme();
    static QString selectedTheme();
    static QColor  ageBadgeColor(int index);
    static QString dateFormat();
    static bool    singleClickOpen();
    static bool    showHiddenFiles();
    static bool    showFileExtensions();
    static int     startupBehavior();
    static QString startupPath();
    static bool    showNewIndicator();
    static int     ageBadgeSaturation();
    static int     ageBadgeLightness();

    static QString lastLeftPath();
    static QString lastRightPath();



    static void setUseSystemTheme(bool b);

    static void setSelectedTheme(const QString &t);
    static void setSingleClickOpen(bool b);
    static void setShowHiddenFiles(bool b);
    static void setShowFileExtensions(bool b);
    static void setStartupBehavior(int i);
    static void setStartupPath(const QString &p);
    static void setShowNewIndicator(bool b);
    static void setAgeBadgeSaturation(int i);
    static void setAgeBadgeLightness(int i);
    static void setLastPaths(const QString &left, const QString &right);

    static bool useThumbnails();
    static int  maxThumbnailSize();
    static void setUseThumbnails(bool b);
    static void setMaxThumbnailSize(int i);

    static QStringList fileTypeColors();
    static void setFileTypeColors(const QStringList &list);

    // Erweiterte Einstellungen
    static bool showDriveIp();
    static bool showMillerIp();
    static QStringList driveBlacklist();
    // === Symbol-Größen (in Pixeln) ===
    static int sidebarIconSize();   // Größe der Icons in der Seitenleiste (Sidebar)
    static int driveIconSize();     // Größe der Symbole für Laufwerke
    static int millerIconSize();    // Größe der Datei- und Ordner-Icons in den Spalten (Miller Columns)
    static int listIconSize();      // Größe der Icons in der klassischen Dateiliste (FilePane)

    // === Zeilenhöhen & Abstände (in Pixeln) ===
    static int sidebarRowHeight();       // Zeilenhöhe für normale Orte und Lesezeichen in der Seitenleiste
    static int sidebarDriveRowHeight();  // Zeilenhöhe für physische Laufwerke in der Seitenleiste
    static int sidebarNetRowHeight();    // Zeilenhöhe für Netzwerklaufwerke in der Seitenleiste
    static int millerDriveRowHeight();   // Zeilenhöhe für Laufwerke in der ersten Spalte ("Dieser PC")
    static int millerHeaderHeight();     // Höhe des Titelbalkens über jeder Spalte (wo der Ordnername steht)
    // === Globale Benutzeroberflächen-Optionen ===
    static int     uiFontSize();        // Die globale Schriftgröße in der gesamten App (in Punkten)
    static QString uiFontFamily();      // Der Name der genutzten Schriftart (Schriftfamilie)
    static QString appLanguage();       // Das Sprachkürzel der App-Sprache (z. B. "de", "en", "fr")
    static int     uiSpacing();         // Der globale Abstand (Dichte/Padding) zwischen UI-Elementen (in Pixeln)
    static void setUiFontSize(int i);
    static void setUiSpacing(int i);
    static void setUiFontFamily(const QString &f);
    static void setAppLanguage(const QString &lang);

    // Drive-Refresh

    static void setShowDriveIp(bool b);
    static void setShowMillerIp(bool b);
    static void setDriveBlacklist(const QStringList &list);
    static void setSidebarIconSize(int i);
    static void setDriveIconSize(int i);
    static void setMillerIconSize(int i);
    static void setListIconSize(int i);

    static void setSidebarRowHeight(int i);
    static void setSidebarDriveRowHeight(int i);
    static void setSidebarNetRowHeight(int i);
    static void setMillerDriveRowHeight(int i);
    static void setMillerHeaderHeight(int i);

#ifdef SC_PLUGIN_GIT
    // === Git-Plugin-Einstellungen ===
    static QString gitLocalDir();       // Lokaler Pfad des standardmäßigen Git-Repositorys
    static QString gitRemoteUrl();      // Remote-URL (z. B. auf GitHub/GitLab) für das Standard-Repo
    static QString gitUsername();       // Benutzername für Git-Operationen

    // Multi-Repo: Struktur für ein zusätzliches Git-Repository
    struct GitRepo {
        QString name;       // Anzeigename des Repositorys in der Sidebar
        QString localDir;   // Lokaler Verzeichnispfad des Git-Repos
        QString remoteUrl;  // Remote-Repository-URL
        QString username;   // Zugehöriger Git-Benutzername
    };
    static QList<GitRepo> gitRepos();                   // Gibt eine Liste aller registrierten Zweit-Repositorys zurück
    static void setGitRepos(const QList<GitRepo> &repos); // Speichert die Liste aller Zweit-Repositorys

    // Git Sidebar & Aktualisierungsintervalle
    static bool gitShowSidebar();                       // Steuert, ob der Git-Bereich in der Seitenleiste angezeigt wird
    static QString gitRefreshMode();                    // Aktualisierungsmodus: "onchange" (bei Dateifokus), "periodic" (zeitgesteuert), "manual" (manuell)
    static int gitRefreshIntervalMinutes();             // Zeitintervall für die periodische Prüfung (in Minuten)
    static void setGitShowSidebar(bool b);
    static void setGitRefreshMode(const QString &m);
    static void setGitRefreshIntervalMinutes(int i);
    static QString gitToken();                          // API-Token für GitHub/GitLab Authentifizierung

    static void setGitLocalDir(const QString &s);
    static void setGitRemoteUrl(const QString &s);
    static void setGitUsername(const QString &s);
    static void setGitToken(const QString &s);
#endif // SC_PLUGIN_GIT

#ifdef SC_PLUGIN_PAPERLESS
    // === Paperless-ngx-Plugin-Einstellungen ===
    static QString paperlessUrl();       // Web-Adresse der Paperless-ngx Instanz (z. B. http://192.168.1.100:8000)
    static QString paperlessToken();     // Persönlicher API-Token für die Authentifizierung bei Paperless
    static bool    paperlessSslIgnore();  // Gibt an, ob SSL-Zertifikatsfehler ignoriert werden sollen (für lokale HTTPS-Instanzen)
    static void setPaperlessUrl(const QString &s);
    static void setPaperlessToken(const QString &s);
    static void setPaperlessSslIgnore(bool b);
#endif // SC_PLUGIN_PAPERLESS

    static KConfigGroup group(const QString &name);

};

