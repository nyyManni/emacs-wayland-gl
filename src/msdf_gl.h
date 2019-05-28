#ifndef MSDF_GL_H
#define MSDF_GL_H

/**
 * OpenGL implementation for multi-channel signed distance field generator
 * ---------------------------------------------------------------
 *
 * MSDF-GL             Henrik Nyman,    (c) 2019 -
 * Original msdfgen by Viktor Chlumsky, (c) 2014 - 2019
 *
 * The technique used to generate multi-channel distance fields in this code has
 * been developed by Viktor Chlumsky in 2014 for his master's thesis, "Shape
 * Decomposition for Multi-Channel Distance Fields". It provides improved
 * quality of sharp corners in glyphs and other 2D shapes in comparison to
 * monochrome distance fields. To reconstruct an image of the shape, apply the
 * median of three operation on the triplet of sampled distance field values.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <GL/gl.h>

#include <msdf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _msdf_gl_context *msdf_gl_context_t;

typedef GLfloat vec4[4];
typedef vec4 mat4[4];

/**
 * Compile shaders and configure uniforms.
 *
 * Returns a new MSDF GL context, or NULL if creating the context failed.
 */
msdf_gl_context_t msdf_gl_create_context();

/**
 * Release resources allocated by `msdf_gl_crate_context`.
 */
void msdf_gl_destroy_context(msdf_gl_context_t ctx);

typedef struct _msdf_gl_font {
    char *font_name;

    double scale;
    double range;
    size_t texture_size;
    
    double vertical_advance;
    double *horizontal_advances;
    
    mat4 projection;

    /**
     * 2D RGBA atlas texture containing all MSDF-glyph bitmaps.
     */
    GLuint atlas_texture;
    GLuint _atlas_framebuffer;

    /**
     * 1D buffer containing glyph position information per character in the
     * atlas texture.
     */
    GLuint index_texture;
    GLuint _index_buffer;

    /**
     * MSDF_GL context handle.
     */
    msdf_gl_context_t context;

    /**
     * FreeType Face handle.
     */
    FT_Face face;

    /**
     * The location in the atlas where the next bitmap would be rendered.
     */
    size_t __offset_y;
    size_t __offset_x;

    /**
     * Texture buffer objects for serialized FreeType data input.
     */
    GLuint __meta_input_buffer;
    GLuint __point_input_buffer;
    GLuint __meta_input_texture;
    GLuint __point_input_texture;

    msdf_font_handle __msdf_font;

} * msdf_gl_font_t;

/**
 * Load font from a font file and generate textures and buffers for it.
 */
msdf_gl_font_t msdf_gl_load_font(msdf_gl_context_t ctx, const char *font_name,
                                 double range, double scale, size_t texture_size);

/**
 * Release resources allocated by `msdf_gl_load_font`.
 */
void msdf_gl_destroy_font(msdf_gl_font_t font);

/**
 * Render a single glyph onto the MSFD atlas. Intented use case is to generate
 * the bitmaps on-demand as the characters are appearing.
 */
int msdf_gl_render_glyph(msdf_gl_font_t font, int32_t char_code);

/**
 * Render a range of glyphs onto the MSFD atlas. The range is inclusive. Intended
 * use case is to initialize the atlas in the beginning with e.g. ASCII characters.
 */
int msdf_gl_render_glyphs(msdf_gl_font_t font, int32_t start, int32_t end);

/**
 * Render arbitrary character codes in bulk.
 */
int msdf_gl_render_glyph_list(msdf_gl_font_t font, int32_t *list, size_t n);

/**
 * Shortcuts for common needs.
 */
#define msdf_gl_generate_ascii(font) msdf_gl_render_glyphs(font, 0, 128)
#define msdf_gl_generate_ascii_ext(font) msdf_gl_render_glyphs(font, 0, 256)

#ifdef __cplusplus
}
#endif

#endif /* MSDF_GL_H */
