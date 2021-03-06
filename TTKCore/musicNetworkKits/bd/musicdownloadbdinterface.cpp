#include "musicdownloadbdinterface.h"
#include "musiccryptographichash.h"
#include "musicnumberutils.h"
#include "musicsemaphoreloop.h"
#include "musictime.h"
#///QJson import
#include "qjson/parser.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QNetworkAccessManager>

void MusicDownLoadBDInterface::readFromMusicSongAttribute(MusicObject::MusicSongInfomation *info, QNetworkAccessManager *manager,
                                                          const QString &bit, const QString &id)
{
    if(id.isEmpty())
    {
        return;
    }

    QUrl musicUrl = MusicCryptographicHash::decryptData(BD_SONG_ATTR_URL, URL_KEY).arg(id).arg(bit)
                    .arg("Xw3BvQ9t46Sb%2BTP4RHZaFsqFx7vHFuQEx6t59t5%2FYrwJuXpxxH6A%2BoWQveBUfYG9");

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["error_code"].toInt() == 22000 && value.contains("bitrate"))
        {
            value = value["bitrate"].toMap();

            MusicObject::MusicSongAttribute attr;
            attr.m_url = value["file_link"].toString();
            if(attr.m_url.contains("pan."))
            {
                readFromMusicPayAttribute(info, manager, id);
            }
            else
            {
                attr.m_url.replace(MusicCryptographicHash::decryptData(BD_SONG_YYDOWN_URL, URL_KEY),
                                   MusicCryptographicHash::decryptData(BD_SONG_SSDOWN_URL, URL_KEY));
                attr.m_duration = MusicTime::msecTime2LabelJustified(value["file_duration"].toInt()*1000);
                attr.m_size = MusicUtils::Number::size2Label(value["file_size"].toInt());
                attr.m_format = value["file_extension"].toString();
                attr.m_bitrate = map2NormalBitrate(value["file_bitrate"].toInt());
                info->m_songAttrs.append(attr);
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicSongAttribute(MusicObject::MusicSongInfomation *info, QNetworkAccessManager *manager,
                                                          const QString &format, const QString &quality, bool all)
{
    QString formatString = format;
    foreach(const QString &f, formatString.split(","))
    {
        if(all)
        {
            if(f != "flac")
            {
                readFromMusicSongAttribute(info, manager, f, info->m_songId);
            }
            else
            {
                readFromMusicLLAttribute(info, manager, info->m_songId);
            }
        }
        else
        {
            if(f != "flac")
            {
                int bit = map2NormalBitrate(f.toInt());
                if(quality == QObject::tr("ST") && bit < MB_128)
                {
                    readFromMusicSongAttribute(info, manager, f, info->m_songId);
                }
                else if(quality == QObject::tr("SD") && bit >= MB_128 && bit < MB_192)
                {
                    readFromMusicSongAttribute(info, manager, f, info->m_songId);
                }
                else if(quality == QObject::tr("HQ") && bit >= MB_192 && bit < MB_320)
                {
                    readFromMusicSongAttribute(info, manager, f, info->m_songId);
                }
                else if(quality == QObject::tr("SQ") && bit >= MB_320)
                {
                    readFromMusicSongAttribute(info, manager, f, info->m_songId);
                }
            }
            else if(quality == QObject::tr("CD"))
            {
                readFromMusicLLAttribute(info, manager, info->m_songId);
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicLLAttribute(MusicObject::MusicSongInfomation *info, QNetworkAccessManager *manager,
                                                        const QString &id)
{
    if(id.isEmpty())
    {
        return;
    }

    QUrl musicUrl = MusicCryptographicHash::decryptData(BD_SONG_INFO_URL, URL_KEY).arg(id);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["error_code"].toInt() == 22000 && value.contains("data"))
        {
            value = value["data"].toMap();
            QVariantList datas = value["songList"].toList();
            foreach(const QVariant &var, datas)
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                value = value["linkinfo"].toMap();
                if(!value.keys().isEmpty())
                {
                    value = value[value.keys().first()].toMap();
                    MusicObject::MusicSongAttribute attr;
                    attr.m_url = value["songLink"].toString();
                    attr.m_size = MusicUtils::Number::size2Label(value["size"].toInt());
                    attr.m_format = value["format"].toString();
                    attr.m_bitrate = map2NormalBitrate(value["rate"].toInt());
                    info->m_songAttrs.append(attr);
                }
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicPayAttribute(MusicObject::MusicSongInfomation *info, QNetworkAccessManager *manager,
                                                         const QString &id)
{
    if(id.isEmpty())
    {
        return;
    }

    QUrl musicUrl = MusicCryptographicHash::decryptData(BD_SONG_FMINFO_URL, URL_KEY).arg(id);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["error_code"].toInt() == 22000 && value.contains("data"))
        {
            value = value["data"].toMap();
            QVariantList datas = value["songList"].toList();
            foreach(const QVariant &var, datas)
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                MusicObject::MusicSongAttribute attr;
                attr.m_url = value["songLink"].toString();
                attr.m_url.replace(MusicCryptographicHash::decryptData(BD_SONG_YYDOWN_URL, URL_KEY),
                                   MusicCryptographicHash::decryptData(BD_SONG_SSDOWN_URL, URL_KEY));
                attr.m_size = MusicUtils::Number::size2Label(value["size"].toInt());
                attr.m_format = value["format"].toString();
                attr.m_bitrate = map2NormalBitrate(value["rate"].toInt());
                info->m_songAttrs.append(attr);
            }
        }
    }
}

int MusicDownLoadBDInterface::map2NormalBitrate(int bitrate)
{
    if(bitrate > 0 && bitrate < 128)
        return MB_32;
    else if(bitrate > 128 && bitrate < 192)
        return MB_128;
    else if(bitrate > 192 && bitrate < 320)
        return MB_320;
    else if(bitrate > 320)
        return MB_500;
    else
        return bitrate;
}
