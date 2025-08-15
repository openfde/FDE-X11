#ifndef INC_GTYPES_H
#define INC_GTYPES_H

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;
typedef glong  gssize;

typedef unsigned char   guchar;
typedef unsigned short  gushort;
typedef unsigned long   gulong;
typedef unsigned int    guint;
typedef unsigned int    guint32;

typedef float   gfloat;
typedef double  gdouble;

typedef long   GPid;

typedef guint32 gunichar;
typedef void* gpointer;
typedef struct _GList GList;
// typedef int GList;

struct _GList
{
  gpointer data;
  _GList *next;
  _GList *prev;
};
struct _GArray
{
  gchar *data;
  guint len;
};

// typedef struct _GSList GSList;
typedef long GSList;

struct _GSList
{
  gpointer data;
  GSList *next;
};

typedef size_t gsize;

typedef struct _GArray	GArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GPtrArray	GPtrArray;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif



#define _G_NEW(struct_type, n_structs, func) \
        ((struct_type *) g_##func##_n ((n_structs), sizeof (struct_type)))

#define g_new0(struct_type, n_structs)			_G_NEW (struct_type, n_structs, malloc)

#define g_new(struct_type, n_structs)			_G_NEW (struct_type, n_structs, malloc)



#endif