#include <gtk/gtk.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

struct Splash {
    std::string logo_path;
    GdkPixbuf *logo = nullptr;
    GtkWidget *area = nullptr;
    bool logo_attempted = false;
    double alpha = 0.0;
    double text_alpha = 0.0;
    gint64 start_us = 0;
    double fade_in = 1.0;
    double fade_out = 1.0;
    double text_delay = 1.0;
    double text_fade_in = 1.0;
    double total = 5.0;
};

static void add_project_logo_candidates(std::vector<fs::path>& paths,
                                        const fs::path& root) {
    if (root.empty()) return;
    paths.push_back(root / "Assets" / "gui" / "logo.png");
    paths.push_back(root / "assets" / "gui" / "logo.png");
}

static fs::path executable_directory(const char *argv0) {
    gchar *found = g_find_program_in_path(argv0);
    fs::path executable = found ? fs::path(found) : fs::path(argv0);
    g_free(found);

    std::error_code ec;
    if (executable.is_relative()) executable = fs::absolute(executable, ec);
    if (ec) return {};
    executable = fs::weakly_canonical(executable, ec);
    if (ec) return {};
    return executable.parent_path();
}

static std::string resolve_logo_path(const char *custom_path,
                                     const char *argv0) {
    std::vector<fs::path> candidates;
    if (custom_path && *custom_path) candidates.emplace_back(custom_path);

    if (const char *asset_dir = std::getenv("NEXORA_ASSETS_DIR")) {
        if (*asset_dir) {
            const fs::path root(asset_dir);
            candidates.push_back(root / "logo.png");
            add_project_logo_candidates(candidates, root);
        }
    }

    const fs::path exe_dir = executable_directory(argv0);
    add_project_logo_candidates(candidates, exe_dir);
    add_project_logo_candidates(candidates, exe_dir / "..");
    candidates.push_back(exe_dir / ".." / "share" / "nexora" / "gui" / "logo.png");
    candidates.push_back(exe_dir / "share" / "nexora" / "gui" / "logo.png");
    candidates.push_back(exe_dir / "logo.png");

    std::error_code ec;
    const fs::path current_dir = fs::current_path(ec);
    if (!ec) {
        add_project_logo_candidates(candidates, current_dir);
        candidates.push_back(current_dir / "logo.png");
    }

    for (const fs::path& candidate : candidates) {
        ec.clear();
        if (fs::is_regular_file(candidate, ec) && !ec) {
            const fs::path absolute = fs::absolute(candidate, ec);
            return ec ? candidate.string() : absolute.string();
        }
    }
    return {};
}

static double ease(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

static double elapsed_sec(Splash *s) {
    return (g_get_monotonic_time() - s->start_us) / 1000000.0;
}

static void draw_fallback_logo(cairo_t *cr, int width, int height, double alpha) {
    const double font_size = std::max(28.0, std::min(height * 0.14, 96.0));
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, font_size);

    const char *label = "NEXORA";
    cairo_text_extents_t ext{};
    cairo_text_extents(cr, label, &ext);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    cairo_move_to(cr, (width - ext.width) / 2.0 - ext.x_bearing,
                  height * 0.52);
    cairo_show_text(cr, label);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    Splash *s = static_cast<Splash *>(data);
    const int width = gtk_widget_get_allocated_width(widget);
    const int height = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    if (!s->logo_attempted) {
        s->logo_attempted = true;
        if (!s->logo_path.empty()) {
            GError *err = nullptr;
            s->logo = gdk_pixbuf_new_from_file_at_size(
                s->logo_path.c_str(), static_cast<int>(width * 0.8),
                static_cast<int>(height * 0.55), &err);
            if (err) {
                g_printerr("Logo yuklenemedi (%s): %s\n",
                           s->logo_path.c_str(), err->message);
                g_error_free(err);
            }
        } else {
            g_printerr("Logo bulunamadi. Assets/gui/logo.png konumunu kontrol edin.\n");
        }
        if (s->start_us == 0) s->start_us = g_get_monotonic_time();
    }

    if (s->logo) {
        const int logo_width = gdk_pixbuf_get_width(s->logo);
        const int logo_height = gdk_pixbuf_get_height(s->logo);
        gdk_cairo_set_source_pixbuf(cr, s->logo,
                                    (width - logo_width) / 2.0,
                                    (height - logo_height) / 2.0);
        cairo_paint_with_alpha(cr, s->alpha);
    } else {
        draw_fallback_logo(cr, width, height, s->alpha);
    }

    const double font_size = std::max(14.0, height * 0.022);
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, font_size);

    const char *text = "Powered by BlackBit";
    cairo_text_extents_t ext{};
    cairo_text_extents(cr, text, &ext);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, s->text_alpha);
    cairo_move_to(cr, (width - ext.width) / 2.0 - ext.x_bearing,
                  height - height * 0.05);
    cairo_show_text(cr, text);

    return FALSE;
}

static gboolean on_tick(gpointer data) {
    Splash *s = static_cast<Splash *>(data);
    if (s->start_us == 0) return G_SOURCE_CONTINUE;

    const double t = elapsed_sec(s);
    if (s->total > 0 && t >= s->total) {
        gtk_main_quit();
        return G_SOURCE_REMOVE;
    }

    double fade_out_alpha = 1.0;
    if (s->total > 0) fade_out_alpha = ease((s->total - t) / s->fade_out);
    s->alpha = std::min(ease(t / s->fade_in), fade_out_alpha);
    s->text_alpha = std::min(ease((t - s->text_delay) / s->text_fade_in),
                             fade_out_alpha);
    gtk_widget_queue_draw(s->area);
    return G_SOURCE_CONTINUE;
}

static gboolean on_key(GtkWidget *, GdkEventKey *event, gpointer data) {
    Splash *s = static_cast<Splash *>(data);
    if (event->keyval == GDK_KEY_Escape) {
        if (s->start_us == 0) {
            gtk_main_quit();
        } else {
            const double t = elapsed_sec(s);
            if (s->total <= 0 || s->total - t > s->fade_out)
                s->total = t + s->fade_out;
        }
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    Splash splash;
    splash.logo_path = resolve_logo_path(argc > 1 ? argv[1] : nullptr, argv[0]);
    splash.total = argc > 2 ? std::atof(argv[2]) : 5.0;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Nexora");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);

    splash.area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), splash.area);
    g_signal_connect(splash.area, "draw", G_CALLBACK(on_draw), &splash);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), &splash);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    gtk_widget_show_all(window);

    GdkDisplay *display = gdk_display_get_default();
    GdkCursor *blank = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
    gdk_window_set_cursor(gtk_widget_get_window(window), blank);
    g_object_unref(blank);

    g_timeout_add(16, on_tick, &splash);
    gtk_main();

    if (splash.logo) g_object_unref(splash.logo);
    return 0;
}
