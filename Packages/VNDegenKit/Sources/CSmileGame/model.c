#include "sg.h"

#include <math.h>

#include "third_party/ufbx.h"

static u8 *read_texture(SG *g, const char *dir, const char *base, size_t *size) {
    char *path = sg_fmt("%s%s", dir, base);
    u8 *data = pack_read(&g->data.pack, path, size);
    free(path);
    if (data) return data;
    char key[512];
    pack_key(base, key, sizeof key);
    size_t kn = strlen(key);
    for (int i = 0; i < g->data.pack.count; i++) {
        const char *k = g->data.pack.entries[i].key;
        size_t n = strlen(k);
        if (n > kn && k[n - kn - 1] == '/' && strcmp(k + n - kn, key) == 0) return pack_read(&g->data.pack, k, size);
    }
    return NULL;
}

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
    size_t size = 0;
    u8 *data = read_texture(g, dir, base, &size);
    if (!data) {
        char png[512];
        snprintf(png, sizeof png, "%s", base);
        char *dot = strrchr(png, '.');
        if (dot && strcasecmp(dot, ".png") != 0 && (size_t)(dot - png) + 5 <= sizeof png) {
            strcpy(dot, ".png");
            data = read_texture(g, dir, png, &size);
        }
    }
    if (!data) return NULL;
    Image *im = img_decode(data, size);
    free(data);
    return im;
}

static ufbx_scene *load_scene(SG *g, const char *path) {
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    if (!data) return NULL;
    ufbx_load_opts opts = {0};
    ufbx_error error;
    ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
    free(data);
    if (!scene) sg_log(g, "model %s: %s", path, error.description.data);
    return scene;
}

static void mat_from(float out[16], const ufbx_matrix *m) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 3; r++) out[c * 4 + r] = (float)m->cols[c].v[r];
    out[3] = out[7] = out[11] = 0;
    out[15] = 1;
}

static void mat_mul(float out[16], const float a[16], const float b[16]) {
    float t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) t[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    memcpy(out, t, sizeof t);
}

static const char *material_key(const char *name) {
    const char *k = strstr(name, "::");
    return k ? k + 2 : name;
}

typedef struct MatInfo {
    u8 mode, flags;
} MatInfo;

