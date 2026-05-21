#ifndef INSTALLER_H
#define INSTALLER_H

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QStyle>
#include <QRegularExpression>
#include <QThread>
#include <QSvgRenderer>
#include <QPainter>
#include <QIcon>

class InstallerWindow : public QWidget {
    Q_OBJECT

public:
    InstallerWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("SplitCommander Installer");
        resize(680, 520);
        setMinimumSize(640, 480);
        
        setupStyles();
        
        mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        stackedWidget = new QStackedWidget(this);
        
        createPageWelcome();
        createPagePlugins();
        createPageProgress();
        createPageSuccess();
        
        mainLayout->addWidget(stackedWidget);
        
        stackedWidget->setCurrentIndex(0);
    }

private:
    void setupStyles() {
        // Pure Premium Neutral Dark Mode (Carbon Slate)
        setStyleSheet(
            "QWidget { "
            "    background-color: #121214; " // Sleek neutral carbon slate background
            "    color: #e4e4e7; "
            "    font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif; "
            "} "
            "QLabel { "
            "    font-size: 14px; "
            "} "
            "QPushButton { "
            "    background-color: #3b82f6; "
            "    color: #ffffff; "
            "    border: none; "
            "    border-radius: 6px; "
            "    padding: 10px 24px; "
            "    font-size: 14px; "
            "    font-weight: bold; "
            "} "
            "QPushButton:hover { "
            "    background-color: #60a5fa; "
            "} "
            "QPushButton:pressed { "
            "    background-color: #2563eb; "
            "} "
            "QPushButton#backBtn { "
            "    background-color: #27272a; "
            "    color: #e4e4e7; "
            "    border: 1px solid #3f3f46; "
            "} "
            "QPushButton#backBtn:hover { "
            "    background-color: #3f3f46; "
            "} "
            "QPushButton#detailsBtn { "
            "    background-color: #27272a; "
            "    color: #a1a1aa; "
            "    padding: 6px 14px; "
            "    font-size: 12px; "
            "} "
            "QFrame#pluginCard { "
            "    background-color: #1c1c1e; " // Premium dark card background
            "    border: 1px solid #2d2d30; "
            "    border-radius: 8px; "
            "} "
            "QFrame#pluginCard:hover { "
            "    background-color: #252529; "
            "    border: 1px solid #3b82f6; "
            "} "
            "QCheckBox { "
            "    background: transparent; "
            "    border: none; "
            "    padding: 0; "
            "} "
            "QCheckBox::indicator { "
            "    width: 18px; "
            "    height: 18px; "
            "    border-radius: 4px; "
            "    border: 2px solid #52525b; "
            "    background-color: #09090b; "
            "} "
            "QCheckBox::indicator:checked { "
            "    background-color: #3b82f6; "
            "    border: 2px solid #60a5fa; "
            "} "
            "QProgressBar { "
            "    border: 1px solid #2d2d30; "
            "    border-radius: 8px; "
            "    text-align: center; "
            "    color: #ffffff; "
            "    background-color: #09090b; "
            "    height: 24px; "
            "    font-weight: bold; "
            "} "
            "QProgressBar::chunk { "
            "    background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #60a5fa); "
            "    border-radius: 7px; "
            "} "
            "QTextEdit { "
            "    background-color: #09090b; "
            "    color: #4ade80; " // Vibrant neon-green for compiler log
            "    font-family: 'Courier New', 'Fira Code', monospace; "
            "    font-size: 11px; "
            "    border: 1px solid #2d2d30; "
            "    border-radius: 6px; "
            "    padding: 8px; "
            "} "
        );
    }

    void createPageWelcome() {
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(40, 30, 40, 30);
        layout->setSpacing(16);
        
        layout->addStretch(1); // Top spacer for breathing room
        
        // Render logo
        QLabel *logoLabel = new QLabel(page);
        logoLabel->setAlignment(Qt::AlignCenter);
        
        QString logoPath = "logo.svg";
        if (!QFile::exists(logoPath)) logoPath = "../logo.svg";
        if (!QFile::exists(logoPath)) logoPath = "src/splitcommander.svg";
        
        if (QFile::exists(logoPath)) {
            QSvgRenderer renderer(logoPath);
            QSize defaultSize = renderer.defaultSize();
            int targetWidth = 400; // Perfect width for wide horizontal banner
            int targetHeight = 150;
            
            if (defaultSize.isValid() && defaultSize.height() > 0) {
                double aspectRatio = static_cast<double>(defaultSize.width()) / defaultSize.height();
                targetHeight = static_cast<int>(targetWidth / aspectRatio);
            }
            
            QPixmap pix(targetWidth, targetHeight);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            renderer.render(&painter);
            
            logoLabel->setPixmap(pix);
            logoLabel->setFixedSize(targetWidth, targetHeight);
        } else {
            logoLabel->setText("<h2>[ SplitCommander ]</h2>");
        }
        
        QLabel *titleLabel = new QLabel("SplitCommander Setup", page);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #ffffff; margin-top: 10px;");
        
        QLabel *descLabel = new QLabel(
            "Willkommen beim Installationsassistenten von SplitCommander.<br>"
            "SplitCommander ist ein nativer KDE Dual-Pane Dateimanager.<br><br>"
            "Klicken Sie auf Weiter, um optionale Plugins auszuwählen und das Programm zu installieren.",
            page
        );
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("color: #a1a1aa; line-height: 1.5; font-size: 13px;");
        
        layout->addStretch(1); // Middle spacer to separate content nicely
        
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        QPushButton *nextBtn = new QPushButton("Weiter", page);
        connect(nextBtn, &QPushButton::clicked, this, [this]() {
            stackedWidget->setCurrentIndex(1);
        });
        btnLayout->addWidget(nextBtn);
        
        layout->addWidget(logoLabel, 0, Qt::AlignCenter);
        layout->addWidget(titleLabel);
        layout->addWidget(descLabel);
        layout->addStretch(2); // Bottom spacer
        layout->addLayout(btnLayout);
        
        stackedWidget->addWidget(page);
    }

    void createPagePlugins() {
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(40, 30, 40, 30);
        layout->setSpacing(14);
        
        layout->addStretch(1);
        
        QLabel *titleLabel = new QLabel("Optionale Features auswählen", page);
        titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #ffffff;");
        
        QLabel *descLabel = new QLabel("Wählen Sie die optionalen Plugins aus, die mitkompiliert werden sollen:", page);
        descLabel->setStyleSheet("color: #a1a1aa; font-size: 13px;");
        
        QVBoxLayout *cbLayout = new QVBoxLayout();
        cbLayout->setSpacing(10);
        
        // --- Card 1: Git Manager ---
        QFrame *cardGit = new QFrame(page);
        cardGit->setObjectName("pluginCard");
        QHBoxLayout *layGit = new QHBoxLayout(cardGit);
        layGit->setContentsMargins(12, 12, 12, 12);
        layGit->setSpacing(12);
        
        cbGit = new QCheckBox(cardGit);
        
        QLabel *lblGit = new QLabel("<b>Git Manager</b><br><font color='#a1a1aa'>Verwaltet Git-Repositories direkt in der Sidebar und zeigt den Repository-Status.</font>", cardGit);
        lblGit->setTextFormat(Qt::RichText);
        lblGit->setStyleSheet("background: transparent; border: none; font-size: 13px;");
        lblGit->setWordWrap(true);
        
        layGit->addWidget(cbGit);
        layGit->addWidget(lblGit, 1);
        cbLayout->addWidget(cardGit);
        
        // --- Card 2: Paperless-ngx ---
        QFrame *cardPaperless = new QFrame(page);
        cardPaperless->setObjectName("pluginCard");
        QHBoxLayout *layPaperless = new QHBoxLayout(cardPaperless);
        layPaperless->setContentsMargins(12, 12, 12, 12);
        layPaperless->setSpacing(12);
        
        cbPaperless = new QCheckBox(cardPaperless);
        
        QLabel *lblPaperless = new QLabel("<b>Paperless-ngx</b><br><font color='#a1a1aa'>Ermöglicht den direkten Dokumenten-Upload und die Suche im Paperless-System.</font>", cardPaperless);
        lblPaperless->setTextFormat(Qt::RichText);
        lblPaperless->setStyleSheet("background: transparent; border: none; font-size: 13px;");
        lblPaperless->setWordWrap(true);
        
        layPaperless->addWidget(cbPaperless);
        layPaperless->addWidget(lblPaperless, 1);
        cbLayout->addWidget(cardPaperless);
        
        // --- Card 3: ISO einbinden ---
        QFrame *cardMountIso = new QFrame(page);
        cardMountIso->setObjectName("pluginCard");
        QHBoxLayout *layMountIso = new QHBoxLayout(cardMountIso);
        layMountIso->setContentsMargins(12, 12, 12, 12);
        layMountIso->setSpacing(12);
        
        cbMountIso = new QCheckBox(cardMountIso);
        
        QLabel *lblMountIso = new QLabel("<b>ISO einbinden</b><br><font color='#a1a1aa'>Ermöglicht das Mounten und Unmounten von ISO/IMG-Dateien per Rechtsklick.</font>", cardMountIso);
        lblMountIso->setTextFormat(Qt::RichText);
        lblMountIso->setStyleSheet("background: transparent; border: none; font-size: 13px;");
        lblMountIso->setWordWrap(true);
        
        layMountIso->addWidget(cbMountIso);
        layMountIso->addWidget(lblMountIso, 1);
        cbLayout->addWidget(cardMountIso);
        
        // --- Card 4: Makefile-Aktionen ---
        QFrame *cardMakefile = new QFrame(page);
        cardMakefile->setObjectName("pluginCard");
        QHBoxLayout *layMakefile = new QHBoxLayout(cardMakefile);
        layMakefile->setContentsMargins(12, 12, 12, 12);
        layMakefile->setSpacing(12);
        
        cbMakefile = new QCheckBox(cardMakefile);
        
        QLabel *lblMakefile = new QLabel("<b>Makefile-Aktionen</b><br><font color='#a1a1aa'>Erlaubt das Ausführen von Make-Targets direkt aus dem Dateimanager.</font>", cardMakefile);
        lblMakefile->setTextFormat(Qt::RichText);
        lblMakefile->setStyleSheet("background: transparent; border: none; font-size: 13px;");
        lblMakefile->setWordWrap(true);
        
        layMakefile->addWidget(cbMakefile);
        layMakefile->addWidget(lblMakefile, 1);
        cbLayout->addWidget(cardMakefile);
        
        // --- Checkbox toggle signals to dynamically style cards ---
        auto updateCardStyle = [](QFrame *card, bool checked) {
            if (checked) {
                card->setStyleSheet(
                    "QFrame#pluginCard { "
                    "    background-color: #182235; "
                    "    border: 1.5px solid #3b82f6; "
                    "    border-radius: 8px; "
                    "} "
                );
            } else {
                card->setStyleSheet(""); // Resets to default CSS stylesheet rule
            }
        };
        
        connect(cbGit, &QCheckBox::toggled, this, [cardGit, updateCardStyle](bool checked) {
            updateCardStyle(cardGit, checked);
        });
        connect(cbPaperless, &QCheckBox::toggled, this, [cardPaperless, updateCardStyle](bool checked) {
            updateCardStyle(cardPaperless, checked);
        });
        connect(cbMountIso, &QCheckBox::toggled, this, [cardMountIso, updateCardStyle](bool checked) {
            updateCardStyle(cardMountIso, checked);
        });
        connect(cbMakefile, &QCheckBox::toggled, this, [cardMakefile, updateCardStyle](bool checked) {
            updateCardStyle(cardMakefile, checked);
        });
        
        // Set Defaults
        cbGit->setChecked(true);
        cbPaperless->setChecked(true);
        updateCardStyle(cardGit, true);
        updateCardStyle(cardPaperless, true);
        
        layout->addStretch(1);
        
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *backBtn = new QPushButton("Zurück", page);
        backBtn->setObjectName("backBtn");
        connect(backBtn, &QPushButton::clicked, this, [this]() {
            stackedWidget->setCurrentIndex(0);
        });
        
        QPushButton *installBtn = new QPushButton("Installieren", page);
        connect(installBtn, &QPushButton::clicked, this, &InstallerWindow::startInstallation);
        
        btnLayout->addWidget(backBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(installBtn);
        
        layout->addWidget(titleLabel);
        layout->addWidget(descLabel);
        layout->addLayout(cbLayout);
        layout->addStretch(2);
        layout->addLayout(btnLayout);
        
        stackedWidget->addWidget(page);
    }

    void createPageProgress() {
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(40, 30, 40, 30);
        layout->setSpacing(16);
        
        layout->addStretch(1);
        
        titleProgress = new QLabel("SplitCommander wird installiert", page);
        titleProgress->setStyleSheet("font-size: 22px; font-weight: bold; color: #ffffff;");
        
        statusLabel = new QLabel("Bereite Build vor...", page);
        statusLabel->setStyleSheet("color: #a1a1aa; font-size: 13px;");
        
        progressBar = new QProgressBar(page);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        
        QHBoxLayout *detailsHeaderLayout = new QHBoxLayout();
        detailsHeaderLayout->addWidget(new QLabel("Installationslog:", page));
        detailsHeaderLayout->addStretch();
        
        QPushButton *detailsBtn = new QPushButton("Details anzeigen", page);
        detailsBtn->setObjectName("detailsBtn");
        detailsHeaderLayout->addWidget(detailsBtn);
        
        logWidget = new QTextEdit(page);
        logWidget->setReadOnly(true);
        logWidget->setMinimumHeight(180);
        logWidget->setVisible(false);
        
        connect(detailsBtn, &QPushButton::clicked, this, [this, detailsBtn]() {
            if (logWidget->isVisible()) {
                logWidget->setVisible(false);
                detailsBtn->setText("Details anzeigen");
            } else {
                logWidget->setVisible(true);
                detailsBtn->setText("Details ausblenden");
            }
        });
        
        layout->addWidget(titleProgress);
        layout->addWidget(statusLabel);
        layout->addWidget(progressBar);
        layout->addLayout(detailsHeaderLayout);
        layout->addWidget(logWidget);
        layout->addStretch(2);
        
        stackedWidget->addWidget(page);
    }

    void createPageSuccess() {
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(40, 30, 40, 30);
        layout->setSpacing(20);
        
        layout->addStretch(1);
        
        QLabel *checkLabel = new QLabel(page);
        checkLabel->setAlignment(Qt::AlignCenter);
        
        QPixmap pix(120, 120);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor("#a6e3a1"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(10, 10, 100, 100);
        painter.setPen(QPen(QColor("#121214"), 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        QPolygonF check;
        check << QPointF(38, 60) << QPointF(52, 74) << QPointF(82, 44);
        painter.drawPolyline(check);
        checkLabel->setPixmap(pix);
        
        QLabel *titleLabel = new QLabel("Installation abgeschlossen!", page);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #a6e3a1;");
        
        QLabel *descLabel = new QLabel(
            "SplitCommander wurde erfolgreich auf Ihrem System installiert.<br>"
            "Sie können die Anwendung nun direkt über den Start-Button öffnen.",
            page
        );
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("color: #a1a1aa; line-height: 1.5; font-size: 13px;");
        
        layout->addStretch(1);
        
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *launchBtn = new QPushButton("SplitCommander starten", page);
        launchBtn->setStyleSheet("background-color: #a6e3a1; color: #121214; font-weight: bold;");
        connect(launchBtn, &QPushButton::clicked, this, &InstallerWindow::launchApp);
        
        QPushButton *closeBtn = new QPushButton("Schließen", page);
        closeBtn->setObjectName("backBtn");
        connect(closeBtn, &QPushButton::clicked, qApp, &QApplication::quit);
        
        btnLayout->addWidget(closeBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(launchBtn);
        
        layout->addWidget(checkLabel);
        layout->addWidget(titleLabel);
        layout->addWidget(descLabel);
        layout->addStretch(2);
        layout->addLayout(btnLayout);
        
        stackedWidget->addWidget(page);
    }

private slots:
    void startInstallation() {
        stackedWidget->setCurrentIndex(2);
        progressBar->setValue(5);
        statusLabel->setText("Starte Build-Prozess...");
        
        QString scriptPath = QDir::current().absoluteFilePath("install.sh");
        
        process = new QProcess(this);
        process->setWorkingDirectory(QDir::currentPath());
        process->setProcessChannelMode(QProcess::MergedChannels);
        
        connect(process, &QProcess::readyReadStandardOutput, this, &InstallerWindow::readProcessOutput);
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &InstallerWindow::processFinished);
        
        QStringList args;
        if (cbGit->isChecked()) args << "--git";
        if (cbPaperless->isChecked()) args << "--paperless";
        if (cbMountIso->isChecked()) args << "--iso";
        if (cbMakefile->isChecked()) args << "--makefile";
        
        args << "--no-deps" << "--no-install";
        
        logWidget->append("Running build command: " + scriptPath + " " + args.join(" ") + "\n");
        
        process->start(scriptPath, args);
    }
    
    void readProcessOutput() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromUtf8(data);
        logWidget->append(text);
        
        QStringList lines = text.split('\n');
        for (const QString &line : lines) {
            if (line.contains("==> Konfiguriere Build")) {
                progressBar->setValue(15);
                statusLabel->setText("Konfiguriere CMake Build...");
            } else if (line.contains("==> Kompiliere SplitCommander")) {
                progressBar->setValue(40);
                statusLabel->setText("Kompiliere SplitCommander Quellcode...");
            } else if (line.contains("Build erfolgreich")) {
                progressBar->setValue(90);
                statusLabel->setText("Kompilierung erfolgreich!");
            }
            
            static QRegularExpression re(R"(\[(\d+)/(\d+)\])");
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                int current = match.captured(1).toInt();
                int total = match.captured(2).toInt();
                if (total > 0) {
                    int p = 40 + static_cast<int>((static_cast<double>(current) / total) * 45);
                    progressBar->setValue(p);
                    statusLabel->setText(QString("Kompiliere Datei %1 von %2...").arg(current).arg(total));
                }
            }
        }
    }
    
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            progressBar->setValue(90);
            statusLabel->setText("System-Installation (Passwort-Eingabe erforderlich)...");
            logWidget->append("\nRunning graphical installation via Polkit...\n");
            
            QString scriptPath = QDir::current().absoluteFilePath("install.sh");
            
            QProcess *installProcess = new QProcess(this);
            installProcess->setWorkingDirectory(QDir::currentPath());
            installProcess->setProcessChannelMode(QProcess::MergedChannels);
            
            connect(installProcess, &QProcess::readyReadStandardOutput, this, [this, installProcess]() {
                logWidget->append(QString::fromUtf8(installProcess->readAllStandardOutput()));
            });
            
            connect(installProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && code == 0) {
                    progressBar->setValue(100);
                    statusLabel->setText("Installation abgeschlossen!");
                    stackedWidget->setCurrentIndex(3);
                } else {
                    statusLabel->setText("System-Installation fehlgeschlagen oder abgebrochen.");
                    progressBar->setStyleSheet("QProgressBar::chunk { background-color: #f38ba8; }");
                    progressBar->setValue(90);
                }
            });
            
            QStringList installArgs;
            installArgs << scriptPath;
            if (cbGit->isChecked()) installArgs << "--git";
            if (cbPaperless->isChecked()) installArgs << "--paperless";
            if (cbMountIso->isChecked()) installArgs << "--iso";
            if (cbMakefile->isChecked()) installArgs << "--makefile";
            installArgs << "--no-deps";
            
            installProcess->start("pkexec", installArgs);
        } else {
            statusLabel->setText("Kompilierung fehlgeschlagen. Überprüfen Sie den Log.");
            progressBar->setStyleSheet("QProgressBar::chunk { background-color: #f38ba8; }");
        }
    }
    
    void launchApp() {
        QProcess::startDetached("splitcommander");
        qApp->quit();
    }

private:
    QVBoxLayout *mainLayout;
    QStackedWidget *stackedWidget;
    
    QCheckBox *cbGit;
    QCheckBox *cbPaperless;
    QCheckBox *cbMountIso;
    QCheckBox *cbMakefile;
    
    QLabel *titleProgress;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QTextEdit *logWidget;
    
    QProcess *process;
};

#endif // INSTALLER_H
