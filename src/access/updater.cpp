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

#include "updater.h"
#include "../global.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#ifdef _WIN32
#    include <windows.h>
#    include <shellapi.h>
#endif

namespace
{
const char* RELEASE_API_URL = "https://api.github.com/repos/" JAMULUSACCESS_GITHUB_REPO "/releases/latest";
const char* INSTALLER_NAME  = "JamulusAccessInstaller.exe";

QNetworkRequest MakeRequest ( const QString& strUrl )
{
    QNetworkRequest request { QUrl ( strUrl ) };
    request.setHeader ( QNetworkRequest::UserAgentHeader, QString ( "JamulusAccess/" JAMULUSACCESS_VERSION " updater" ) );
    request.setAttribute ( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
    return request;
}

// "v1.01", "1.01" -> 1, 1; -1, -1 if not a version
void ParseVersion ( const QString& strText, int& iMajor, int& iMinor )
{
    static const QRegularExpression versionRegex ( "(\\d+)\\.(\\d+)" );
    const QRegularExpressionMatch   match = versionRegex.match ( strText );
    iMajor                                = match.hasMatch() ? match.captured ( 1 ).toInt() : -1;
    iMinor                                = match.hasMatch() ? match.captured ( 2 ).toInt() : -1;
}
} // namespace

CAccessUpdater::CAccessUpdater ( QObject* parent ) :
    QObject ( parent ),
    pNetwork ( new QNetworkAccessManager ( this ) ),
    pDownload ( nullptr ),
    bCheckFailed ( false ),
    iInstallerSize ( -1 )
{}

void CAccessUpdater::CheckForUpdates()
{
    // JAMULUSACCESS_UPDATE_API: for tests only, a local server standing for GitHub
    const QString strApi =
        qEnvironmentVariableIsSet ( "JAMULUSACCESS_UPDATE_API" ) ? qEnvironmentVariable ( "JAMULUSACCESS_UPDATE_API" ) : QString ( RELEASE_API_URL );
    QNetworkRequest request = MakeRequest ( strApi );
    request.setRawHeader ( "Accept", "application/vnd.github+json" );
    QNetworkReply* reply = pNetwork->get ( request );
    connect ( reply, &QNetworkReply::finished, this, [this, reply] { OnReleaseInfo ( reply ); } );
}

void CAccessUpdater::OnReleaseInfo ( QNetworkReply* reply )
{
    reply->deleteLater();

    if ( reply->attribute ( QNetworkRequest::HttpStatusCodeAttribute ).toInt() == 404 )
    {
        // nothing released yet: this version is the latest
        emit UpdateCheckFinished();
        return;
    }
    if ( reply->error() != QNetworkReply::NoError )
    {
        bCheckFailed = true;
        strError     = reply->errorString();
        emit UpdateCheckFinished();
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson ( reply->readAll() ).object();
    int               iMajor;
    int               iMinor;
    ParseVersion ( release.value ( "tag_name" ).toString(), iMajor, iMinor );
    if ( iMajor < 0 )
    {
        bCheckFailed = true;
        strError     = tr ( "The latest release has no version number." );
        emit UpdateCheckFinished();
        return;
    }

    if ( iMajor > JAMULUSACCESS_VERSION_MAJOR || ( iMajor == JAMULUSACCESS_VERSION_MAJOR && iMinor > JAMULUSACCESS_VERSION_MINOR ) )
    {
        const QJsonArray assets = release.value ( "assets" ).toArray();
        for ( const QJsonValue& assetValue : assets )
        {
            const QJsonObject asset = assetValue.toObject();
            if ( asset.value ( "name" ).toString().compare ( INSTALLER_NAME, Qt::CaseInsensitive ) == 0 )
            {
                strInstallerUrl = asset.value ( "browser_download_url" ).toString();
                iInstallerSize  = static_cast<qint64> ( asset.value ( "size" ).toDouble ( -1 ) );
            }
        }
        if ( strInstallerUrl.isEmpty() )
        {
            bCheckFailed = true;
            strError     = tr ( "The latest release has no installer." );
        }
        else
        {
            strNewVersion  = QString ( "%1.%2" ).arg ( iMajor ).arg ( iMinor, 2, 10, QChar ( '0' ) );
            strReleaseBody = release.value ( "body" ).toString();
        }
    }
    emit UpdateCheckFinished();
}

QString CAccessUpdater::ReleaseNotes ( const QString& strLang ) const
{
    // parts marked "[lang=fr]" / "[lang=en]" on their own line: the wanted language, else English,
    // else the whole text; light markdown ("**", "#") removed for reading
    QHash<QString, QStringList>     parts;
    QString                         strCurrent;
    static const QRegularExpression marker ( "^\\s*\\[lang=(\\w+)\\]\\s*$" );
    const QStringList               lines = strReleaseBody.split ( '\n' );
    for ( const QString& strLine : lines )
    {
        const QRegularExpressionMatch match = marker.match ( strLine );
        if ( match.hasMatch() )
        {
            strCurrent = match.captured ( 1 ).toLower();
            continue;
        }
        parts[strCurrent].append ( strLine );
    }

    QString strText = parts.value ( strLang ).join ( '\n' ).trimmed();
    if ( strText.isEmpty() )
    {
        strText = parts.value ( "en" ).join ( '\n' ).trimmed();
    }
    if ( strText.isEmpty() )
    {
        strText = strReleaseBody.trimmed();
    }

    strText.remove ( "**" ).remove ( '\r' );
    static const QRegularExpression heading ( "^#+\\s*", QRegularExpression::MultilineOption );
    strText.remove ( heading );
    return strText;
}

void CAccessUpdater::DownloadInstaller()
{
    pDownload = pNetwork->get ( MakeRequest ( strInstallerUrl ) );
    connect ( pDownload, &QNetworkReply::downloadProgress, this, &CAccessUpdater::DownloadProgress );
    QNetworkReply* reply = pDownload;
    connect ( reply, &QNetworkReply::finished, this, [this, reply] { OnInstaller ( reply ); } );
}

void CAccessUpdater::CancelDownload()
{
    if ( pDownload )
    {
        pDownload->abort();
    }
}

void CAccessUpdater::OnInstaller ( QNetworkReply* reply )
{
    reply->deleteLater();
    pDownload = nullptr;

    if ( reply->error() == QNetworkReply::OperationCanceledError )
    {
        return;
    }
    if ( reply->error() != QNetworkReply::NoError )
    {
        emit InstallerFailed ( reply->errorString() );
        return;
    }

    // a connection cut short must not give a broken installer: complete size, and a real program ("MZ")
    const QByteArray data = reply->readAll();
    if ( ( iInstallerSize > 0 && data.size() != iInstallerSize ) || !data.startsWith ( "MZ" ) )
    {
        emit InstallerFailed ( tr ( "The download is incomplete." ) );
        return;
    }

    strInstallerPath = QDir::temp().filePath ( QString ( "JamulusAccessInstaller_%1.exe" ).arg ( strNewVersion ) );
    QFile file ( strInstallerPath );
    if ( !file.open ( QIODevice::WriteOnly | QIODevice::Truncate ) || file.write ( data ) != data.size() )
    {
        emit InstallerFailed ( tr ( "The installer could not be saved: %1" ).arg ( QDir::toNativeSeparators ( strInstallerPath ) ) );
        return;
    }
    file.close();
    emit InstallerReady();
}

bool CAccessUpdater::RunInstaller() const
{
#ifdef _WIN32
    // the installer asks for administrator rights (Windows prompt), closes JamulusAccess if it still
    // runs, installs without questions and starts JamulusAccess again
    const std::wstring strPath = QDir::toNativeSeparators ( strInstallerPath ).toStdWString();
    const auto         result  = reinterpret_cast<INT_PTR> (
        ::ShellExecuteW ( nullptr, L"open", strPath.c_str(), L"/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS", nullptr, SW_SHOWNORMAL ) );
    return result > 32;
#else
    return false;
#endif
}