static void read_def(SG *g, const char *path, ufbx_scene *scene, MatInfo *info, Rig *rig) {
    char *defPath = sg_strdup(path);
    size_t dn = strlen(defPath);
    if (dn > 4) memcpy(defPath + dn - 4, ".def", 4);
    size_t defSize = 0;
    u8 *def = pack_read(&g->data.pack, defPath, &defSize);
    free(defPath);
    if (!def) return;
    def[defSize] = 0;
    char *save = NULL, current[256] = {0};
    bool anim = false;
    double fps = scene->settings.frames_per_second > 0 ? scene->settings.frames_per_second : 30.0;
    for (char *line = strtok_r((char *)def, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        if (strncmp(line, "mtl ", 4) == 0) {
            snprintf(current, sizeof current, "%s", line + 4);
            anim = false;
            continue;
        }
        if (strcmp(line, "anim") == 0) {
            anim = true;
            continue;
        }
        if (anim && rig && scene->anim_stacks.count) {
            char name[32], loop[16] = {0};
            int a = 0, b = -1;
            if (sscanf(line, "%31s %d %d %15s", name, &a, &b, loop) >= 2) {
                if (b < a) b = a;
                rig->clips = sg_realloc(rig->clips, sizeof(Clip) * (size_t)(rig->clipCount + 1));
                Clip *c = &rig->clips[rig->clipCount++];
                memset(c, 0, sizeof *c);
                snprintf(c->name, sizeof c->name, "%s", name);
                c->start = (double)a / fps;
                c->end = (double)b / fps;
                c->loop = strncmp(loop, "loo", 3) == 0;
                c->anim = scene->anim_stacks.data[0]->anim;
            }
            continue;
        }
        if (!current[0]) continue;
        const char *want = material_key(current);
        for (size_t i = 0; i < scene->materials.count; i++) {
            const char *mn = scene->materials.data[i]->name.data;
            if (strcmp(mn, want) != 0 && strcmp(mn, current) != 0) continue;
            if (strncmp(line, "blend modulate", 14) == 0) info[i].mode = TRI_MODULATE, info[i].flags |= TF_NOZWRITE;
            else if (strncmp(line, "blend add", 9) == 0) info[i].mode = TRI_ADD, info[i].flags |= TF_NOZWRITE;
            else if (strncmp(line, "depthwrite false", 16) == 0) info[i].flags |= TF_NOZWRITE;
        }
    }
    free(def);
}

Model *model_load(SG *g, const char *path) {
    ufbx_scene *scene = load_scene(g, path);
    if (!scene) {
        sg_log(g, "model missing: %s", path);
        return NULL;
    }
    Model *m = sg_alloc(sizeof *m);
    Rig *rig = sg_alloc(sizeof *rig);
    m->rig = rig;
    rig->scene = scene;
    char dir[1024];
    pack_key(path, dir, sizeof dir);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0;
    else dir[0] = 0;
    rig->dir = sg_strdup(dir);
    size_t nm = scene->materials.count;
    m->textures = sg_alloc(sizeof(Image *) * (nm + 1));
    for (size_t i = 0; i < nm; i++) {
        ufbx_material *mat = scene->materials.data[i];
        ufbx_texture *tex = mat->fbx.diffuse_color.texture;
        if (!tex && mat->textures.count) tex = mat->textures.data[0].texture;
        m->textures[i] = tex ? find_texture(g, path, tex->relative_filename.length ? tex->relative_filename.data : tex->filename.data) : NULL;
    }
    m->textureCount = (int)nm;
    m->modulate = sg_alloc(nm + 1);
    MatInfo *info = sg_alloc(sizeof(MatInfo) * (nm + 1));
    read_def(g, path, scene, info, rig);
    for (size_t i = 0; i < nm; i++) m->modulate[i] = info[i].mode == TRI_MODULATE;
    int boneCap = 0;
    for (size_t si = 0; si < scene->skin_deformers.count; si++) boneCap += (int)scene->skin_deformers.data[si]->clusters.count;
    rig->boneNodes = sg_alloc(sizeof(void *) * (size_t)(boneCap + 1));
    rig->bind = sg_alloc(sizeof(float[16]) * (size_t)(boneCap + 1));
    rig->world = sg_alloc(sizeof(float[16]) * scene->nodes.count);
    int vcap = 0;
    for (size_t ni = 0; ni < scene->nodes.count; ni++)
        if (scene->nodes.data[ni]->mesh) vcap += (int)scene->nodes.data[ni]->mesh->num_vertices;
    rig->pos = sg_alloc(sizeof(float) * 3 * (size_t)(vcap + 1));
    rig->out = sg_alloc(sizeof(float) * 3 * (size_t)(vcap + 1));
    rig->bone = sg_alloc(sizeof(u8[4]) * (size_t)(vcap + 1));
    rig->weight = sg_alloc(sizeof(float[4]) * (size_t)(vcap + 1));
    rig->vertNode = sg_alloc(sizeof(int) * (size_t)(vcap + 1));
    int ncap = 256;
    rig->nrm = sg_alloc(sizeof(float) * 9 * (size_t)ncap);
    rig->corner = sg_alloc(sizeof(int) * 3 * (size_t)ncap);
    uint32_t idx[64];
    for (size_t ni = 0; ni < scene->nodes.count; ni++) {
        ufbx_node *node = scene->nodes.data[ni];
        ufbx_mesh *mesh = node->mesh;
        if (!mesh || !node->visible) continue;
        int vbase = rig->vertCount;
        ufbx_skin_deformer *skin = mesh->skin_deformers.count ? mesh->skin_deformers.data[0] : NULL;
        int boneBase = rig->boneCount;
        if (skin) {
            for (size_t ci = 0; ci < skin->clusters.count; ci++) {
                ufbx_skin_cluster *cl = skin->clusters.data[ci];
                rig->boneNodes[rig->boneCount] = cl->bone_node;
                mat_from(rig->bind[rig->boneCount], &cl->geometry_to_bone);
                rig->boneCount++;
            }
        }
        ufbx_matrix g2w = node->geometry_to_world;
        for (size_t vi = 0; vi < mesh->num_vertices; vi++) {
            int v = rig->vertCount++;
            ufbx_vec3 p = mesh->vertices.data[vi];
            rig->pos[v * 3] = (float)p.x, rig->pos[v * 3 + 1] = (float)p.y, rig->pos[v * 3 + 2] = (float)p.z;
            rig->vertNode[v] = skin ? -1 : (int)node->typed_id;
            if (skin && vi < skin->vertices.count) {
                ufbx_skin_vertex sv = skin->vertices.data[vi];
                float total = 0;
                for (uint32_t k = 0; k < sv.num_weights && k < 4; k++) {
                    ufbx_skin_weight w = skin->weights.data[sv.weight_begin + k];
                    rig->bone[v][k] = (u8)(boneBase + (int)w.cluster_index);
                    rig->weight[v][k] = (float)w.weight;
                    total += (float)w.weight;
                }
                for (int k = 0; k < 4 && total > 0; k++) rig->weight[v][k] /= total;
            }
        }
        for (size_t fi = 0; fi < mesh->faces.count; fi++) {
            ufbx_face face = mesh->faces.data[fi];
            if (face.num_indices > 64) continue;
            uint32_t nt = ufbx_triangulate_face(idx, 64, mesh, face);
            ufbx_material *mat = NULL;
            if (mesh->face_material.count) {
                uint32_t mi = mesh->face_material.data[fi];
                if (mi < node->materials.count) mat = node->materials.data[mi];
            } else if (node->materials.count) {
                mat = node->materials.data[0];
            }
            Tri base = {.alphaTest = true, .model = 1};
            for (size_t k = 0; k < nm && mat; k++)
                if (scene->materials.data[k] == mat) {
                    base.tex = m->textures[k];
                    base.mode = info[k].mode;
                    base.flags = info[k].flags;
                    base.modulate = info[k].mode == TRI_MODULATE;
                }
            for (uint32_t t = 0; t < nt; t++) {
                if (m->mesh.count >= ncap) {
                    ncap *= 2;
                    rig->nrm = sg_realloc(rig->nrm, sizeof(float) * 9 * (size_t)ncap);
                    rig->corner = sg_realloc(rig->corner, sizeof(int) * 3 * (size_t)ncap);
                }
                Tri tri = base;
                int ti = m->mesh.count;
                for (int c = 0; c < 3; c++) {
                    uint32_t ix = idx[t * 3 + (uint32_t)c];
                    int v = vbase + (int)mesh->vertex_indices.data[ix];
                    ufbx_vec2 uv = mesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh->vertex_uv, ix) : ufbx_zero_vec2;
                    ufbx_vec3 n = mesh->vertex_normal.exists ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix) : (ufbx_vec3){.y = 1};
                    ufbx_vec3 wp = mesh->vertices.data[mesh->vertex_indices.data[ix]];
                    if (!skin) {
                        n = ufbx_transform_direction(&g2w, n);
                        wp = ufbx_transform_position(&g2w, wp);
                    }
                    float nl = sqrtf((float)(n.x * n.x + n.y * n.y + n.z * n.z));
                    if (nl > 0) n.x /= nl, n.y /= nl, n.z /= nl;
                    rig->corner[ti * 3 + c] = v;
                    rig->nrm[ti * 9 + c * 3] = (float)n.x, rig->nrm[ti * 9 + c * 3 + 1] = (float)n.y, rig->nrm[ti * 9 + c * 3 + 2] = (float)n.z;
                    tri.v[c] = (Vert){(float)wp.x, (float)wp.y, (float)wp.z, (float)uv.x, 1.0f - (float)uv.y, 1.0f};
                }
                mesh_add(&m->mesh, &tri);
            }
        }
    }
    free(info);
    rig->outN = sg_alloc(sizeof(float) * 9 * (size_t)(m->mesh.count + 1));
    memcpy(rig->outN, rig->nrm, sizeof(float) * 9 * (size_t)m->mesh.count);
    rig->animated = rig->boneCount > 0 || rig->clipCount > 0;
    rig->g = g;
    sg_log(g, "model %s: %d triangles, %d bones, %d clips", path, m->mesh.count, rig->boneCount, rig->clipCount);
    return m;
}

