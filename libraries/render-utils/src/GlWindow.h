//
//  Created by Bradley Austin Davis on 2015/05/21
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_GlWindow_h
#define hifi_GlWindow_h

#include <QWindow>

class QOpenGLContext;
class QOpenGLDebugLogger;

class GlWindow : public QWindow {
public:
    GlWindow(QOpenGLContext* shareContext = nullptr);
    GlWindow(const QSurfaceFormat& format, QOpenGLContext* shareContext = nullptr);
    virtual ~GlWindow();
    void makeCurrent();
    void doneCurrent();
    void swapBuffers();
private:
    QOpenGLContext* _context{ nullptr };
#ifdef DEBUG
    QOpenGLDebugLogger* _logger{ nullptr };
#endif
};

#endif
