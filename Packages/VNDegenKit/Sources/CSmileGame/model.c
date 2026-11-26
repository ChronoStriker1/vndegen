#include "sg.h"

#include "third_party/ufbx.h"

static Image *find_texture(SG *g, const char *modelPath, const char *file) {
    const char *base = file;
    for (const char *p = file; *p; p++)
        if (*p == '\\' || *p == '/') base = p + 1;
    if (!*base) return NULL;
    char dir[1024];
    pack_key(modelPath, dir, sizeof dir);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0;
    else dir[0] = 0;
    char *path = sg_fmt("%s%s", dir, base);
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    free(path);
    if (!data) {
        char key[512];
        pack_key(base, key, sizeof key);
        size_t kn = strlen(key);
        for (int i = 0; i < g->data.pack.count && !data; i++) {
            const char *k = g->data.pack.entries[i].key;
            size_t n = strlen(k);
            if (n > kn && k[n - kn - 1] == '/' && strcmp(k + n - kn, key) == 0) data = pack_read(&g->data.pack, k, &size);
        }
    }
    if (!data) return NULL;
    Image *im = img_decode(data, size);
    free(data);
    return im;
}

Model *model_load(SG *g, const char *path) {
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    if (!data) {
        sg_log(g, "model missing: %s", path);
        return NULL;
    }
    ufbx_load_opts opts = {0};
    ufbx_error error;
    ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
    free(data);
    if (!scene) {
        sg_log(g, "model %s: %s", path, error.description.data);
        return NULL;
    }
    Model *m = sg_alloc(sizeof *m);
    m->textures = sg_alloc(sizeof(Image *) * (scene->materials.count + 1));
    for (size_t i = 0; i < scene->materials.count; i++) {
        ufbx_material *mat = scene->materials.data[i];
        ufbx_texture *tex = mat->fbx.diffuse_color.texture;
        if (!tex && mat->textures.count) tex = mat->textures.data[0].texture;
        m->textures[i] = tex ? find_texture(g, path, tex->relative_filename.length ? tex->relative_filename.data : tex->filename.data) : NULL;
    }
    m->textureCount = (int)scene->materials.count;
    m->modulate = sg_alloc(scene->materials.count + 1);
    char *defPath = sg_strdup(path);
    size_t dn = strlen(defPath);
    if (dn > 4) memcpy(defPath + dn - 4, ".def", 4);
    size_t defSize = 0;
    u8 *def = pack_read(&g->data.pack, defPath, &defSize);
    free(defPath);
    if (def) {
        def[defSize] = 0;
        char *save = NULL, current[256] = {0};
        for (char *line = strtok_r((char *)def, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
            if (strncmp(line, "mtl ", 4) == 0) {
                snprintf(current, sizeof current, "%s", line + 4);
            } else if (strncmp(line, "blend modulate", 14) == 0 && current[0]) {
                const char *want = strstr(current, "::") ? strstr(current, "::") + 2 : current;
                for (size_t i = 0; i < scene->materials.count; i++)
                    if (strcmp(scene->materials.data[i]->name.data, want) == 0 || strcmp(scene->materials.data[i]->name.data, current) == 0)
                        m->modulate[i] = 1;
            }
        }
        free(def);
    }
    uint32_t idx[64];
    for (size_t ni = 0; ni < scene->nodes.count; ni++) {
        ufbx_node *node = scene->nodes.data[ni];
        ufbx_mesh *mesh = node->mesh;
        if (!mesh || !node->visible) continue;
        ufbx_matrix xf = node->geometry_to_world;
        for (size_t fi = 0; fi < mesh->faces.count; fi++) {
            ufbx_face face = mesh->faces.data[fi];
            if (face.num_indices > 64) continue;
            uint32_t nt = ufbx_triangulate_face(idx, 64, mesh, face);
            const Image *tex = NULL;
            bool modulate = false;
            ufbx_material *mat = NULL;
            if (mesh->face_material.count) {
                uint32_t mi = mesh->face_material.data[fi];
                if (mi < node->materials.count) mat = node->materials.data[mi];
            } else if (node->materials.count) {
                mat = node->materials.data[0];
            }
            for (size_t k = 0; k < scene->materials.count && mat; k++)
                if (scene->materials.data[k] == mat) {
                    tex = m->textures[k];
                    modulate = m->modulate[k];
                }
            for (uint32_t t = 0; t < nt; t++) {
                Tri tri = {.tex = tex, .alphaTest = true, .modulate = modulate};
                for (int c = 0; c < 3; c++) {
                    uint32_t ix = idx[t * 3 + (uint32_t)c];
                    ufbx_vec3 p = ufbx_transform_position(&xf, ufbx_get_vertex_vec3(&mesh->vertex_position, ix));
                    ufbx_vec2 uv = mesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh->vertex_uv, ix) : ufbx_zero_vec2;
                    float shade = 1.0f;
                    if (mesh->vertex_normal.exists) {
                        ufbx_vec3 n = ufbx_transform_direction(&xf, ufbx_get_vertex_vec3(&mesh->vertex_normal, ix));
                        float l = (float)(n.x * -0.35 + n.y * 0.85 + n.z * 0.4);
                        shade = 0.62f + 0.38f * (l > 0 ? l : 0);
                    }
                    tri.v[c] = (Vert){(float)p.x, (float)p.y, (float)p.z, (float)uv.x, 1.0f - (float)uv.y, shade};
                }
                mesh_add(&m->mesh, &tri);
            }
        }
    }
    ufbx_free_scene(scene);
    sg_log(g, "model %s: %d triangles", path, m->mesh.count);
    return m;
}

void model_free(Model *m) {
    if (!m) return;
    mesh_free(&m->mesh);
    for (int i = 0; i < m->textureCount; i++) img_free(m->textures[i]);
    free(m->textures);
    free(m->modulate);
    free(m);
}
