#ifndef INC_TRANSIENTS_H
#define INC_TRANSIENTS_H

#include <glib.h>
#include "client.h"

GList                   *clientListTransientOrModal             (Client *);
gboolean                 clientIsTransientOrModalFor            (Client *,
                                                                 Client *);
gboolean                 clientSameGroup                        (Client *,
                                                                 Client *);
gboolean                 clientIsModalFor                       (Client *,
                                                                 Client *);
gboolean                 clientTransientOrModalHasAncestor      (Client *,
                                                                 guint);
gboolean                 clientIsTransientOrModalForGroup       (Client *);
gboolean                 clientIsTransientOrModal               (Client *);
gboolean                 clientIsDirectTransient                (Client *);
gboolean                 clientIsTransient                      (Client *);
gboolean                 clientIsModal                          (Client *);
gboolean                 clientIsTransientForGroup              (Client *);
gboolean                 clientIsModalForGroup                  (Client *);
gboolean                 clientIsValidTransientOrModal          (Client *);
Client                  *clientGetTransient                     (Client *);

 #endif /* INC_TRANSIENTS_H */