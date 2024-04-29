#include "constants.h"
#include "macros.h"

#define __PROFILER_C
#define __LOGGING_C
#include "profiler.h"
#include "logging.h"

#include "stb_image_write.h"
#include "stb_image.h"

#include <stdlib.h>
#include <float.h>
#include <math.h>

typedef struct {
    f64 x, y;
    RGBA color;

    enum {
        UNDEFINED,
        NOISE,
        CLUSTER
    } label;
    u64 cluster;
} Point, *PointPtr;

typedef struct {
    Point *items;
    u64 len;
    u64 cap;

    u64 height;
    u64 width;
} Points;

typedef struct {
    PointPtr *items;
    u64 len;
    u64 cap;
} PointsPtr;

typedef struct {
    f64 x, y;
    f64 r_avg, g_avg, b_avg;

    RGBA color;

    u64 id;
    u64 count;
} Cluster;

typedef struct {
    Cluster *items;
    u64 len;
    u64 cap;
} Clusters;

Clusters BFS(Points *ps, f64(*metric)(Point, Point), f64 eps, u64 min_pts) {
    debug_assert(ps->len == (ps->width * ps->height));

    PointsPtr queue = {0};
    bool *seen = calloc(sizeof(*seen) * ps->len, 1);
    debug_assert(seen && "what the actual fuck");
    Clusters ret = {0};

    for (u64 i = 0; i < ps->len; ++i) {
        PointPtr it = &ps->items[i];

        if (it->label != UNDEFINED)
            continue;

        queue.len = 0;
        memset(seen, 0, sizeof(*seen) * ps->len);
        da_append(&queue, it);
        u64 count = 0;
        while (queue.len) {
            PointPtr qt = queue.items[0];
            s64 x = qt->x;
            s64 y = qt->y;
            bool *saw = &seen[y * ps->width + x];
            if (qt->label != UNDEFINED || *saw) {
                da_remove(&queue, 0);
                continue;
            }
            *saw = true;

            u64 right = x + 1;
            u64 ridx = y * ps->width + right;
            if (right < ps->width) {
                if (metric(ps->items[ridx], *qt) < eps && !seen[ridx])
                    da_append(&queue, &ps->items[ridx]);
            }

            s64 left = x - 1;
            u64 lidx = y * ps->width + left;
            if (left >= 0) {
                if (metric(ps->items[lidx], *qt) < eps && !seen[lidx])
                    da_append(&queue, &ps->items[lidx]);
            }

            s64 up = y - 1;
            u64 uidx = up * ps->width + x;
            if (up >= 0) {
                if (metric(ps->items[uidx], *qt) < eps && !seen[uidx])
                    da_append(&queue, &ps->items[uidx]);
            }

            u64 down = y + 1;
            u64 didx = down * ps->width + x;
            if (down < ps->height) {
                if (metric(ps->items[didx], *qt) < eps && !seen[didx])
                    da_append(&queue, &ps->items[didx]);
            }
            da_remove(&queue, 0);
            count++;
        }

        s32 label = UNDEFINED;
        if (count < min_pts)
            label = NOISE;
        else {
            da_append(&ret, ((Cluster){.x = it->x, .y = it->y,
                        .r_avg = it->color.r / 255.0,
                        .g_avg = it->color.g / 255.0,
                        .b_avg = it->color.b / 255.0,
                        .id = ret.len, .count = 1}));
            label = CLUSTER;
        }

        queue.len = 0;
        memset(seen, 0, sizeof(*seen) * ps->len);
        da_append(&queue, it);
        while (queue.len) {
            PointPtr qt = queue.items[0];
            s64 x = qt->x;
            s64 y = qt->y;
            
            bool *saw = &seen[y * ps->width + x];
            if (qt->label != UNDEFINED || *saw) {
                da_remove(&queue, 0);
                continue;
            }
            *saw = true;

            qt->label = label;
            if (qt->label == CLUSTER) {
                qt->cluster = ret.len - 1;
                ret.items[qt->cluster].x += qt->x;
                ret.items[qt->cluster].y += qt->y;
                ret.items[qt->cluster].r_avg += qt->color.r / 255.0;
                ret.items[qt->cluster].g_avg += qt->color.g / 255.0;
                ret.items[qt->cluster].b_avg += qt->color.b / 255.0;
                ret.items[qt->cluster].count += 1;
            }

            u64 right = x + 1;
            u64 ridx = y * ps->width + right;
            if (right < ps->width) {
                if (metric(ps->items[ridx], *qt) < eps && !seen[ridx])
                    da_append(&queue, &ps->items[ridx]);
            }

            s64 left = x - 1;
            u64 lidx = y * ps->width + left;
            if (left >= 0) {
                if (metric(ps->items[lidx], *qt) < eps && !seen[lidx])
                    da_append(&queue, &ps->items[lidx]);
            }

            s64 up = y - 1;
            u64 uidx = up * ps->width + x;
            if (up >= 0) {
                if (metric(ps->items[uidx], *qt) < eps && !seen[uidx])
                    da_append(&queue, &ps->items[uidx]);
            }

            u64 down = y + 1;
            u64 didx = down * ps->width + x;
            if (down < ps->height) {
                if (metric(ps->items[didx], *qt) < eps && !seen[didx])
                    da_append(&queue, &ps->items[didx]);
            }
            da_remove(&queue, 0);

        }
    }
    free(queue.items);

    for (u64 i = 0; i < ret.len; ++i) {
        Cluster *it = &ret.items[i];
        it->x /= it->count;
        it->y /= it->count;

        it->r_avg /= it->count;
        it->g_avg /= it->count;
        it->b_avg /= it->count;

        it->color = (RGBA){.r = it->r_avg * 255, .g = it->g_avg * 255, .b = it->b_avg * 255, .a = 255};
    }
    return ret;
}

