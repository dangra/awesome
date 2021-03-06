/*
 * draw.c - draw functions
 *
 * Copyright © 2007-2009 Julien Danjou <julien@danjou.info>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <cairo-xcb.h>

#include "config.h"

#include <langinfo.h>
#include <iconv.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include "globalconf.h"
#include "screen.h"

#include "common/tokenize.h"
#include "common/xutil.h"

/** Convert text from any charset to UTF-8 using iconv.
 * \param iso The ISO string to convert.
 * \param len The string size.
 * \param dest The destination pointer. Memory will be allocated, up to you to
 * free, like any char *.
 * \param dlen The destination length, can be NULL.
 * \return True if conversion was done.
 */
bool
draw_iso2utf8(const char *iso, size_t len, char **dest, ssize_t *dlen)
{
    static iconv_t iso2utf8 = (iconv_t) -1;
    static int8_t dont_need_convert = -1;

    if(dont_need_convert == -1)
        dont_need_convert = !a_strcmp(nl_langinfo(CODESET), "UTF-8");

    if(!len || dont_need_convert)
        return false;

    if(iso2utf8 == (iconv_t) -1)
    {
        iso2utf8 = iconv_open("UTF-8", nl_langinfo(CODESET));
        if(iso2utf8 == (iconv_t) -1)
        {
            if(errno == EINVAL)
                warn("unable to convert text from %s to UTF-8, not available",
                     nl_langinfo(CODESET));
            else
                warn("unable to convert text: %s", strerror(errno));

            return false;
        }
    }

    size_t orig_utf8len, utf8len;
    char *utf8;

    orig_utf8len = utf8len = 2 * len + 1;
    utf8 = *dest = p_new(char, utf8len);

    if(iconv(iso2utf8, (char **) &iso, &len, &utf8, &utf8len) == (size_t) -1)
    {
        warn("text conversion failed: %s", strerror(errno));
        p_delete(dest);
        return false;
    }

    if(dlen)
        *dlen = orig_utf8len - utf8len;

    return true;
}

/** Initialize a draw_text_context_t with text data.
 * \param data The draw text context to init.
 * \param str The text string to render.
 * \param slen The text string length.
 * \return True if everything is ok, false otherwise.
 */
bool
draw_text_context_init(draw_text_context_t *data, const char *str, ssize_t slen)
{
    GError *error = NULL;

    if(!str)
        return false;

    if(!pango_parse_markup(str, slen, 0, &data->attr_list, &data->text, NULL, &error))
    {
        warn("cannot parse pango markup: %s", error ? error->message : "unknown error");
        if(error)
            g_error_free(error);
        return false;
    }

    data->len = a_strlen(data->text);

    return true;
}

/** Initialize a new draw context.
 * \param d The draw context to initialize.
 * \param phys_screen Physical screen id.
 * \param width Width.
 * \param height Height.
 * \param px Pixmap object to store.
 * \param fg Foreground color.
 * \param bg Background color.
 */
void
draw_context_init(draw_context_t *d, int phys_screen,
                  int width, int height, xcb_pixmap_t px,
                  const xcolor_t *fg, const xcolor_t *bg)
{
    d->phys_screen = phys_screen;
    d->width = width;
    d->height = height;
    d->pixmap = px;
    d->surface = cairo_xcb_surface_create(globalconf.connection,
                                          px, globalconf.screens.tab[phys_screen].visual,
                                          width, height);
    d->cr = cairo_create(d->surface);
    d->layout = pango_cairo_create_layout(d->cr);
    d->fg = *fg;
    d->bg = *bg;
};

/** Draw text into a draw context.
 * \param ctx Draw context  to draw to.
 * \param data Draw text context data.
 * \param ellip Ellipsize mode.
 * \param wrap Wrap mode.
 * \param align Text alignment.
 * \param valign Vertical text alignment.
 * \param area Area to draw to.
 */
