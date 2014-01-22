/*
 * Copyright (C) 2011-2013  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Chris Lumens <clumens@redhat.com>
 */

#include "BaseWindow.h"
#include "SpokeWindow.h"
#include "intl.h"
#include "widgets-common.h"

#include <gdk/gdkkeysyms.h>

/**
 * SECTION: SpokeWindow
 * @title: AnacondaSpokeWindow
 * @short_description: Window for displaying single spokes
 *
 * A #AnacondaSpokeWindow is a top-level window that displays a single spoke
 * on the entire screen.  Examples include the keyboard and language
 * configuration screens off the first hub.
 *
 * The window consists of two areas:
 *
 * - A navigation area in the top of the screen, inherited from #AnacondaBaseWindow
 *   and augmented with a button in the upper left corner.
 *
 * - An action area in the rest of the screen, taking up a majority of the
 *   space.  This is where widgets will be added and the user will do things.
 */

#define DEFAULT_BUTTON_LABEL _("_Done")

enum {
    SIGNAL_BUTTON_CLICKED,
    LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

struct _AnacondaSpokeWindowPrivate {
    GtkWidget  *button;
};

G_DEFINE_TYPE(AnacondaSpokeWindow, anaconda_spoke_window, ANACONDA_TYPE_BASE_WINDOW)

static void anaconda_spoke_window_realize(GtkWidget *widget, gpointer user_data);
static void anaconda_spoke_window_button_clicked(GtkButton *button,
                                                 AnacondaSpokeWindow *win);
                                                 

static int get_topbar_width(GtkWidget *window) {
	GtkAllocation allocation;
	/* change value below to make topbar bigger / smaller */
	float topbar_width_percentage = .18;  
	gtk_widget_get_allocation(window, &allocation);
	return allocation.width * topbar_width_percentage;
}

static int get_topbar_height(GtkWidget *window) {
	GtkAllocation allocation;
	gtk_widget_get_allocation(window, &allocation);
	return allocation.height;
}

/* function to override default drawing to insert topbar image */
gboolean anaconda_spoke_window_on_draw(GtkWidget *win, cairo_t *cr) {
	
	/* Create topbar */
	GTK_WIDGET_CLASS(anaconda_spoke_window_parent_class)->draw(win,cr);
	cairo_rectangle(cr, 0, 0, get_topbar_width(win), get_topbar_height(win));
	
	/* Configure topbar base color */
	/* Dark grey for RHEL:  65/255.0, 65/255.0, 62/255.0, 1 */
	/* Blue for Fedora:     60/255.0, 110/255.0, 180/255.0, 1 */
    cairo_set_source_rgba(cr, 65/255.0, 65/255.0, 62/255.0, 1); 
    cairo_fill_preserve(cr); 
    
	/* Configure topbar texture image */
	GdkPixbuf *pixbuf_background = gdk_pixbuf_new_from_file("/usr/share/anaconda/pixmaps/noise-texture.png", NULL); 
	cairo_surface_t *surface= gdk_cairo_surface_create_from_pixbuf(pixbuf_background, 0, gtk_widget_get_window(GTK_WIDGET(win))); 
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	cairo_fill(cr);
 	
    /* Configure logo image overlaid on topbar */
    GdkPixbuf *pixbuf_logo = gdk_pixbuf_new_from_file("/usr/share/anaconda/pixmaps/redhat-logo.png", NULL); 
	if (pixbuf_logo != NULL) {
		double x_value = (get_topbar_width(win) - gdk_pixbuf_get_width(pixbuf_logo))/2;
		double y_value = 20;
		cairo_surface_t *surface= gdk_cairo_surface_create_from_pixbuf(pixbuf_logo, 0, gtk_widget_get_window(GTK_WIDGET(win))); 
		cairo_set_source_surface(cr, surface, x_value, y_value);
		cairo_rectangle(cr, x_value, y_value, gdk_pixbuf_get_width(pixbuf_logo), gdk_pixbuf_get_height(pixbuf_logo));
		cairo_fill(cr);
	}
    return TRUE; /* TRUE to avoid default draw handler */
}

/* Move base window content appropriate amount of space to the right to make room for topbar */
static void anaconda_spoke_window_size_allocate (GtkWidget *window, GtkAllocation *allocation) {
	GtkAllocation child_allocation;
	GtkWidget *child;
	
	gtk_widget_set_allocation(window, allocation);
	int topbar_width = get_topbar_width(window);
	child_allocation.x = allocation->x+topbar_width;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width-topbar_width;
	child_allocation.height = allocation->height;
	
	child = gtk_bin_get_child (GTK_BIN (window));
	if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);
}

