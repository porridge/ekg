#ifndef UI_GTK_PALETTE_H
#define UI_GTK_PALETTE_H

extern GdkColor colors[];

#define COL_MARK_FG 32
#define COL_MARK_BG 33
#define COL_FG 34
#define COL_BG 35
#define COL_MARKER 36
#define COL_NEW_DATA 37
#define COL_HILIGHT 38
#define COL_NEW_MSG 39
#define COL_AWAY 40

void palette_alloc(GtkWidget *widget);
void pixmaps_init(void);

extern GdkPixbuf *pix_ekg;
extern GdkPixbuf *gg_pixs[];

#define PIXBUF_AVAIL 0
#define PIXBUF_AWAY 1
#define PIXBUF_INVISIBLE 2
#define PIXBUF_BLOCKED 3
#define PIXBUF_NOTAVAIL 4

#define STATUS_PIXBUFS 5 /* AVAIL, AWAY, INVISIBLE, BLOCKED, NOTAVAIL */

#endif
