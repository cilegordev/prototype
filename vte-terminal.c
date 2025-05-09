// dep : gtk3 vte zsh
// Penulis : Cilegordev & Dibuat bareng GitHub Copilot 🤖✨

#include <gtk/gtk.h>
#include <vte/vte.h>

GtkWidget *notebook;
GtkWidget *main_window;
GtkWidget *header;

static void update_tab_visibility() {
    int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), page_count > 1);

    if (page_count == 0) {
        gtk_window_close(GTK_WINDOW(main_window));
    }
}

static void on_copy(GtkMenuItem *item, gpointer user_data) {
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(user_data), VTE_FORMAT_TEXT);
}

static void on_paste(GtkMenuItem *item, gpointer user_data) {
    vte_terminal_paste_clipboard(VTE_TERMINAL(user_data));
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *copy_item = gtk_menu_item_new_with_label("Copy");
        GtkWidget *paste_item = gtk_menu_item_new_with_label("Paste");

        g_signal_connect(copy_item, "activate", G_CALLBACK(on_copy), widget);
        g_signal_connect(paste_item, "activate", G_CALLBACK(on_paste), widget);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);

        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_tab_label(GtkWidget *child) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label = gtk_label_new("Tab");
    GtkWidget *close_btn = gtk_button_new_with_label("×");

    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(close_btn, FALSE);
    gtk_widget_set_valign(close_btn, GTK_ALIGN_CENTER);

    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), close_btn, FALSE, FALSE, 0);

    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), child);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(update_tab_visibility), NULL);

    gtk_widget_show_all(box);
    return box;
}

static void on_terminal_exit(VteTerminal *terminal, gint status, gpointer user_data) {
    GtkWidget *page = GTK_WIDGET(terminal);
    GtkNotebook *nb = GTK_NOTEBOOK(notebook);
    int page_count = gtk_notebook_get_n_pages(nb);

    if (page_count <= 1) {
        gtk_window_close(GTK_WINDOW(main_window));
    } else {
        gtk_widget_destroy(page);
        update_tab_visibility();
    }
}

static void on_terminal_spawn_success(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    if (error != NULL) {
        g_warning("Failed to spawn terminal: %s", error->message);
        g_error_free(error);
        return;
    }

    gtk_widget_grab_focus(GTK_WIDGET(terminal));
}

static GtkWidget* create_terminal_tab() {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *terminal = vte_terminal_new();
    //BADPIG
    gtk_container_add(GTK_CONTAINER(scrolled_window), terminal);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

    g_signal_connect(terminal, "button-press-event", G_CALLBACK(on_button_press), terminal);

    char *cwd = g_get_current_dir();
    char **argv = (char*[]){ "/bin/zsh", NULL };
    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        VTE_PTY_DEFAULT,
        cwd,
        argv,
        NULL,
        G_SPAWN_DEFAULT,
        NULL, NULL, 
        NULL, -1,
        NULL,
        on_terminal_spawn_success,
        NULL
    );

    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_exit), NULL);
    g_free(cwd);
    //BADPIG
    return scrolled_window;
}


static void on_new_tab(GtkButton *button, gpointer user_data) {
    GtkNotebook *nb = GTK_NOTEBOOK(user_data);
    GtkWidget *terminal = create_terminal_tab();
    GtkWidget *tab_label = create_tab_label(terminal);

    int page_num = gtk_notebook_append_page(nb, terminal, tab_label);

    gtk_widget_show_all(terminal);
    
    gtk_notebook_set_current_page(nb, page_num);

    gtk_widget_grab_focus(terminal);

    update_tab_visibility();
}

static void activate(GtkApplication *app, gpointer user_data) {
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "VTE Terminal");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 824, 472);

    header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "VTE Terminal");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header);

    GtkWidget *new_tab_icon = gtk_image_new_from_icon_name("gtk-new", GTK_ICON_SIZE_BUTTON);
    GtkWidget *new_tab_btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(new_tab_btn), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(new_tab_btn), new_tab_icon);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), new_tab_btn);

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_container_add(GTK_CONTAINER(main_window), notebook);

    g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(on_new_tab), notebook);

    on_new_tab(GTK_BUTTON(new_tab_btn), notebook);

    gtk_widget_show_all(main_window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.vte.terminal", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}