#include <glib.h>

#include "screen.h"
#include "client.h"
#include "transients.h"
#include "native_log.h"


/* Build a GList of clients that have a transient or modal relationship */
GList *
clientListTransientOrModal (Client * c)
{
    ScreenInfo *screen_info;
    Client *c2, *c3;
    GList *transients;
    GList *l1, *l2;

    g_return_val_if_fail (c != NULL, NULL);
    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    transients = g_list_append (NULL, c);

    screen_info = c->screen_info;
    for (l1 = screen_info->windows_stack; l1; l1 = g_list_next (l1))
    {
        c2 = (Client *) l1->data;
        if (c2 != c)
        {
            if (clientIsTransientOrModalFor (c2, c))
            {
                transients = g_list_append (transients, c2);
            }
            else
            {
                for (l2 = transients; l2; l2 = g_list_next (l2))
                {
                    c3 = (Client *) l2->data;
                    if ((c3 != c2) && clientIsTransientOrModalFor (c2, c3))
                    {
                        transients = g_list_append (transients, c2);
                        break;
                    }
                }
            }
        }
    }
    return transients;
}

gboolean
clientIsTransientOrModalFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    logd ("client 1 \"%s\" (0x%lx), client 2 \"%s\" (0x%lx)",
           c1->name, c1->window, c2->name, c2->window);

    return (clientIsTransientFor(c1, c2) || clientIsModalFor(c1, c2));
}

gboolean
clientIsTransientFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    logd ("client 1 \"%s\" (0x%lx), client 2 \"%s\" (0x%lx)",
           c1->name, c1->window, c2->name, c2->window);

    if (c1->transient_for)
    {
        if (c1->transient_for != c1->screen_info->xroot)
        {
            return (c1->transient_for == c2->window);
        }
        /*
         * Transients for group shouldn't apply to other transients, or
         * it breaks stacking for some apps, noticeably mozilla "save as"
         * dialog...
         */
        else if (c2->transient_for == None)
        {
            return (clientSameGroup (c1, c2));
        }
    }
    return FALSE;
}

gboolean
clientSameGroup (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    logd ("client 1 \"%s\" (0x%lx), client 2 \"%s\" (0x%lx)",
           c1->name, c1->window, c2->name, c2->window);

    return ((c1 != c2) &&
            (((c1->group_leader != None) &&
              (c1->group_leader == c2->group_leader)) ||
             (c1->group_leader == c2->window) ||
             (c2->group_leader == c1->window)));
}

gboolean
clientIsModalFor (Client * c1, Client * c2)
{
    g_return_val_if_fail (c1 != NULL, FALSE);
    g_return_val_if_fail (c2 != NULL, FALSE);

    logd ("client 1 \"%s\" (0x%lx), client 2 \"%s\" (0x%lx)",
           c1->name, c1->window, c2->name, c2->window);

    if (FLAG_TEST (c1->flags, CLIENT_FLAG_STATE_MODAL) && (c1->type & WINDOW_REGULAR_FOCUSABLE) && (c1->serial >= c2->serial))
    {
        /*
         * We used to consider all windows of the same group here
         * (ie. "clientSameGroup (c1, c2)") but that seems too
         * wide so we restric modality to transient relationship
         * for now.
         */
        return (clientIsTransientFor (c1, c2));
    }
    return FALSE;
}

gboolean
clientTransientOrModalHasAncestor (Client * c, guint ws)
{
    Client *c2;
    GList *list;
    ScreenInfo *screen_info;

    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx), workspace %u", c->name, c->window, ws);

    if (!clientIsTransientOrModal (c))
    {
        return FALSE;
    }

    screen_info = c->screen_info;
    for (list = screen_info->windows_stack; list; list = g_list_next (list))
    {
        c2 = (Client *) list->data;
        if ((c2 != c)
            && !clientIsTransientOrModal (c2)
            && clientIsTransientOrModalFor (c, c2)
            && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE)
            && (c2->win_workspace == ws)
            && (((ws == screen_info->current_ws) && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE))
                || !FLAG_TEST (c2->flags, CLIENT_FLAG_ICONIFIED)))
        {
            return TRUE;
        }
    }
    return FALSE;

}

gboolean
clientIsTransientOrModalForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return (clientIsTransientForGroup(c) || clientIsModalForGroup(c));
}

gboolean
clientIsTransientOrModal (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return (clientIsTransient(c) || clientIsModal(c));
}

gboolean
clientIsTransient (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return (clientIsDirectTransient(c) || clientIsTransientForGroup (c));
}

gboolean
clientIsDirectTransient (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return ((c->transient_for != c->screen_info->xroot) && (c->transient_for != None) && (c->transient_for != c->window));
}

gboolean
clientIsModal (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);
    /*
       If the WM_TRANSIENT_FOR hint is set to another toplevel window, the dialog is modal for that window;
       if WM_TRANSIENT_FOR is not set or set to the root window the dialog is modal for its window group.
     */
    return (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL) && (c->type & WINDOW_REGULAR_FOCUSABLE) &&
            clientIsTransient (c));
}

gboolean
clientIsTransientForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return ((c->transient_for == c->screen_info->xroot) && (c->group_leader != None) && (c->group_leader != c->window));
}

gboolean
clientIsModalForGroup (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    return (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL) && (c->type & WINDOW_REGULAR_FOCUSABLE) &&
            !clientIsTransient(c) && (c->group_leader != None));
}

gboolean
clientIsValidTransientOrModal (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);
    if (clientIsTransientOrModalForGroup (c))
    {
        ScreenInfo *screen_info = c->screen_info;
        GList *list;

        /* Look for a valid transient or modal for the same group */
        for (list = screen_info->windows_stack; list; list = g_list_next (list))
        {
            Client *c2 = (Client *) list->data;
            if (c2 != c)
            {
                if (clientIsTransientOrModalFor (c, c2))
                {
                    /* We found one, look no further */
                    return TRUE;
                }
            }
        }
    }
    else if (clientIsTransientOrModal (c))
    {
        return (clientGetTransient (c) != NULL);
    }

    return FALSE;
}

Client *
clientGetTransient (Client * c)
{
    g_return_val_if_fail (c != NULL, NULL);

    logd ("client \"%s\" (0x%lx)", c->name, c->window);

    if ((c->transient_for) && (c->transient_for != c->screen_info->xroot))
    {
        return myScreenGetClientFromWindow (c->screen_info, c->transient_for, SEARCH_WINDOW|SEARCH_FRAME);
    }
    return NULL;
}