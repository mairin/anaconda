/*
 * Copyright (C) 2011  Red Hat, Inc.
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
#include "HubWindow.h"
#include "intl.h"

/**
 * SECTION: HubWindow
 * @title: AnacondaHubWindow
 * @short_description: Window for displaying a Hub
 *
 * A #AnacondaHubWindow is a top-level window that displays a hub on the
 * entire screen.  A Hub allows selection of multiple configuration spokes
 * from a single interface, as well as a place to display current configuration
 * selections.
 *
 * The window consists of three areas:
 *
 * - A navigation area in the top of the screen, inherited from #AnacondaBaseWindow.
 *
 * - A selection area in the middle of the screen, taking up a majority of the space.
 *   This is where spokes will be displayed and the user can decide what to do.
 *
 * - An action area on the bottom of the screen.  This area is different for
 *   different kinds of hubs.  It may have buttons, or it may have progress
 *   information.
 *
 * <refsect2 id="AnacondaHubWindow-BUILDER-UI"><title>AnacondaHubWindow as GtkBuildable</title>
 * <para>
 * The AnacondaHubWindow implementation of the #GtkBuildable interface exposes
 * the @action_area and @scrolled_window as internal children with the names
 * "action_area" and "scrolled_window".  action_area, in this case, is largely
 * there to give a box to contain both the scrolled_window and a #GtkButtonBox.
 * </para>
 * <example>
 * <title>A <structname>AnacondaHubWindow</structname> UI definition fragment.</title>
 * <programlisting><![CDATA[
 * <object class="AnacondaHubWindow" id="hub1">
 *     <child internal-child="action_area">
 *         <object class="GtkVBox" id="vbox1">
 *             <child internal-child="scrolled_window">
 *                 <object class="GtkScrolledWindow" id="window1">
 *                     <child>...</child>
 *                 </object>
 *             </child>
 *             <child>
 *                 <object class="GtkHButtonBox" id="buttonbox1">
 *                     <child>...</child>
 *                 </object>
 *             </child>
 *         </object>
 *     </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </refsect2>
 */

struct _AnacondaHubWindowPrivate {
    GtkWidget *scrolled_window;
};

static void anaconda_hub_window_buildable_init(GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE(AnacondaHubWindow, anaconda_hub_window, ANACONDA_TYPE_BASE_WINDOW,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_BUILDABLE, anaconda_hub_window_buildable_init))

static int get_sidebar_width(GtkWidget *window) {
    GtkAllocation allocation;
    /* change value below to make sidebar bigger / smaller */
    float sidebar_width_percentage = .18;
    gtk_widget_get_allocation(window, &allocation);
    return allocation.width * sidebar_width_percentage;
}

static int get_sidebar_height(GtkWidget *window) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(window, &allocation);
    return allocation.height;
}

/* function to override default drawing to insert sidebar image */
gboolean anaconda_hub_window_on_draw(GtkWidget *win, cairo_t *cr) {

    /* calls parent class' draw handler */
    GTK_WIDGET_CLASS(anaconda_hub_window_parent_class)->draw(win,cr);

    GtkStyleContext * context = gtk_widget_get_style_context(win);
    gtk_style_context_save (context);

    gtk_style_context_add_class(context, "sidebar");
    gtk_render_background(context, cr, 0, 0, get_sidebar_width(win), get_sidebar_height(win));
    gtk_style_context_remove_class(context, "sidebar");

    gtk_style_context_add_class(context, "logo");
    gtk_render_background(context, cr, 0, 0, get_sidebar_width(win), get_sidebar_height(win));
    gtk_style_context_remove_class(context, "logo");

    gtk_style_context_restore (context);


    return TRUE; /* TRUE to avoid default draw handler */
}

/* Move base window content appropriate amount of space to the right to make room for sidebar */
static void anaconda_hub_window_size_allocate (GtkWidget *window, GtkAllocation *allocation) {
    GtkAllocation child_allocation;
    GtkWidget *child;

    gtk_widget_set_allocation(window, allocation);
    int sidebar_width = get_sidebar_width(window);
    child_allocation.x = allocation->x+sidebar_width;
    child_allocation.y = allocation->y;
    child_allocation.width = allocation->width-sidebar_width;
    child_allocation.height = allocation->height;

    child = gtk_bin_get_child (GTK_BIN (window));
    if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);
}

static void anaconda_hub_window_class_init(AnacondaHubWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->draw=anaconda_hub_window_on_draw;
    widget_class->size_allocate=anaconda_hub_window_size_allocate;
    g_type_class_add_private(object_class, sizeof(AnacondaHubWindowPrivate));
}

/**
 * anaconda_hub_window_new:
 *
 * Creates a new #AnacondaHubWindow, which is a window designed for displaying
 * multiple spokes in one location.
 *
 * Returns: A new #AnacondaHubWindow.
 */
GtkWidget *anaconda_hub_window_new() {
    return g_object_new(ANACONDA_TYPE_HUB_WINDOW, NULL);
}



static void anaconda_hub_window_init(AnacondaHubWindow *win) {
    GtkWidget *action_area = anaconda_base_window_get_action_area(ANACONDA_BASE_WINDOW(win));

    win->priv = G_TYPE_INSTANCE_GET_PRIVATE(win,
                                            ANACONDA_TYPE_HUB_WINDOW,
                                            AnacondaHubWindowPrivate);

    win->priv->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win->priv->scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(action_area), win->priv->scrolled_window, TRUE, TRUE, 0);

    /* The hub has different alignment requirements than a spoke. */
    gtk_alignment_set(GTK_ALIGNMENT(anaconda_base_window_get_alignment(ANACONDA_BASE_WINDOW(win))),
                      0.5, 0.0, 0.5, 1.0);


}

/**
 * anaconda_hub_window_get_spoke_area:
 * @win: a #AnacondaHubWindow
 *
 * Returns the scrolled window of @win where spokes may be displayed
 *
 * Returns: (transfer none): The spoke area
 *
 * Since: 1.0
 */
GtkWidget *anaconda_hub_window_get_spoke_area(AnacondaHubWindow *win) {
    return win->priv->scrolled_window;
}

static GtkBuildableIface *parent_buildable_iface;

static GObject *
anaconda_hub_window_buildable_get_internal_child (GtkBuildable *buildable,
                                                  GtkBuilder *builder,
                                                  const gchar *childname) {
    if (strcmp (childname, "scrolled_window") == 0)
        return G_OBJECT(anaconda_hub_window_get_spoke_area(ANACONDA_HUB_WINDOW(buildable)));

    return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void anaconda_hub_window_buildable_init (GtkBuildableIface *iface) {
    parent_buildable_iface = g_type_interface_peek_parent (iface);
    iface->get_internal_child = anaconda_hub_window_buildable_get_internal_child;
}