static void scan_motions(Rig *rig, SG *g) {
    rig->clipsScanned = true;
    if (!g) return;
    char prefix[1100];
    snprintf(prefix, sizeof prefix, "%smotion/", rig->dir);
    size_t pn = strlen(prefix);
    for (int i = 0; i < g->data.pack.count; i++) {
        const char *k = g->data.pack.entries[i].key;
        if (strncmp(k, prefix, pn) != 0) continue;
        const char *file = k + pn;
        size_t fl = strlen(file);
        if (fl < 5 || strcasecmp(file + fl - 4, ".fbx") != 0 || strchr(file, '/')) continue;
        const char *us = strchr(file, '_');
        if (!us) continue;
        char name[32];
        snprintf(name, sizeof name, "%.*s", (int)(fl - 4 - (size_t)(us + 1 - file)), us + 1);
        rig->clips = sg_realloc(rig->clips, sizeof(Clip) * (size_t)(rig->clipCount + 1));
        Clip *c = &rig->clips[rig->clipCount++];
        memset(c, 0, sizeof *c);
        snprintf(c->name, sizeof c->name, "%s", strncmp(name, "o_", 2) == 0 ? name + 2 : name);
        c->scene = sg_strdup(k);
        c->loop = strcmp(c->name, "walk") == 0 || strcmp(c->name, "wait") == 0 || strcmp(c->name, "run") == 0 || strncmp(c->name, "wait", 4) == 0;
        c->start = -1;
    }
}

