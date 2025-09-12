#ifndef INC_MOVERESIZE_H
#define INC_MOVERESIZE_H

#include <X11/X.h>
#include <X11/Xlib.h>

#include "screen.h"
#include "client.h"
#include "device.h"

int                      clientCheckWidth                       (Client *,
                                                                 int,
                                                                 gboolean);
int                      clientCheckHeight                      (Client *,
                                                                 int,
                                                                 gboolean);
void                     clientMoveWarp                         (Client *,
                                                                 ScreenInfo *,
                                                                 int *,
                                                                 int *,
                                                                 guint32);
void                     clientMove                             (Client *,
                                                                 XfwmEventButton *);
void                     clientResize                           (Client *,
                                                                 int,
                                                                 XfwmEventButton *);

#endif /* INC_MOVERESIZE_H */