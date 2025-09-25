// dep : gtk3 vte
// Penulis : Cilegordev & Dibuat bareng ChatGPT-4o 🤖✨

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <string.h>

GtkWidget *main_window;
GtkWidget *notebook;
GtkWidget *header_bar;
static gboolean is_fullscreen = FALSE;

typedef struct {
    GtkWidget *terminal;
    GtkWidget *tab_label;
    GPid pid;
    gchar *last_cwd;
    guint timeout_id;
} TerminalData;

// --- Forward declarations ---
static gboolean check_cwd_change(gpointer user_data);
static void update_tab_title(TerminalData *data);
static void update_tab_visibility(void);
static gboolean scroll_to_bottom(gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));
    if (adjustment) {
        gdouble upper = gtk_adjustment_get_upper(adjustment);
        gdouble page_size = gtk_adjustment_get_page_size(adjustment);
        gtk_adjustment_set_value(adjustment, upper - page_size);
    }
    return G_SOURCE_REMOVE;
}

// --- Copy/Paste callbacks ---
static void copy_cb(GtkWidget *widget, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);
}

static void paste_cb(GtkWidget *widget, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_paste_clipboard(terminal);
    g_idle_add(scroll_to_bottom, terminal);
}

// --- Right-click menu dengan ikon ---
static gboolean button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        VteTerminal *terminal = VTE_TERMINAL(user_data);
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *copy_item = gtk_image_menu_item_new_with_label("Copy");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(copy_item),
                                      gtk_image_new_from_icon_name("edit-copy", GTK_ICON_SIZE_MENU));
        g_signal_connect(copy_item, "activate", G_CALLBACK(copy_cb), terminal);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);

        GtkWidget *paste_item = gtk_image_menu_item_new_with_label("Paste");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(paste_item),
                                      gtk_image_new_from_icon_name("edit-paste", GTK_ICON_SIZE_MENU));
        g_signal_connect(paste_item, "activate", G_CALLBACK(paste_cb), terminal);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

        return TRUE;
    }
    return FALSE;
}

// --- Terminal CWD / tab title ---
static gboolean check_cwd_change(gpointer user_data) {
    TerminalData *data = user_data;
    if (data->pid <= 0) return G_SOURCE_CONTINUE;

    gchar *proc_path = g_strdup_printf("/proc/%d/cwd", data->pid);
    gchar *cwd = g_file_read_link(proc_path, NULL);
    g_free(proc_path);

    if (cwd && (!data->last_cwd || strcmp(data->last_cwd, cwd) != 0)) {
        g_free(data->last_cwd);
        data->last_cwd = cwd;
        update_tab_title(data);
    } else {
        g_free(cwd);
    }
    return G_SOURCE_CONTINUE;
}

static void update_tab_title(TerminalData *data) {
    gchar *title = "Terminal";
    if (data->last_cwd) title = g_path_get_basename(data->last_cwd);
    gtk_label_set_text(GTK_LABEL(data->tab_label), title);
    if (title != "Terminal") g_free(title);
}

static void on_cwd_changed(VteTerminal *terminal, GParamSpec *pspec, gpointer user_data) {
    TerminalData *data = user_data;
    if (data->timeout_id) g_source_remove(data->timeout_id);
    data->timeout_id = g_timeout_add(100, check_cwd_change, data);
}

// --- Terminal lifecycle ---
static void on_terminal_exit(VteTerminal *terminal, gint status, gpointer user_data) {
    TerminalData *data = user_data;
    if (data->timeout_id) g_source_remove(data->timeout_id);
    gtk_widget_destroy(gtk_widget_get_parent(GTK_WIDGET(terminal)));
    update_tab_visibility();
    g_free(data->last_cwd);
    g_free(data);
}

// --- Spawn async with debug ---
static void on_spawn_async_ready(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    TerminalData *data = user_data;
    if (error) {
        g_warning("Failed to spawn terminal: %s", error->message);
        g_error_free(error);
    } else {
        data->pid = pid;
        g_print("Terminal spawned with PID: %d\n", pid);
        data->timeout_id = g_timeout_add(100, check_cwd_change, data);
        update_tab_title(data);
        gtk_widget_grab_focus(GTK_WIDGET(terminal));
    }
}

// --- Notebook tabs ---
static GtkWidget* create_tab_label(GtkWidget *child, TerminalData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label = gtk_label_new("Terminal");
    GtkWidget *close_btn = gtk_button_new_with_label("⨯");

    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), child);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(update_tab_visibility), NULL);

    data->tab_label = label;
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(close_btn, FALSE);
    gtk_widget_set_valign(close_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), close_btn, FALSE, FALSE, 0);
    gtk_widget_show_all(box);
    return box;
}