void
draw_text(draw_context_t *ctx, draw_text_context_t *data,
          PangoEllipsizeMode ellip, PangoWrapMode wrap,
          alignment_t align, alignment_t valign, area_t area)
{
    pango_layout_set_text(ctx->layout, data->text, data->len);
    pango_layout_set_width(ctx->layout,
                           pango_units_from_double(area.width));
    pango_layout_set_height(ctx->layout, pango_units_from_double(area.height));
    pango_layout_set_ellipsize(ctx->layout, ellip);
    pango_layout_set_wrap(ctx->layout, wrap);
    pango_layout_set_attributes(ctx->layout, data->attr_list);
    pango_layout_set_font_description(ctx->layout, globalconf.font->desc);

    PangoRectangle ext;
    pango_layout_get_pixel_extents(ctx->layout, NULL, &ext);

    switch(align)
    {
      case AlignCenter:
        area.x += (area.width - ext.width) / 2;
        break;
      case AlignRight:
        area.x += area.width - ext.width;
        break;
      default:
        break;
    }

    switch(valign)
    {
      case AlignCenter:
        area.y += (area.height - ext.height) / 2;
        break;
      case AlignBottom:
        area.y += area.height - ext.height;
        break;
      default:
        break;
    }

    cairo_move_to(ctx->cr, area.x, area.y);

    cairo_set_source_rgba(ctx->cr,
                          ctx->fg.red / 65535.0,
                          ctx->fg.green / 65535.0,
                          ctx->fg.blue / 65535.0,
                          ctx->fg.alpha / 65535.0);
    pango_cairo_update_layout(ctx->cr, ctx->layout);
    pango_cairo_show_layout(ctx->cr, ctx->layout);
}

/** Draw rectangle inside the coordinates
 * \param ctx Draw context
 * \param geometry geometry
 * \param line_width line width
 * \param filled fill rectangle?
 * \param color color to use
 */
void
draw_rectangle(draw_context_t *ctx, area_t geometry,
               float line_width, bool filled, const color_t *color)
{
    cairo_set_antialias(ctx->cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width(ctx->cr, line_width);
    cairo_set_miter_limit(ctx->cr, 10.0);
    cairo_set_line_join(ctx->cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_source_rgba(ctx->cr,
                          color->red / 255.0,
                          color->green / 255.0,
                          color->blue / 255.0,
                          color->alpha / 255.0);
    if(filled)
    {
        cairo_rectangle(ctx->cr, geometry.x, geometry.y,
                        geometry.width, geometry.height);
        cairo_fill(ctx->cr);
    }
    else
    {
        cairo_rectangle(ctx->cr, geometry.x + line_width / 2.0, geometry.y + line_width / 2.0,
                        geometry.width - line_width, geometry.height - line_width);
        cairo_stroke(ctx->cr);
    }
}

/** Draw an image from ARGB data to a draw context.
 * Data should be stored as an array of alpha, red, blue, green for each pixel
 * and the array size should be w * h elements long.
 * \param ctx Draw context to draw to.
 * \param x X coordinate.
 * \param y Y coordinate.
 * \param w Width.
 * \param h Height.
 * \param ratio The ratio to apply to the image.
 * \param data The image pixels array.
 */
static void
draw_image_from_argb_data(draw_context_t *ctx, int x, int y, int w, int h,
                          double ratio, unsigned char *data)
{
    cairo_t *cr;
    cairo_surface_t *source;

    source = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, w, h,
#if CAIRO_VERSION_MAJOR < 1 || (CAIRO_VERSION_MAJOR == 1 && CAIRO_VERSION_MINOR < 5) || (CAIRO_VERSION_MAJOR == 1 && CAIRO_VERSION_MINOR == 5 && CAIRO_VERSION_MICRO < 8)
                                                 sizeof(unsigned char) * 4 * w);
#else
                                                 cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w));
#endif
    cr = cairo_create(ctx->surface);
    cairo_scale(cr, ratio, ratio);
    cairo_set_source_surface(cr, source, x / ratio, y / ratio);

    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(source);
}