static Clip *find_clip(Model *m, const char *name, SG *g) {
    Rig *rig = m->rig;
    if (!rig->clipsScanned && rig->clipCount == 0) scan_motions(rig, g);
    rig->clipsScanned = true;
    for (int i = 0; i < rig->clipCount; i++) {
        Clip *c = &rig->clips[i];
        if (strcasecmp(c->name, name) != 0) continue;
        if (c->start < 0) {
            ufbx_scene *s = g ? load_scene(g, (const char *)c->scene) : NULL;
            free(c->scene);
            c->scene = s;
            c->start = 0;
            if (!s || !s->anim_stacks.count) return NULL;
            ufbx_anim_stack *st = s->anim_stacks.data[0];
            c->anim = st->anim;
            c->start = st->time_begin;
            c->end = st->time_end;
            ufbx_scene *ms = rig->scene;
            c->map = sg_alloc(sizeof(void *) * ms->nodes.count);
            for (size_t ni = 0; ni < ms->nodes.count; ni++) {
                const char *nn = ms->nodes.data[ni]->name.data;
                for (size_t k = 0; k < s->nodes.count; k++)
                    if (nn[0] && strcmp(s->nodes.data[k]->name.data, nn) == 0) {
                        c->map[ni] = s->nodes.data[k];
                        break;
                    }
            }
        }
        return c->anim ? c : NULL;
    }
    return NULL;
}

bool model_has_motion(const Model *m, const char *motion) {
    if (!m || !m->rig) return false;
    return find_clip((Model *)m, motion, m->rig->g) != NULL;
}

const char *model_rest_clip(Model *m) {
    Rig *rig = m->rig;
    if (!rig) return "";
    static const char *prefer[] = {"wait", "keepClose", "close", "lightOff"};
    for (int i = 0; i < 4; i++)
        if (model_has_motion(m, prefer[i])) return prefer[i];
    return rig->clipCount ? rig->clips[0].name : "";
}

const Mesh *model_mesh(const Model *m) { return m->rig && m->rig->posed.count ? &m->rig->posed : &m->mesh; }

const float *model_normals(const Model *m) { return m->rig ? (m->rig->posed.count ? m->rig->outN : m->rig->nrm) : NULL; }

