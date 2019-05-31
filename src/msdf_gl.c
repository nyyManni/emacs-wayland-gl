
#ifdef __linux__
#define GL_GLEXT_PROTOTYPES
#else
/* Figure out something. */
#endif

#define CHECK_ERROR                                                  \
    do {                                                             \
        GLenum err = glGetError();                                   \
        if (err) {                                                   \
            fprintf(stderr, "line %d error: %x \n", __LINE__, err);  \
        }                                                            \
    } while (0);

#include "msdf_gl.h"

#include "_msdf_shaders.h"

typedef struct msdf_gl_index_entry {
    GLfloat offset_x;
    GLfloat offset_y;
    GLfloat size_x;
    GLfloat size_y;
    GLfloat bearing_x;
    GLfloat bearing_y;
    GLfloat glyph_width;
    GLfloat glyph_height;
} msdf_gl_index_entry;

struct _msdf_gl_context {
    GLuint gen_shader;

    GLint _atlas_projection_uniform;
    GLint _texture_offset_uniform;
    GLint _translate_uniform;
    GLint _scale_uniform;
    GLint _range_uniform;
    GLint _glyph_height_uniform;

    GLint _meta_offset_uniform;
    GLint _point_offset_uniform;

    GLint metadata_uniform;
    GLint point_data_uniform;

    GLuint render_shader;

    GLint window_projection_uniform;
    GLint _font_atlas_projection_uniform;
    GLint _index_uniform;
    GLint _atlas_uniform;
    GLint _padding_uniform;
    GLint _offset_uniform;
};

GLfloat _MAT4_ZERO_INIT[4][4] = {{0.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f}};

static inline void _ortho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
                          GLfloat nearVal, GLfloat farVal, GLfloat dest[][4]) {
    GLfloat rl, tb, fn;

    memcpy(dest, _MAT4_ZERO_INIT, sizeof(_MAT4_ZERO_INIT));

    rl = 1.0f / (right - left);
    tb = 1.0f / (top - bottom);
    fn = -1.0f / (farVal - nearVal);

    dest[0][0] = 2.0f * rl;
    dest[1][1] = 2.0f * tb;
    dest[2][2] = 2.0f * fn;
    dest[3][0] = -(right + left) * rl;
    dest[3][1] = -(top + bottom) * tb;
    dest[3][2] = (farVal + nearVal) * fn;
    dest[3][3] = 1.0f;
}


int compile_shader(const char *source, GLenum type, GLuint *shader) {
    *shader = glCreateShader(type);
    if (!*shader) {
        fprintf(stderr, "failed to create shader\n");
    }

    glShaderSource(*shader, 1, (const char *const *)&source, NULL);
    glCompileShader(*shader);

    GLint status;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        return 0;
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(*shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling: %*s\n",
			len, log);
    }

    return 1;
}