static GtkWidget* create_terminal_tab(TerminalData **data_ptr) {
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *terminal = vte_terminal_new();
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), -1);
    gtk_container_add(GTK_CONTAINER(scrolled), terminal);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(terminal), FALSE);

    // Set font Liberation Mono 11
    PangoFontDescription *font = pango_font_description_from_string("Liberation Mono 11");
    vte_terminal_set_font(VTE_TERMINAL(terminal), font);
    pango_font_description_free(font);

    TerminalData *data = g_new0(TerminalData, 1);
    data->terminal = terminal;
    data->pid = -1;
    data->last_cwd = NULL;
    data->timeout_id = 0;
    *data_ptr = data;
    g_object_set_data(G_OBJECT(scrolled), "terminal-data", data);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    g_signal_connect(terminal, "button-press-event", G_CALLBACK(button_press_cb), terminal);
    g_signal_connect(terminal, "notify::current-directory-uri", G_CALLBACK(on_cwd_changed), data);

    gchar *cwd = g_get_current_dir();
    const gchar *shell = g_getenv("SHELL");
    if (!shell) shell = "/bin/sh";
    char *argv[] = { (char *)shell, NULL };

    vte_terminal_spawn_async(VTE_TERMINAL(terminal), VTE_PTY_DEFAULT, cwd, argv, NULL,
                             G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL,
                             on_spawn_async_ready, data);
    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_exit), data);
    g_free(cwd);

    return scrolled;
}

static void on_new_tab(GtkButton *button, gpointer user_data) {
    GtkNotebook *nb = GTK_NOTEBOOK(user_data);
    TerminalData *data = NULL;
    GtkWidget *tab = create_terminal_tab(&data);
    GtkWidget *label = create_tab_label(tab, data);
    int page_num = gtk_notebook_append_page(nb, tab, label);
    gtk_widget_show_all(tab);
    gtk_notebook_set_current_page(nb, page_num);
    gtk_widget_grab_focus(tab);
    update_tab_visibility();
}

static void update_tab_visibility(void) {
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), n > 1);
    if (n == 0) gtk_window_close(GTK_WINDOW(main_window));
}

// --- Fullscreen toggle ---
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_F11) {
        if (is_fullscreen) {
            gtk_window_unfullscreen(GTK_WINDOW(main_window));
        } else {
            gtk_window_fullscreen(GTK_WINDOW(main_window));
        }
        is_fullscreen = !is_fullscreen;
        return TRUE;
    }
    return FALSE;
}

// --- About dialog ---
static void show_about(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("_Tutup", GTK_WINDOW(main_window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Tutup", GTK_RESPONSE_CLOSE, NULL);

    gtk_window_set_title(GTK_WINDOW(dialog), "Tentang Terminal Ini");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 360, 144);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "indoterm v1.0.1");
    gtk_window_set_titlebar(GTK_WINDOW(dialog), header);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("utilities-terminal", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_halign(icon, GTK_ALIGN_START);
    gtk_widget_set_valign(icon, GTK_ALIGN_START);

    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
        "Di compile dengan : \n"
        "GCC v13.2.0\n"
        "GTK+-3.0 v3.24.28\n"
        "VTE-2.91 v0.74.2\n"
        "<a href=\"https://github.com/cilegordev/prototype/blob/main/indoterm.c\">Lihat Halaman Projek Ini</a>");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// --- App activation ---
static void activate(GtkApplication *app, gpointer user_data) {
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Indonesia Terminal");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 824, 472);
    gtk_window_set_icon_name(GTK_WINDOW(main_window), "utilities-terminal");
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);

    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "indoterm");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header_bar);

    GtkWidget *new_tab_btn = gtk_button_new();
    GtkWidget *new_tab_icon = gtk_image_new_from_icon_name("gtk-new", GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(new_tab_btn), new_tab_icon);
    gtk_button_set_relief(GTK_BUTTON(new_tab_btn), GTK_RELIEF_NONE);

    GtkWidget *about_btn = gtk_button_new();
    GtkWidget *about_icon = gtk_image_new_from_icon_name("gtk-about", GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(about_btn), about_icon);
    gtk_button_set_relief(GTK_BUTTON(about_btn), GTK_RELIEF_NONE);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), about_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), new_tab_btn);

    g_signal_connect(about_btn, "clicked", G_CALLBACK(show_about), NULL);

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_container_add(GTK_CONTAINER(main_window), notebook);

    g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(on_new_tab), notebook);
    g_signal_connect(notebook, "switch-page", G_CALLBACK(update_tab_visibility), NULL);
    g_signal_connect(main_window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    on_new_tab(GTK_BUTTON(new_tab_btn), notebook);
    gtk_widget_show_all(main_window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.indonesia.terminal", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