void model_pose(Model *m, const char *motion, float seconds) {
    Rig *rig = m->rig;
    if (!rig || !rig->animated) return;
    ufbx_scene *s = rig->scene;
    Clip *c = motion && motion[0] ? find_clip(m, motion, rig->g) : NULL;
    double t = 0;
    if (c) {
        double len = c->end - c->start;
        t = seconds;
        if (len > 0) t = c->loop ? fmod(t, len) : fmin(t, len);
        t += c->start;
    }
    for (size_t ni = 0; ni < s->nodes.count; ni++) {
        ufbx_node *n = s->nodes.data[ni];
        ufbx_transform tr = n->local_transform;
        if (c) {
            if (c->map && c->map[ni]) tr = ufbx_evaluate_transform((ufbx_anim *)c->anim, (ufbx_node *)c->map[ni], t);
            else if (!c->map) tr = ufbx_evaluate_transform((ufbx_anim *)c->anim, n, t);
        }
        ufbx_matrix lm = ufbx_transform_to_matrix(&tr);
        float local[16];
        mat_from(local, &lm);
        int pi = n->parent ? (int)n->parent->typed_id : -1;
        if (pi >= 0) mat_mul(rig->world[ni], rig->world[pi], local);
        else memcpy(rig->world[ni], local, sizeof local);
    }
    float (*skinM)[16] = sg_alloc(sizeof(float[16]) * (size_t)(rig->boneCount + 1));
    for (int b = 0; b < rig->boneCount; b++) {
        int bi = rig->boneNodes[b] ? (int)((ufbx_node *)rig->boneNodes[b])->typed_id : -1;
        if (bi >= 0) mat_mul(skinM[b], rig->world[bi], rig->bind[b]);
        else memcpy(skinM[b], rig->bind[b], sizeof skinM[b]);
    }
    float *vm = sg_alloc(sizeof(float) * 12 * (size_t)(rig->vertCount + 1));
    for (int v = 0; v < rig->vertCount; v++) {
        float M[12] = {0};
        if (rig->vertNode[v] >= 0) {
            int ni = rig->vertNode[v];
            float g2n[16], R[16];
            mat_from(g2n, &s->nodes.data[ni]->geometry_to_node);
            mat_mul(R, rig->world[ni], g2n);
            for (int i = 0; i < 3; i++) M[i] = R[i], M[3 + i] = R[4 + i], M[6 + i] = R[8 + i], M[9 + i] = R[12 + i];
        } else {
            for (int k = 0; k < 4; k++) {
                float w = rig->weight[v][k];
                if (w <= 0) continue;
                const float *B = skinM[rig->bone[v][k]];
                for (int i = 0; i < 3; i++) M[i] += B[i] * w, M[3 + i] += B[4 + i] * w, M[6 + i] += B[8 + i] * w, M[9 + i] += B[12 + i] * w;
            }
        }
        memcpy(vm + v * 12, M, sizeof M);
        const float *p = rig->pos + v * 3;
        for (int i = 0; i < 3; i++) rig->out[v * 3 + i] = M[i] * p[0] + M[3 + i] * p[1] + M[6 + i] * p[2] + M[9 + i];
    }
    if (rig->posed.cap < m->mesh.count) {
        rig->posed.cap = m->mesh.count;
        rig->posed.tris = sg_realloc(rig->posed.tris, sizeof(Tri) * (size_t)m->mesh.count);
    }
    rig->posed.count = m->mesh.count;
    for (int ti = 0; ti < m->mesh.count; ti++) {
        Tri t2 = m->mesh.tris[ti];
        for (int k = 0; k < 3; k++) {
            int v = rig->corner[ti * 3 + k];
            t2.v[k].x = rig->out[v * 3];
            t2.v[k].y = rig->out[v * 3 + 1];
            t2.v[k].z = rig->out[v * 3 + 2];
            const float *M = vm + v * 12, *n = rig->nrm + ti * 9 + k * 3;
            float nx = M[0] * n[0] + M[3] * n[1] + M[6] * n[2], ny = M[1] * n[0] + M[4] * n[1] + M[7] * n[2], nz = M[2] * n[0] + M[5] * n[1] + M[8] * n[2];
            float l = sqrtf(nx * nx + ny * ny + nz * nz);
            if (l > 0) nx /= l, ny /= l, nz /= l;
            rig->outN[ti * 9 + k * 3] = nx, rig->outN[ti * 9 + k * 3 + 1] = ny, rig->outN[ti * 9 + k * 3 + 2] = nz;
        }
        rig->posed.tris[ti] = t2;
    }
    free(vm);
    free(skinM);
}

void model_free(Model *m) {
    if (!m) return;
    mesh_free(&m->mesh);
    for (int i = 0; i < m->textureCount; i++) img_free(m->textures[i]);
    free(m->textures);
    free(m->modulate);
    Rig *rig = m->rig;
    if (rig) {
        for (int i = 0; i < rig->clipCount; i++) {
            Clip *c = &rig->clips[i];
            if (c->start < 0) free(c->scene);
            else if (c->scene) ufbx_free_scene((ufbx_scene *)c->scene);
            free(c->map);
        }
        free(rig->clips);
        ufbx_free_scene((ufbx_scene *)rig->scene);
        free(rig->dir);
        free(rig->pos);
        free(rig->nrm);
        free(rig->out);
        free(rig->outN);
        free(rig->bone);
        free(rig->weight);
        free(rig->vertNode);
        free(rig->corner);
        free(rig->boneNodes);
        free(rig->bind);
        free(rig->world);
        mesh_free(&rig->posed);
        free(rig);
    }
    free(m);
}