msdf_gl_context_t msdf_gl_create_context() {
    msdf_gl_context_t ctx = (msdf_gl_context_t)malloc(sizeof(struct _msdf_gl_context));

    if (!ctx)
        return NULL;

    GLuint vertex_shader, geometry_shader, fragment_shader;
    if (!compile_shader(_msdf_vertex, GL_VERTEX_SHADER, &vertex_shader))
        return NULL;
    if (!compile_shader(_msdf_fragment, GL_FRAGMENT_SHADER, &fragment_shader))
        return NULL;

    ctx->gen_shader = glCreateProgram();
    if (!(ctx->gen_shader = glCreateProgram()))
        return NULL;

    glAttachShader(ctx->gen_shader, vertex_shader);
    glAttachShader(ctx->gen_shader, fragment_shader);

    glLinkProgram(ctx->gen_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint status;
    glGetProgramiv(ctx->gen_shader, GL_LINK_STATUS, &status);
    if (!status)
        return NULL;

    ctx->_atlas_projection_uniform = glGetUniformLocation(ctx->gen_shader, "projection");
    ctx->_texture_offset_uniform = glGetUniformLocation(ctx->gen_shader, "offset");
    ctx->_translate_uniform = glGetUniformLocation(ctx->gen_shader, "translate");
    ctx->_scale_uniform = glGetUniformLocation(ctx->gen_shader, "scale");
    ctx->_range_uniform = glGetUniformLocation(ctx->gen_shader, "range");
    ctx->_glyph_height_uniform = glGetUniformLocation(ctx->gen_shader, "glyph_height");

    ctx->_meta_offset_uniform = glGetUniformLocation(ctx->gen_shader, "meta_offset");
    ctx->_point_offset_uniform = glGetUniformLocation(ctx->gen_shader, "point_offset");

    ctx->metadata_uniform = glGetUniformLocation(ctx->gen_shader, "metadata");
    ctx->point_data_uniform = glGetUniformLocation(ctx->gen_shader, "point_data");

    GLenum err = glGetError();
    if (err) {
        fprintf(stderr, "error: %x \n", err);
        glDeleteProgram(ctx->gen_shader);
        return NULL;
    }

    if (!compile_shader(_font_vertex, GL_VERTEX_SHADER, &vertex_shader))
        return NULL;
    if (!compile_shader(_font_geometry, GL_GEOMETRY_SHADER, &geometry_shader))
        return NULL;
    if (!compile_shader(_font_fragment, GL_FRAGMENT_SHADER, &fragment_shader))
        return NULL;

    if (!(ctx->render_shader = glCreateProgram()))
        return NULL;
    glAttachShader(ctx->render_shader, vertex_shader);
    glAttachShader(ctx->render_shader, geometry_shader);
    glAttachShader(ctx->render_shader, fragment_shader);

    glLinkProgram(ctx->render_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(geometry_shader);
    glDeleteShader(fragment_shader);

    glGetProgramiv(ctx->render_shader, GL_LINK_STATUS, &status);

    if (!status) {
        glDeleteProgram(ctx->gen_shader);
        return NULL;
    }

    ctx->window_projection_uniform = glGetUniformLocation(ctx->render_shader, "projection");
    ctx->_font_atlas_projection_uniform = glGetUniformLocation(ctx->render_shader, "font_projection");
    ctx->_index_uniform = glGetUniformLocation(ctx->render_shader, "font_index");
    ctx->_atlas_uniform = glGetUniformLocation(ctx->render_shader, "font_atlas");
    ctx->_padding_uniform = glGetUniformLocation(ctx->render_shader, "padding");
    ctx->_offset_uniform = glGetUniformLocation(ctx->render_shader, "offset");

    if ((err = glGetError())) {
        fprintf(stderr, "error: %x \n", err);
        glDeleteProgram(ctx->gen_shader);
        glDeleteProgram(ctx->render_shader);
        return NULL;
    }

    return ctx;
}


msdf_gl_font_t msdf_gl_load_font(msdf_gl_context_t ctx, const char *font_name,
                                 double range, double scale, int texture_size) {

    msdf_gl_font_t f = (msdf_gl_font_t)malloc(sizeof(struct _msdf_gl_font) * 2);

    f->_msdf_font = msdf_load_font(font_name);

    f->scale = scale;
    f->range = range;
    f->texture_size = texture_size;
    f->context = ctx;
    f->nglyphs = 0;

    glGenBuffers(1, &f->_meta_input_buffer);
    glGenBuffers(1, &f->_point_input_buffer);
    glGenTextures(1, &f->_meta_input_texture);
    glGenTextures(1, &f->_point_input_texture);

    glGenBuffers(1, &f->_index_buffer);
    glGenTextures(1, &f->index_texture);

    glGenTextures(1, &f->atlas_texture);
    glGenFramebuffers(1, &f->_atlas_framebuffer);

    return f;
}

int msdf_gl_generate_glyphs(msdf_gl_font_t font, int32_t start, int32_t end) {

    msdf_gl_context_t ctx = font->context;

    if (end - start <= 0)
        return -1;

    /* Calculate the amount of memory needed on the GPU.*/
    size_t *meta_sizes = (size_t *)malloc((end - start + 1) * sizeof(size_t));
    size_t *point_sizes = (size_t *)malloc((end - start + 1) * sizeof(size_t));
    msdf_gl_index_entry *atlas_index =
        (msdf_gl_index_entry *)malloc((end - start + 1) * sizeof(msdf_gl_index_entry));

    size_t meta_size_sum = 0, point_size_sum = 0;
    for (size_t i = 0; i <= end - start; ++i) {
        msdf_glyph_buffer_size(font->_msdf_font, start + i, &meta_sizes[i],
                               &point_sizes[i]);
        meta_size_sum += meta_sizes[i];
        point_size_sum += point_sizes[i];
    }

    /* Allocate the calculated amount. */
    void *point_data = malloc(point_size_sum);
    void *metadata = malloc(meta_size_sum);

    /* Serialize the glyphs into RAM. */
    font->vertical_advance = font->_msdf_font->height;
    char *meta_ptr = metadata;
    char *point_ptr = point_data;
    float offset_x = 1, offset_y = 1, y_increment = 0;
    size_t index_size_sum = 0;
    for (size_t i = 0; i <= end - start; ++i) {
        float glyph_width, glyph_height, buffer_width, buffer_height;
        float bearing_x, bearing_y, advance;
        msdf_serialize_glyph(font->_msdf_font, start + i, meta_ptr, point_ptr,
                             &glyph_width, &glyph_height, &bearing_x, &bearing_y,
                             &advance);

        buffer_width = (glyph_width + font->range) * font->scale;
        buffer_height = (glyph_height + font->range) * font->scale;

        meta_ptr += meta_sizes[i];
        point_ptr += point_sizes[i];

        if (offset_x + buffer_width > font->texture_size) {
            offset_y += (y_increment + 1);
            offset_x = 1;
            y_increment = 0;
        }
        y_increment = buffer_height > y_increment ? buffer_height : y_increment;

        atlas_index[i].offset_x = offset_x;
        atlas_index[i].offset_y = offset_y;
        atlas_index[i].size_x = buffer_width;
        atlas_index[i].size_y = buffer_height;
        atlas_index[i].bearing_x = bearing_x;
        atlas_index[i].bearing_y = bearing_y;
        atlas_index[i].glyph_width = glyph_width;
        atlas_index[i].glyph_height = glyph_height;
        font->horizontal_advances[i + start] = advance;

        offset_x += buffer_width + 1;
        index_size_sum += 8 * sizeof(GLfloat);
    }

    /* Allocate and fill the buffers on GPU. */
    glBindBuffer(GL_ARRAY_BUFFER, font->_meta_input_buffer);
    glBufferData(GL_ARRAY_BUFFER, meta_size_sum, metadata, GL_STATIC_READ);

    glBindBuffer(GL_ARRAY_BUFFER, font->_point_input_buffer);
    glBufferData(GL_ARRAY_BUFFER, point_size_sum, point_data, GL_STATIC_READ);

    glBindBuffer(GL_ARRAY_BUFFER, font->_index_buffer);
    /* glBufferData(GL_ARRAY_BUFFER, sizeof(atlas_index), atlas_index, GL_STATIC_READ); */
    glBufferData(GL_ARRAY_BUFFER, index_size_sum, atlas_index, GL_STATIC_READ);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Link sampler textures to the buffers. */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, font->_meta_input_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, font->_meta_input_buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, font->_point_input_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, font->_point_input_buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_BUFFER, font->index_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, font->_index_buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);

    /* Generate the atlas texture and bind it as the framebuffer. */
    glBindTexture(GL_TEXTURE_2D, font->atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, font->texture_size, 
                 font->texture_size, 0, GL_RGBA, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, font->_meta_input_texture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, font->_point_input_texture);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, font->_atlas_framebuffer);

    glUseProgram(ctx->gen_shader);
    glUniform1i(ctx->metadata_uniform, 0);
    glUniform1i(ctx->point_data_uniform, 1);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    _ortho(-font->texture_size, font->texture_size, -font->texture_size,
           font->texture_size, -1.0, 1.0, font->atlas_projection);

    GLfloat msdf_projection[4][4];
    _ortho(0, font->texture_size, 0, font->texture_size, -1.0, 1.0, msdf_projection);
    glUniformMatrix4fv(ctx->_atlas_projection_uniform, 1, GL_FALSE, (GLfloat *)msdf_projection);

    glUniform2f(ctx->_scale_uniform, font->scale, font->scale);
    glUniform1f(ctx->_range_uniform, font->range);
    glUniform1i(ctx->_meta_offset_uniform, 0);
    glUniform1i(ctx->_point_offset_uniform, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           font->atlas_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("framebuffer incomplete: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, font->texture_size, font->texture_size);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint _vbo;
    glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);

    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), 0, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(0);

    int meta_offset = 0;
    int point_offset = 0;
    for (int i = 0; i <= end - start; ++i) {
        msdf_gl_index_entry g = atlas_index[i];
        float w = g.size_x;
        float h = g.size_y;
        GLfloat bounding_box[] = {0, 0, w, 0, 0, h, 0, h, w, 0, w, h};
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bounding_box), bounding_box);

        glUniform2f(ctx->_translate_uniform, -g.bearing_x + font->range / 2.0,
                    g.glyph_height - g.bearing_y + font->range / 2.0);

        glUniform2f(ctx->_texture_offset_uniform, g.offset_x, g.offset_y);
        glUniform1i(ctx->_meta_offset_uniform, meta_offset);
        glUniform1i(ctx->_point_offset_uniform, point_offset / (2 * sizeof(GLfloat)));
        glUniform1f(ctx->_glyph_height_uniform, g.size_y);

        /* Do not bother rendering control characters */
        /* if (i > 31 && !(i > 126 && i < 160)) */
        glDrawArrays(GL_TRIANGLES, 0, 6);

        meta_offset += meta_sizes[i];
        point_offset += point_sizes[i];
    }

    glDisableVertexAttribArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    
    glUseProgram(0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (meta_sizes)
        free(meta_sizes);
    if (point_sizes)
        free(point_sizes);
    if (atlas_index)
        free(atlas_index);

    return end - start;
}