void points_to_image(const Points *ps, s32 width, s32 height, const char *path, const Clusters *c) {
    RGBA *buffer = calloc(width * height * sizeof(*buffer), 1);

    if (!buffer)
        logging_log(LOG_FATAL, "What the actual fuck");

    debug_assert(ps->len == (u64)(width * height));

    for (u64 i = 0; i < ps->len; ++i) {
        Point it = ps->items[i];
        if (it.label == CLUSTER)
            buffer[i] = c->items[it.cluster].color;
        else if (it.label == NOISE)
            buffer[i] = (RGBA){.a = 50, .r = it.color.r, .g = it.color.g, .b = it.color.b};
        else
            buffer[i] = (RGBA){.a = 0};
    }

    //Magic numbers
    for (u64 i = 0; i < c->len; ++i) {
        Cluster *it = &c->items[i];
        if (it->count < 500)
            continue;
        s64 radius = 5;
        for (s64 y = -radius; y <= radius; ++y) {
            for (s64 x = -radius; x <= radius; ++x) {
                if (x * x + y * y > radius * radius)
                    continue;

                s64 xidx = it->x + x;
                s64 yidx = it->y + y;
                if (xidx < 0 || xidx >= width || yidx < 0 || yidx >= height)
                    continue;
                buffer[yidx * width + xidx] = (RGBA){.a = 255, .r = 255, .g = 255, .b = 0};
            }
        }
    }

    for (u64 i = 0; i < c->len; ++i) {
        Cluster *it = &c->items[i];
        if (it->count < 500)
            continue;
        s64 radius = 3;
        for (s64 y = -radius; y <= radius; ++y) {
            for (s64 x = -radius; x <= radius; ++x) {
                if (x * x + y * y > radius * radius)
                    continue;

                s64 xidx = it->x + x;
                s64 yidx = it->y + y;
                if (xidx < 0 || xidx >= width || yidx < 0 || yidx >= height)
                    continue;
                buffer[yidx * width + xidx] = it->color;
            }
        }
    }

    if (!stbi_write_png(path, width, height, 4, buffer, width * sizeof(*buffer)))
        logging_log(LOG_ERROR, "Could not export image from points to %s", path);
    free(buffer);
}

f64 metric(Point p1, Point p2) {
    return ((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y)) +
           sqrt(
                ((f64)p1.color.r - p2.color.r) * ((f64)p1.color.r - p2.color.r) +
                ((f64)p1.color.g - p2.color.g) * ((f64)p1.color.g - p2.color.g) + 
                ((f64)p1.color.b - p2.color.b) * ((f64)p1.color.b - p2.color.b)
           );
}

s32 compare_clusters(const void *a, const void *b) {
    Cluster a_ = *(Cluster*)a;
    Cluster b_ = *(Cluster*)b;
    return (s32)b_.count - (s32)a_.count;
}

int main(s32 argc, const char *argv[]) {
    const char *program_name = argv[0];
    if (argc < 3)
        logging_log(LOG_FATAL, "Usage %s <input_file> <output_file>", program_name);

    const char *input = argv[1];
    const char *output = argv[2];

    Points ps = {0};
    s32 width, height, dummy;
    RGBA *image = (RGBA*)stbi_load(input, &width, &height, &dummy, 4);
    if (!image)
        logging_log(LOG_FATAL, "Could not open %s", input);

    //This is dumb
    for (u64 i = 0; i < (u64)(width * height); ++i)
        da_append(&ps, ((Point){.x = i % width, .y = (i - i % width) / (f64)width, .color = image[i]}));

    ps.width = width;
    ps.height = height;

    profiler_start_measure("BFS");
    Clusters clusters = BFS(&ps, metric, 30, 5);
    profiler_end_measure("BFS");

    points_to_image(&ps, width, height, output, &clusters);

    qsort(clusters.items, clusters.len, sizeof(*clusters.items), compare_clusters);
    for (u64 i = clusters.len; i-->0;)
        logging_log(LOG_INFO, "Cluster %"PRIu64" <x>: %f <y>: %f members: %"PRIu64" color: %08x", clusters.items[i].id, clusters.items[i].x, clusters.items[i].y, clusters.items[i].count, clusters.items[i].color.hex);

    logging_log(LOG_INFO, "Found %"PRIu64" clusters", clusters.len);
    profiler_print_measures(stdout);

    free(ps.items);
    free(clusters.items);
    free(image);
    return 0;
}
