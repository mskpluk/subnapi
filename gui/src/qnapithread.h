/*****************************************************************************
** QNapi
** Copyright (C) 2008-2017 Piotr Krzemiński <pio.krzeminski@gmail.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
*****************************************************************************/

#ifndef __QNAPITHREAD__H__
#define __QNAPITHREAD__H__

#include <QThread>

#include <atomic>

class QNapiThread : public QThread {
  Q_OBJECT
 public:
  QNapiThread() : abort(false) {}
  void requestAbort() { abort = true; }

 protected:
  // Set from the GUI thread, read by the worker: atomic, or the write is a
  // data race the compiler may hoist out of the worker's loop.
  std::atomic<bool> abort;
};

#endif
