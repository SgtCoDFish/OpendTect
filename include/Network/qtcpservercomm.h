#ifndef qtcpservercomm_h
#define qtcpservercomm_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Nanne Hemstra
 Date:          March 2009
 RCS:           $Id: qtcpservercomm.h,v 1.1 2009-03-18 04:24:39 cvsnanne Exp $
________________________________________________________________________

-*/

#include <QTcpServer>
#include "tcpserver.h"

/*\brief QTcpServer communication class

  Internal object, to hide Qt's signal/slot mechanism.
*/

class QTcpServerComm : public QObject 
{
    Q_OBJECT
    friend class	TcpServer;

protected:

QTcpServerComm( QTcpServer* qtcpserver, TcpServer* tcpserver )
    : qtcpserver_(qtcpserver)
    , tcpserver_(tcpserver)
{
    connect( qtcpserver, SIGNAL(newConnection()), this, SLOT(newConnection()) );
}

private slots:

void newConnection()
{ 
    tcpserver_->newConnection.trigger( *tcpserver_ );
}

private:

    QTcpServer*		qtcpserver_;
    TcpServer*		tcpserver_;

};

#endif
