/*
 *  Copyright (C) 2009 Aleksey Palazhchenko
 *  Copyright (C) 2014 Sergey Shambir
 *  Copyright (C) 2016 Alexander Makarov
 *
 * This file is a part of Breakpad-qt library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <QDir>
#include <QProcess>
#include <QCoreApplication>

#include "QBreakpadHandler.h"
#include "QBreakpadHttpUploader.h"

#if defined(Q_OS_MAC)
#include "client/mac/handler/exception_handler.h"
#elif defined(Q_OS_LINUX)
#include "client/linux/handler/exception_handler.h"
//#include "common/linux/http_upload.h"
#elif defined(Q_OS_WIN32)
#include "client/windows/handler/exception_handler.h"
#endif

#if defined(Q_OS_WIN32)
bool DumpCallback(const wchar_t* dump_dir,
                                    const wchar_t* minidump_id,
                                    void* context,
                                    EXCEPTION_POINTERS* exinfo,
                                    MDRawAssertionInfo* assertion,
                                    bool succeeded)
#elif defined(Q_OS_MAC)
bool DumpCallback(const char *dump_dir,
                                    const char *minidump_id,
                                    void *context, bool succeeded)
#else
bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                                    void* context,
                                    bool succeeded)
#endif
{
#ifdef Q_OS_LINUX
    Q_UNUSED(descriptor);
#endif
    Q_UNUSED(context);
#if defined(Q_OS_WIN32)
    Q_UNUSED(assertion);
    Q_UNUSED(exinfo);
#endif
    /*
        NO STACK USE, NO HEAP USE THERE !!!
        Creating QString's, using qDebug, etc. - everything is crash-unfriendly.
    */

#if defined(Q_OS_WIN32)
    QString path = QString::fromWCharArray(dump_dir) + QLatin1String("/") + QString::fromWCharArray(minidump_id);
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));
#elif defined(Q_OS_MAC)
    QString path = QString::fromUtf8(dump_dir) + QLatin1String("/") + QString::fromUtf8(minidump_id);
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));
#else
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", descriptor.path());
#endif


    // TODO: add http-reporter call here
    // qDebug("Trying to report (HTTP POST)");
    // Adding parameters
//    std::map<string, string> params;
//    params["prod"] = productName;
//    params["ver"] = productVersion;
//    google_breakpad::HTTPUpload::SendRequest(QBreakpadHandler::uploadUrl(),
//                                             params,
//                                             );

    return true;
}

class QBreakpadHandlerPrivate
{
public:
    google_breakpad::ExceptionHandler* pExptHandler;
    QString dumpPath;
    QUrl uploadUrl;
};

//------------------------------------------------------------------------------
QBreakpadHandler::QBreakpadHandler() :
    d(new QBreakpadHandlerPrivate())
{
}

QBreakpadHandler::~QBreakpadHandler()
{
    delete d;
}

void QBreakpadHandler::setDumpPath(const QString& path)
{
    QString absPath = path;
    if(!QDir::isAbsolutePath(absPath)) {
        absPath = QDir::cleanPath(qApp->applicationDirPath() + "/" + path);
    }
    Q_ASSERT(QDir::isAbsolutePath(absPath));

    QDir().mkpath(absPath);
    if (!QDir().exists(absPath)) {
        qDebug("Failed to set dump path which not exists: %s", qPrintable(absPath));
        return;
    }

    d->dumpPath = absPath;

// NOTE: ExceptionHandler initialization
#if defined(Q_OS_WIN32)
    d->handler_ = new google_breakpad::ExceptionHandler(absPath.toStdWString(), /*FilterCallback*/ 0,
                                                        DumpCallback, /*context*/ 0,
                                                        google_breakpad::ExceptionHandler::HANDLER_ALL);
#elif defined(Q_OS_MAC)
    d->handler_ = new google_breakpad::ExceptionHandler(absPath.toStdString(), /*FilterCallback*/ 0,
                                                        DumpCallback, /*context*/ 0, true, NULL);
#else
    d->pExptHandler = new google_breakpad::ExceptionHandler(google_breakpad::MinidumpDescriptor(absPath.toStdString()),
                                                            /*FilterCallback*/ 0,
                                                            DumpCallback,
                                                            /*context*/ 0,
                                                            true,
                                                            -1);
#endif
}

QString QBreakpadHandler::uploadUrl() const
{
    return d->uploadUrl.toString();
}

QStringList QBreakpadHandler::dumpFileList() const
{
    if(!d->dumpPath.isNull() && !d->dumpPath.isEmpty()) {
        QDir dumpDir(d->dumpPath);
        dumpDir.setNameFilters(QStringList()<<"*.dmp");
        return dumpDir.entryList();
    }

    return QStringList();
}

void QBreakpadHandler::setUploadUrl(const QUrl &url)
{
    if(!url.isValid() || url.isEmpty())
        return;

    d->uploadUrl = url;
}

void QBreakpadHandler::sendDumps()
{
    if(!d->dumpPath.isNull() && !d->dumpPath.isEmpty()) {
        QDir dumpDir(d->dumpPath);
        dumpDir.setNameFilters(QStringList()<<"*.dmp");
        QStringList dumpFiles = dumpDir.entryList();

        //qDebug() << "Files for send(" << d->uploadUrl.toString() << ")";
        foreach(QString itDmpFileName, dumpFiles) {
            //qDebug() << "/tsending " << d->dumpPath + QLatin1String("/") + itDmpFileName;
            QBreakpadHttpUploader *sender = new QBreakpadHttpUploader(d->uploadUrl);
            sender->uploadDump(d->dumpPath + "/" + itDmpFileName);
        }
    }
}

