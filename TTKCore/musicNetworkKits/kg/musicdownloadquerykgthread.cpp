#include "musicdownloadquerykgthread.h"
#include "musicsemaphoreloop.h"
#include "musicnumberutils.h"
#include "musictime.h"
#///QJson import
#include "qjson/parser.h"

MusicDownLoadQueryKGThread::MusicDownLoadQueryKGThread(QObject *parent)
    : MusicDownLoadQueryThreadAbstract(parent)
{
    m_queryServer = "Kugou";
}

QString MusicDownLoadQueryKGThread::getClassName()
{
    return staticMetaObject.className();
}

void MusicDownLoadQueryKGThread::startSearchSong(QueryType type, const QString &text)
{
    m_searchText = text.trimmed();
    m_currentType = type;
    QUrl musicUrl = MusicCryptographicHash::decryptData(KG_SONG_SEARCH_URL, URL_KEY).arg(text).arg(0).arg(50);

    if(m_reply)
    {
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    m_reply = m_manager->get(request);
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicDownLoadQueryKGThread::downLoadFinished()
{
    if(m_reply == nullptr)
    {
        deleteAll();
        return;
    }

    emit clearAllItems();      ///Clear origin items
    m_musicSongInfos.clear();  ///Empty the last search to songsInfo

    if(m_reply->error() == QNetworkReply::NoError)
    {
        QByteArray bytes = m_reply->readAll();///Get all the data obtained by request

        QJson::Parser parser;
        bool ok;
        QVariant data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value.contains("data"))
            {
                value = value["data"].toMap();
                QVariantList datas = value["info"].toList();
                foreach(const QVariant &var, datas)
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInfomation musicInfo;
                    musicInfo.m_singerName = value["singername"].toString();
                    musicInfo.m_songName = value["songname"].toString();
                    musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(value["duration"].toInt()*1000);

                    if(m_currentType != MovieQuery)
                    {
                        musicInfo.m_songId = value["hash"].toString();
                        musicInfo.m_albumId = value["album_id"].toString();
                        readFromMusicSongLrcAndPic(&musicInfo, value["hash"].toString(), m_manager);
                        if(!m_querySimplify)
                        {
                            readFromMusicSongAttribute(&musicInfo, m_manager, value, m_searchQuality, m_queryAllRecords);

                            if(musicInfo.m_songAttrs.isEmpty())
                            {
                                continue;
                            }
                            emit createSearchedItems(musicInfo.m_songName, musicInfo.m_singerName, musicInfo.m_timeLength);
                        }
                        m_musicSongInfos << musicInfo;
                    }
                    else
                    {
                        //MV
                        musicInfo.m_songId = value["mvhash"].toString();
                        readFromMusicMVAttribute(&musicInfo, musicInfo.m_songId);

                        if(musicInfo.m_songAttrs.isEmpty())
                        {
                            continue;
                        }

                        emit createSearchedItems(musicInfo.m_songName, musicInfo.m_singerName, musicInfo.m_timeLength);
                        m_musicSongInfos << musicInfo;
                    }
                }
            }
        }
    }

    emit downLoadDataChanged(QString());
    deleteAll();
}

void MusicDownLoadQueryKGThread::readFromMusicMVAttribute(MusicObject::MusicSongInfomation *info,
                                                          const QString &hash)
{
    if(hash.isEmpty())
    {
        return;
    }

    QByteArray encodedData = QCryptographicHash::hash(QString("%1kugoumvcloud").arg(hash).toUtf8(),
                                                      QCryptographicHash::Md5).toHex().toLower();
    QUrl musicUrl = MusicCryptographicHash::decryptData(KG_MV_ATTR_URL, URL_KEY).arg(QString(encodedData)).arg(hash);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = m_manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(!value.isEmpty() && value.contains("mvdata"))
        {
            value = value["mvdata"].toMap();
            QVariantMap mv = value["sd"].toMap();
            if(!mv.isEmpty())
            {
                readFromMusicMVInfoAttribute(info, mv);
            }
            mv = value["hd"].toMap();
            if(!mv.isEmpty())
            {
                readFromMusicMVInfoAttribute(info, mv);
            }
            mv = value["sq"].toMap();
            if(!mv.isEmpty())
            {
                readFromMusicMVInfoAttribute(info, mv);
            }
            mv = value["rq"].toMap();
            if(!mv.isEmpty())
            {
                readFromMusicMVInfoAttribute(info, mv);
            }
        }
    }
}

void MusicDownLoadQueryKGThread::readFromMusicMVInfoAttribute(MusicObject::MusicSongInfomation *info,
                                                              const QVariantMap &key)
{
    MusicObject::MusicSongAttribute attr;
    attr.m_url = key["downurl"].toString();
    attr.m_size = MusicUtils::Number::size2Label(key["filesize"].toInt());
    attr.m_format = attr.m_url.right(attr.m_url.length() - attr.m_url.lastIndexOf(".") - 1);

    int bitRate = key["bitrate"].toInt()/1000;
    if(bitRate > 375 && bitRate <= 625)
        attr.m_bitrate = MB_500;
    else if(bitRate > 625 && bitRate <= 875)
        attr.m_bitrate = MB_750;
    else if(bitRate > 875)
        attr.m_bitrate = MB_1000;
    else
        attr.m_bitrate = bitRate;

    attr.m_duration = MusicTime::msecTime2LabelJustified(key["timelength"].toInt());
    info->m_songAttrs.append(attr);
}
