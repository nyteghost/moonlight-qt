#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QFile>
#include <QString>

class QNetworkReply;

class AutoUpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit AutoUpdateChecker(QObject *parent = nullptr);

    Q_INVOKABLE void start();

    // Checks GitHub for a newer release right now (used by the "Check for updates" button).
    // Works on every platform and always reports a result through onCheckFinished().
    Q_INVOKABLE void checkForUpdates();

    // Downloads the release asset found by the update check (the portable zip on
    // Windows) into the user's Downloads folder. Progress is reported via signals.
    Q_INVOKABLE void downloadUpdate();

    // Opens the folder containing the downloaded update
    Q_INVOKABLE void revealDownload();

    // True when the downloaded update can be installed by Vibelight itself: Windows, a
    // downloaded .zip, a portable install (portable.dat next to the exe) in a writable folder.
    Q_INVOKABLE bool canInstallUpdate() const;

    // Starts the updater script and closes Vibelight so it can replace the files.
    // Returns false (and changes nothing) if the script could not be started.
    Q_INVOKABLE bool installUpdateAndRestart();

    // True when the latest release has a package for this platform that can be downloaded
    Q_INVOKABLE bool hasDownload() const { return !m_DownloadUrl.isEmpty(); }

signals:
    void onUpdateAvailable(QString newVersion, QString url);

    // Emitted when any check completes, with a short human-readable result
    void onCheckFinished(bool updateAvailable, QString message);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(QString filePath);
    void downloadFailed(QString message);

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
    void ensureNetworkManager();
    void sendCheckRequest();

    void parseStringToVersionQuad(QString& string, QVector<int>& version);

    int compareVersion(QVector<int>& version1, QVector<int>& version2);

    QString getPlatform();

    void pickDownloadAsset(const QJsonArray& assets);

    QString uniqueDownloadPath(const QString& fileName) const;

    void finishDownload();
    void failDownload(const QString& message);

    QVector<int> m_CurrentVersionQuad;
    QNetworkAccessManager* m_Nam;

    // Update package details taken from the release's assets
    QString m_DownloadUrl;
    QString m_DownloadName;
    QString m_DownloadSha256;
    qint64 m_DownloadSize = 0;

    // Download state
    QNetworkAccessManager* m_DownloadNam = nullptr;
    QNetworkReply* m_DownloadReply = nullptr;
    QFile m_DownloadFile;
    QString m_DownloadPath;
    QString m_DownloadedFile;
};