static void anaconda_spoke_window_class_init(AnacondaSpokeWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    
    widget_class->draw=anaconda_spoke_window_on_draw;
    widget_class->size_allocate=anaconda_spoke_window_size_allocate;
    klass->button_clicked = NULL;

    /**
     * AnacondaSpokeWindow::button-clicked:
     * @window: the window that received the signal
     *
     * Emitted when the button in the upper left corner has been activated
     * (pressed and released).  This is commonly the button that takes the user
     * back to the hub, but could do other things.  Note that we do not want
     * to trap people in spokes, so there should always be a way back to the
     * hub via this signal, even if it involves canceling some operation or
     * resetting things.
     *
     * Since: 1.0
     */
    window_signals[SIGNAL_BUTTON_CLICKED] = g_signal_new("button-clicked",
                                                         G_TYPE_FROM_CLASS(object_class),
                                                         G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                                         G_STRUCT_OFFSET(AnacondaSpokeWindowClass, button_clicked),
                                                         NULL, NULL,
                                                         g_cclosure_marshal_VOID__VOID,
                                                         G_TYPE_NONE, 0);

    g_type_class_add_private(object_class, sizeof(AnacondaSpokeWindowPrivate));
}

/**
 * anaconda_spoke_window_new:
 *
 * Creates a new #AnacondaSpokeWindow, which is a window designed for
 * displaying a single spoke, such as the keyboard or network configuration
 * screens.
 *
 * Returns: A new #AnacondaSpokeWindow.
 */
GtkWidget *anaconda_spoke_window_new() {
    return g_object_new(ANACONDA_TYPE_SPOKE_WINDOW, NULL);
}

static void anaconda_spoke_window_init(AnacondaSpokeWindow *win) {
    GtkWidget *nav_area;

    win->priv = G_TYPE_INSTANCE_GET_PRIVATE(win,
                                            ANACONDA_TYPE_SPOKE_WINDOW,
                                            AnacondaSpokeWindowPrivate);

    g_signal_connect(win, "map", G_CALLBACK(anaconda_spoke_window_realize), NULL);

    /* Set some default properties. */
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    /* Create the button. */
    win->priv->button = gtk_button_new_with_mnemonic(DEFAULT_BUTTON_LABEL);
    gtk_widget_set_halign(win->priv->button, GTK_ALIGN_START);
    gtk_widget_set_vexpand(win->priv->button, FALSE);
    gtk_widget_set_valign(win->priv->button, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(win->priv->button, 6);
    
    /* Set 'Done' button to blue 'suggested-action' style class */
    GtkStyleContext *context = gtk_widget_get_style_context(win->priv->button);
    gtk_style_context_add_class(context, "suggested-action");
    
    /* Hook up some signals for that button.  The signal handlers here will
     * just raise our own custom signals for the whole window.
     */
    g_signal_connect(win->priv->button, "clicked",
                     G_CALLBACK(anaconda_spoke_window_button_clicked), win);

    /* And then put the button into the navigation area. */
    nav_area = anaconda_base_window_get_nav_area(ANACONDA_BASE_WINDOW(win));
    gtk_grid_attach(GTK_GRID(nav_area), win->priv->button, 0, 1, 1, 2);
}

static void anaconda_spoke_window_realize(GtkWidget *widget, gpointer user_data) {
    GtkAccelGroup *accel_group;
    GError *error;
    GdkPixbuf *pixbuf;
    cairo_pattern_t *pattern;
    cairo_surface_t *surface;
    cairo_t *cr;
    gchar *file;

    AnacondaSpokeWindow *window = ANACONDA_SPOKE_WINDOW(widget);

    /* Set the background gradient in the header.  If we fail to load the
     * background for any reason, just print an error message and display the
     * header without an image.
     */
    error = NULL;
    file = g_strdup_printf("%s/pixmaps/anaconda_spoke_header.png", get_widgets_datadir());
    pixbuf = gdk_pixbuf_new_from_file(file, &error);
    g_free(file);
    if (!pixbuf) {
        fprintf(stderr, "could not load header background: %s\n", error->message);
        g_error_free(error);
    } else {
        GtkWidget *nav_box = anaconda_base_window_get_nav_area_background_window(ANACONDA_BASE_WINDOW(window));
        gtk_widget_set_size_request(nav_box, -1, gdk_pixbuf_get_height (pixbuf));

        surface = gdk_window_create_similar_surface(gtk_widget_get_window(nav_box), CAIRO_CONTENT_COLOR_ALPHA,
                                                    gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));
        cr = cairo_create(surface);

        gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        pattern = cairo_pattern_create_for_surface(surface);
        cairo_surface_destroy(surface);

        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
        gdk_window_set_background_pattern(gtk_widget_get_window(nav_box), pattern);
    }

    /* Pressing F12 should send you back to the hub, similar to how the old UI worked. */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
    gtk_widget_add_accelerator(window->priv->button,
                               "clicked",
                               accel_group,
                               GDK_KEY_F12,
                               0,
                               0);
}

static void anaconda_spoke_window_button_clicked(GtkButton *button,
                                                 AnacondaSpokeWindow *win) {
    g_signal_emit(win, window_signals[SIGNAL_BUTTON_CLICKED], 0);
}
