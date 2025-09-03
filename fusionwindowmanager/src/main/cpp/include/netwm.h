#ifndef INC_NETWM_H
#define INC_NETWM_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "glib.h"
#include "screen.h"
#include "client.h"

#ifndef Button7
#define Button7 7
#endif

#define NET_WINDOW_ACTION_UNDEFINED 0
#define NET_WINDOW_ACTION_MAXIMIZED 1000
#define NET_WINDOW_ACTION_MAXIMIZED_REMOVE 1001
#define NET_WINDOW_ACTION_MINIMIZE 1003
#define NET_WINDOW_ACTION_MINIMIZE_REMOVE 1004
#define NET_WINDOW_ACTION_MAXIMIZED_HORZ 1
#define NET_WINDOW_ACTION_MAXIMIZED_VERT 2
#define NET_WINDOW_ACTION_DELETE 1007

void                     clientSetNetState                      (Client *);
void                     clientGetNetState                      (Client *);
void                     clientUpdateNetWmDesktop               (Client *,
                                                                 XClientMessageEvent *);
int                     clientUpdateNetState                   (Client *,
                                                                 XClientMessageEvent *);
void                     clientNetMoveResize                    (Client *,
                                                                 XClientMessageEvent *);
void                     clientNetMoveResizeWindow              (Client *,
                                                                 XClientMessageEvent *);
void                     clientUpdateFullscreenState            (Client *);
void                     clientGetNetWmType                     (Client *);
void                     clientGetInitialNetWmDesktop           (Client *);
void                     clientSetNetClientList                 (ScreenInfo *,
                                                                 Atom,
                                                                 GList *);
gboolean                 clientValidateNetStrut                 (Client *);
gboolean                 clientGetNetStruts                     (Client *);
void                     clientSetNetActions                    (Client *);
void                     clientWindowType                       (Client *);
void                     clientUpdateLayerState                 (Client *);
void                     clientSetNetActiveWindow               (ScreenInfo *,
                                                                 Client *,
                                                                 guint32);
void                     clientHandleNetActiveWindow            (Client *,
                                                                 guint32,
                                                                 gboolean);
void                     clientRemoveNetWMPing                  (Client *);
gboolean                 clientSendNetWMPing                    (Client *,
                                                                 guint32);
void                     clientReceiveNetWMPong                 (ScreenInfo *,
                                                                 guint32);
gboolean                 clientGetUserTime                      (Client *);
void                     clientAddUserTimeWin                   (Client *);
void                     clientRemoveUserTimeWin                (Client *);

#endif /* INC_NETWM_H */
