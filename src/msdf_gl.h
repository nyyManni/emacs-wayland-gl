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

struct _msdf_gl_context {
    GLuint gen_shader;

    GLuint _projection_uniform;
    GLuint _texture_offset_uniform;
    GLuint _translate_uniform;
    GLuint _scale_uniform;
    GLuint _range_uniform;
    GLuint _glyph_height_uniform;

    GLuint _meta_offset_uniform;
    GLuint _point_offset_uniform;

    GLuint metadata_uniform;
    GLuint point_data_uniform;

    GLuint render_shader;

    GLuint window_projection_uniform;
    GLuint _font_projection_uniform;
    GLuint _font_vertex_uniform;
    GLuint _font_texture_uniform;
    GLuint _padding_uniform;
    GLuint _offset_uniform;
};
typedef struct _msdf_gl_context *msdf_gl_context_t;


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

struct _msdf_gl_font {
    char *font_name;

    double scale;
    double range;
    int texture_size;

    double vertical_advance;
    float horizontal_advances[256];

    GLfloat projection[4][4];

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
    size_t _offset_y;
    size_t _offset_x;

    /**
     * Texture buffer objects for serialized FreeType data input.
     */
    GLuint _meta_input_buffer;
    GLuint _point_input_buffer;
    GLuint _meta_input_texture;
    GLuint _point_input_texture;

    msdf_font_handle _msdf_font;

};
typedef struct _msdf_gl_font *msdf_gl_font_t;

struct msdf_gl_glyph {
    GLfloat x;
    GLfloat y; 
    GLuint color;
    GLint key;
    GLfloat size;
    GLfloat offset;
    GLfloat skew;
    GLfloat strength;
};

/**
 * Load font from a font file and generate textures and buffers for it.
 */
msdf_gl_font_t msdf_gl_load_font(msdf_gl_context_t ctx, const char *font_name,
                                 double range, double scale, int texture_size);

/**
 * Release resources allocated by `msdf_gl_load_font`.
 */
void msdf_gl_destroy_font(msdf_gl_font_t font);

/**
 * Render a single glyph onto the MSFD atlas. Intented use case is to generate
 * the bitmaps on-demand as the characters are appearing.
 */
int msdf_gl_generate_glyph(msdf_gl_font_t font, int32_t char_code);

/**
 * Render a range of glyphs onto the MSFD atlas. The range is inclusive. Intended
 * use case is to initialize the atlas in the beginning with e.g. ASCII characters.
 */
int msdf_gl_generate_glyphs(msdf_gl_font_t font, int32_t start, int32_t end);

/**
 * Render arbitrary character codes in bulk.
 */
int msdf_gl_generate_glyph_list(msdf_gl_font_t font, int32_t *list, size_t n);

/**
 * Shortcuts for common needs.
 */
#define msdf_gl_generate_ascii(font) msdf_gl_generate_glyphs(font, 0, 128)
#define msdf_gl_generate_ascii_ext(font) msdf_gl_generate_glyphs(font, 0, 256)

#ifdef __cplusplus
}
#endif

#endif /* MSDF_GL_H */