/** Draw an image to a draw context.
 * \param ctx Draw context to draw to.
 * \param x X coordinate.
 * \param y Y coordinate.
 * \param ratio The ratio to apply to the image.
 * \param image The image to draw.
 */
void
draw_image(draw_context_t *ctx, int x, int y, double ratio, image_t *image)
{
    draw_image_from_argb_data(ctx, x, y, image_getwidth(image), image_getheight(image), ratio, image_getdata(image));
}

/** Rotate a pixmap.
 * \param ctx Draw context to draw with.
 * \param src Drawable to draw from.
 * \param dest Drawable to draw to.
 * \param src_w Drawable width.
 * \param src_h Drawable height.
 * \param dest_w Drawable width.
 * \param dest_h Drawable height.
 * \param angle angle to rotate.
 * \param tx Translate to this x coordinate.
 * \param ty Translate to this y coordinate.
 */
void
draw_rotate(draw_context_t *ctx,
            xcb_pixmap_t src, xcb_pixmap_t dest,
            int src_w, int src_h,
            int dest_w, int dest_h,
            double angle, int tx, int ty)
{
    cairo_surface_t *surface, *source;
    cairo_t *cr;

    surface = cairo_xcb_surface_create(globalconf.connection, dest,
                                       globalconf.screens.tab[ctx->phys_screen].visual,
                                       dest_w, dest_h);
    source = cairo_xcb_surface_create(globalconf.connection, src,
                                      globalconf.screens.tab[ctx->phys_screen].visual,
                                      src_w, src_h);
    cr = cairo_create (surface);

    cairo_translate(cr, tx, ty);
    cairo_rotate(cr, angle);

    cairo_set_source_surface(cr, source, 0.0, 0.0);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(source);
    cairo_surface_destroy(surface);
}

/** Return the width and height of a text in pixel.
 * \param data The draw context text data.
 * \return Text height and width.
 */
area_t
draw_text_extents(draw_text_context_t *data)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    PangoLayout *layout;
    PangoRectangle ext;
    xcb_screen_t *s = xutil_screen_get(globalconf.connection, globalconf.default_screen);
    area_t geom = { 0, 0, 0, 0 };

    if(data->len <= 0)
        return geom;

    surface = cairo_xcb_surface_create(globalconf.connection,
                                       globalconf.default_screen,
                                       globalconf.screens.tab[0].visual,
                                       s->width_in_pixels,
                                       s->height_in_pixels);

    cr = cairo_create(surface);
    layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, data->text, data->len);
    pango_layout_set_attributes(layout, data->attr_list);
    pango_layout_set_font_description(layout, globalconf.font->desc);
    pango_layout_get_pixel_extents(layout, NULL, &ext);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    geom.width = ext.width;
    geom.height = ext.height;

    return geom;
}

/** Transform a string to a alignment_t type.
 * Recognized string are flex, fixed, left, center, middle or right.
 * \param align A string with align text.
 * \param len The string length.
 * \return An alignment_t type.
 */
alignment_t
draw_align_fromstr(const char *align, ssize_t len)
{
    switch(a_tokenize(align, len))
    {
      case A_TK_CENTER: return AlignCenter;
      case A_TK_RIGHT:  return AlignRight;
      case A_TK_TOP:    return AlignTop;
      case A_TK_BOTTOM: return AlignBottom;
      case A_TK_MIDDLE: return AlignMiddle;
      default:          return AlignLeft;
    }
}

/** Transform an alignment to a string.
 * \param a The alignment.
 * \return A string which must not be freed.
 */
const char *
draw_align_tostr(alignment_t a)
{
    switch(a)
    {
      case AlignLeft:   return "left";
      case AlignCenter: return "center";
      case AlignRight:  return "right";
      case AlignBottom: return "bottom";
      case AlignTop:    return "top";
      case AlignMiddle: return "middle";
      default:          return NULL;
    }
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
