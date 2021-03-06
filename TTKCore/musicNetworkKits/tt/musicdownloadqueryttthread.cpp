#include "musicdownloadqueryttthread.h"
#include "musicdownloadttinterface.h"
#include "musicnumberutils.h"
#include "musictime.h"
#///QJson import
#include "qjson/parser.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>

MusicDownLoadQueryTTThread::MusicDownLoadQueryTTThread(QObject *parent)
    : MusicDownLoadQueryThreadAbstract(parent)
{
    m_queryServer = "TTpod";
}

QString MusicDownLoadQueryTTThread::getClassName()
{
    return staticMetaObject.className();
}

void MusicDownLoadQueryTTThread::startSearchSong(QueryType type, const QString &text)
{
    m_searchText = text.trimmed();
    m_currentType = type;
    QUrl musicUrl = MusicCryptographicHash::decryptData(TT_SONG_SEARCH_URL, URL_KEY).arg(text);

    if(m_reply)
    {
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    QNetworkRequest request;
    request.setUrl(musicUrl);
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    m_reply = m_manager->get( request );
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()) );
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     SLOT(replyError(QNetworkReply::NetworkError)) );
}

void MusicDownLoadQueryTTThread::downLoadFinished()
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
                QVariantList datas = value["data"].toList();
                foreach(const QVariant &var, datas)
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInfomation musicInfo;
                    musicInfo.m_singerName = value["singerName"].toString();
                    musicInfo.m_songName = value["name"].toString();
                    musicInfo.m_timeLength = "-";
                    musicInfo.m_songId = QString::number(value["songId"].toULongLong());

                    if(m_currentType != MovieQuery)
                    {
                        musicInfo.m_artistId = QString::number(value["singerId"].toULongLong());
                        musicInfo.m_albumId = QString::number(value["albumId"].toULongLong());
                        if(!m_querySimplify)
                        {
                            musicInfo.m_lrcUrl = MusicCryptographicHash::decryptData(TT_SONG_LRC_URL, URL_KEY)
                                                 .arg(musicInfo.m_singerName).arg(musicInfo.m_songName).arg(musicInfo.m_songId);
                            musicInfo.m_smallPicUrl = value["picUrl"].toString();

                            readFromMusicSongAttribute(&musicInfo, value, m_searchQuality, m_queryAllRecords);

                            if(musicInfo.m_songAttrs.isEmpty())
                            {
                                continue;
                            }

                            musicInfo.m_timeLength = musicInfo.m_songAttrs.first().m_duration;
                            emit createSearchedItems(musicInfo.m_songName, musicInfo.m_singerName, musicInfo.m_timeLength);
                        }
                        m_musicSongInfos << musicInfo;
                    }
                    else
                    {
                        QVariantList mvs = value["mvList"].toList();
                        if(!mvs.isEmpty())
                        {
                            foreach(const QVariant &mv, mvs)
                            {
                                QVariantMap mvUrlValue = mv.toMap();
                                if(mvUrlValue.isEmpty())
                                {
                                    continue;
                                }

                                int bitRate = mvUrlValue["bitRate"].toInt();
                                if(bitRate == 0)
                                {
                                    continue;
                                }

                                musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(mvUrlValue["duration"].toInt());
                                MusicObject::MusicSongAttribute songAttr;

                                if(bitRate > 375 && bitRate <= 625)
                                    songAttr.m_bitrate = MB_500;
                                else if(bitRate > 625 && bitRate <= 875)
                                    songAttr.m_bitrate = MB_750;
                                else if(bitRate > 875)
                                    songAttr.m_bitrate = MB_1000;
                                else
                                    songAttr.m_bitrate = bitRate;

                                songAttr.m_format = mvUrlValue["suffix"].toString();
                                songAttr.m_url = mvUrlValue["url"].toString();
                                songAttr.m_size = MusicUtils::Number::size2Label(mvUrlValue["size"].toInt());
                                musicInfo.m_songAttrs << songAttr;
                            }
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
        ///If there is no search to song_id, is repeated several times in the search
        ///If more than 5 times or no results give up
        static int counter = 5;
        if(m_musicSongInfos.isEmpty() && counter-- > 0)
        {
            startSearchSong(m_currentType, m_searchText);
        }
        else
        {
            M_LOGGER_ERROR("not find the song_Id");
        }
    }

    emit downLoadDataChanged(QString());
    deleteAll();
}