void msdf_gl_destroy_context(msdf_gl_context_t ctx) {

    if (!ctx)
        return;

    glDeleteProgram(ctx->gen_shader);
    glDeleteProgram(ctx->render_shader);

    free(ctx);
}

void msdf_gl_destroy_font(msdf_gl_font_t font) {
    
    glDeleteBuffers(1, &font->_meta_input_buffer);
    glDeleteBuffers(1, &font->_point_input_buffer);
    glDeleteTextures(1, &font->_meta_input_texture);
    glDeleteTextures(1, &font->_point_input_texture);

    glDeleteBuffers(1, &font->_index_buffer);
    glDeleteTextures(1, &font->index_texture);

    glDeleteTextures(1, &font->atlas_texture);
    glDeleteFramebuffers(1, &font->_atlas_framebuffer);

    free(font);
}

void msdf_gl_render(msdf_gl_font_t font, msdf_gl_glyph_t *glyphs, int n,
                    GLfloat *projection) {
    
    GLuint glyph_buffer;
    GLuint vao;
    glGenBuffers(1, &glyph_buffer);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, glyph_buffer);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(struct  _msdf_gl_glyph), 
                 &glyphs[0], GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct _msdf_gl_glyph),
                          (void *)offsetof(struct _msdf_gl_glyph, x));

    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, sizeof(struct _msdf_gl_glyph),
                           (void *)offsetof(struct _msdf_gl_glyph, color));

    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(struct _msdf_gl_glyph),
                           (void *)offsetof(struct _msdf_gl_glyph, key));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(struct _msdf_gl_glyph),
                          (void *)offsetof(struct _msdf_gl_glyph, size));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(struct _msdf_gl_glyph),
                          (void *)offsetof(struct _msdf_gl_glyph, offset));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT,GL_FALSE, sizeof(struct _msdf_gl_glyph),
                          (void *)offsetof(struct _msdf_gl_glyph, skew));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT,GL_FALSE, sizeof(struct _msdf_gl_glyph),
                          (void *)offsetof(struct _msdf_gl_glyph, strength));

    glUseProgram(font->context->render_shader);

    /* Bind atlas texture and index buffer. */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->atlas_texture);
    glUniform1i(font->context->_atlas_uniform, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, font->index_texture);
    glUniform1i(font->context->_index_uniform, 1);

    glUniformMatrix4fv(font->context->_font_atlas_projection_uniform, 1, GL_FALSE, 
                       (GLfloat *)font->atlas_projection);

    glUniformMatrix4fv(font->context->window_projection_uniform, 1, GL_FALSE, projection);
    
    /* TODO: Window offset. */
    glUniform2f(font->context->_offset_uniform, 0.0, 0.0);

    glUniform1f(font->context->_padding_uniform, (GLfloat)(font->range / 2.0));
    
    /* Render the glyphs. */
    glDrawArrays(GL_POINTS, 0, n);

    /* Clean up. */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glDisableVertexAttribArray(6);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
