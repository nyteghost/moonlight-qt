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

    // Downloads the release asset found by the update check (the portable zip on
    // Windows) into the user's Downloads folder. Progress is reported via signals.
    Q_INVOKABLE void downloadUpdate();

    // Opens the folder containing the downloaded update
    Q_INVOKABLE void revealDownload();

    // True when the latest release has a package for this platform that can be downloaded
    Q_INVOKABLE bool hasDownload() const { return !m_DownloadUrl.isEmpty(); }

signals:
    void onUpdateAvailable(QString newVersion, QString url);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(QString filePath);
    void downloadFailed(QString message);

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
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
