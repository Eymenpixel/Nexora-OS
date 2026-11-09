#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <ctime>

static gboolean update_clock(gpointer user_data) {
    GtkLabel *clock = GTK_LABEL(user_data);
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    gtk_label_set_text(clock, buf);
    return G_SOURCE_CONTINUE;
}

static void launch_command(const char *cmd) {
    if (cmd && *cmd) {
        g_spawn_command_line_async(cmd, NULL);
    }
}

static void show_windows(GtkWidget *parent) {
    FILE *fp = popen("wmctrl -l", "r");
    std::string out;
    if (!fp) out = "wmctrl not available or failed to run.";
    else {
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) out += buf;
        pclose(fp);
        if (out.empty()) out = "No windows listed or wmctrl returned nothing.";
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Open windows", GTK_WINDOW(gtk_widget_get_toplevel(parent)), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(sw, 500, 300);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    gtk_container_add(GTK_CONTAINER(content), sw);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf, out.c_str(), -1);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Kaya Taskbar");
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 28);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add(GTK_CONTAINER(win), hbox);

    // Load logo from likely locations
    const char *paths[] = {"Main/logo.svg", "./Main/logo.svg", "../Main/logo.svg", "../../Main/logo.svg"};
    GtkWidget *logo_img = NULL;
    for (int i = 0; i < (int)(sizeof(paths)/sizeof(paths[0])); ++i) {
        if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
            logo_img = gtk_image_new_from_file(paths[i]);
            break;
        }
    }
    if (!logo_img) logo_img = gtk_label_new("Kaya");

    GtkWidget *logo_btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(logo_btn), logo_img);
    gtk_box_pack_start(GTK_BOX(hbox), logo_btn, FALSE, FALSE, 4);

    // Logo menu
    g_signal_connect(logo_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer){
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *mi1 = gtk_menu_item_new_with_label("Open xterm");
        g_signal_connect_swapped(mi1, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer){ launch_command("xterm &"); }), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi1);
        GtkWidget *mi2 = gtk_menu_item_new_with_label("Open Firefox");
        g_signal_connect_swapped(mi2, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer){ launch_command("firefox &"); }), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi2);
        GtkWidget *mi3 = gtk_menu_item_new_with_label("Show open windows");
        g_signal_connect_swapped(mi3, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer){ show_windows(NULL); }), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi3);
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_widget(GTK_MENU(menu), NULL, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    }), NULL);

    // Spacer/center area (will expand)
    GtkWidget *center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(center, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), center, TRUE, TRUE, 0);

    // Windows list button
    GtkWidget *winbtn = gtk_button_new_with_label("Windows");
    g_signal_connect_swapped(winbtn, "clicked", G_CALLBACK(+[](GtkWidget *w, gpointer){ show_windows(w); }), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), winbtn, FALSE, FALSE, 4);

    // Clock
    GtkWidget *clock = gtk_label_new("");
    gtk_box_pack_end(GTK_BOX(hbox), clock, FALSE, FALSE, 8);
    g_timeout_add_seconds(1, update_clock, clock);

    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
