/******************************************************************************\
 * Copyright (c) 2026
 *
 * Author(s):
 *  JamulusAccess
 *
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
\******************************************************************************/

#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// JamulusAccess updates itself from its own GitHub releases (never from the Jamulus servers). The latest
// release is compared with this version; the installer ("JamulusAccessInstaller.exe" asset) is downloaded,
// checked, then run silently: it closes the program and starts it again afterwards.
class CAccessUpdater : public QObject
{
    Q_OBJECT

public:
    explicit CAccessUpdater ( QObject* parent = nullptr );

    void CheckForUpdates();

    // after UpdateCheckFinished(): the newer version ("1.01"), empty when this one is the latest
    QString NewVersion() const { return strNewVersion; }
    bool    CheckFailed() const { return bCheckFailed; }
    QString ErrorString() const { return strError; }

    // the release notes in this language ("fr", "en"): the release text has "[lang=fr]" / "[lang=en]" parts
    QString ReleaseNotes ( const QString& strLang ) const;

    // downloads the installer: DownloadProgress(), then InstallerReady() or InstallerFailed()
    void DownloadInstaller();
    void CancelDownload();

    // runs the downloaded installer silently; false if it could not start
    bool RunInstaller() const;

signals:
    void UpdateCheckFinished();
    void DownloadProgress ( qint64 iReceived, qint64 iTotal );
    void InstallerReady();
    void InstallerFailed ( QString strReason );

protected:
    void OnReleaseInfo ( QNetworkReply* reply );
    void OnInstaller ( QNetworkReply* reply );

    QNetworkAccessManager* pNetwork;
    QNetworkReply*         pDownload;
    QString                strNewVersion;
    bool                   bCheckFailed;
    QString                strError;
    QString                strReleaseBody;
    QString                strInstallerUrl;
    qint64                 iInstallerSize;
    QString                strInstallerPath;
};
