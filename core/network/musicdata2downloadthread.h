#ifndef MUSICDATA2DOWNLOADTHREAD_H
#define MUSICDATA2DOWNLOADTHREAD_H

/* =================================================
 * This file is part of the Music Player project
 * Copyright (c) 2014 - 2015 Greedysky Studio
 * All rights reserved!
 * Redistribution and use of the source code or any derivative
 * works are strictly forbiden.
   =================================================*/

#include "musicdatadownloadthread.h"

class MUSIC_CORE_EXPORT MusicData2DownloadThread : public MusicDataDownloadThread
{
    Q_OBJECT
public:
    explicit MusicData2DownloadThread(const QString &url, const QString &save,
                                      QObject *parent = 0);
    void deleteAll();
    virtual void startToDownload();

public slots:
    void dataGetFinished();
    void dataReplyError(QNetworkReply::NetworkError error);

protected:
    QNetworkAccessManager *m_dataManager;
    QNetworkReply *m_dataReply;

};

#endif // MUSICDATA2DOWNLOADTHREAD_H