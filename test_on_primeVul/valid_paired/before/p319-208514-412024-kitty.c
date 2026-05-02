handle_add_command(GraphicsManager *self, const GraphicsCommand *g, const uint8_t *payload, bool *is_dirty, uint32_t iid) {
#define ABRT(code, ...) { set_add_response(#code, __VA_ARGS__); self->loading_image = 0; if (img) img->data_loaded = false; return NULL; }
#define MAX_DATA_SZ (4u * 100000000u)
    has_add_respose = false;
    bool existing, init_img = true;
    Image *img = NULL;
    unsigned char tt = g->transmission_type ? g->transmission_type : 'd';
    enum FORMATS { RGB=24, RGBA=32, PNG=100 };
    uint32_t fmt = g->format ? g->format : RGBA;
    if (tt == 'd' && self->loading_image) init_img = false;
    if (init_img) {
        self->last_init_graphics_command = *g;
        self->last_init_graphics_command.id = iid;
        self->loading_image = 0;
        if (g->data_width > 10000 || g->data_height > 10000) ABRT(EINVAL, "Image too large");
        remove_images(self, add_trim_predicate, 0);
        img = find_or_create_image(self, iid, &existing);
        if (existing) {
            free_load_data(&img->load_data);
            img->data_loaded = false;
            free_refs_data(img);
            *is_dirty = true;
            self->layers_dirty = true;
        } else {
            img->internal_id = internal_id_counter++;
            img->client_id = iid;
        }
        img->atime = monotonic(); img->used_storage = 0;
        img->width = g->data_width; img->height = g->data_height;
        switch(fmt) {
            case PNG:
                if (g->data_sz > MAX_DATA_SZ) ABRT(EINVAL, "PNG data size too large");
                img->load_data.is_4byte_aligned = true;
                img->load_data.is_opaque = false;
                img->load_data.data_sz = g->data_sz ? g->data_sz : 1024 * 100;
                break;
            case RGB:
            case RGBA:
                img->load_data.data_sz = (size_t)g->data_width * g->data_height * (fmt / 8);
                if (!img->load_data.data_sz) ABRT(EINVAL, "Zero width/height not allowed");
                img->load_data.is_4byte_aligned = fmt == RGBA || (img->width % 4 == 0);
                img->load_data.is_opaque = fmt == RGB;
                break;
            default:
                ABRT(EINVAL, "Unknown image format: %u", fmt);
        }
        if (tt == 'd') {
            if (g->more) self->loading_image = img->internal_id;
            img->load_data.buf_capacity = img->load_data.data_sz + (g->compressed ? 1024 : 10);  // compression header
            img->load_data.buf = malloc(img->load_data.buf_capacity);
            img->load_data.buf_used = 0;
            if (img->load_data.buf == NULL) {
                ABRT(ENOMEM, "Out of memory");
                img->load_data.buf_capacity = 0; img->load_data.buf_used = 0;
            }
        }
    } else {
        self->last_init_graphics_command.more = g->more;
        self->last_init_graphics_command.payload_sz = g->payload_sz;
        g = &self->last_init_graphics_command;
        tt = g->transmission_type ? g->transmission_type : 'd';
        fmt = g->format ? g->format : RGBA;
        img = img_by_internal_id(self, self->loading_image);
        if (img == NULL) {
            self->loading_image = 0;
            ABRT(EILSEQ, "More payload loading refers to non-existent image");
        }
    }
    int fd;
    static char fname[2056] = {0};
    switch(tt) {
        case 'd':  // direct
            if (img->load_data.buf_capacity - img->load_data.buf_used < g->payload_sz) {
                if (img->load_data.buf_used + g->payload_sz > MAX_DATA_SZ || fmt != PNG) ABRT(EFBIG, "Too much data");
                img->load_data.buf_capacity = MIN(2 * img->load_data.buf_capacity, MAX_DATA_SZ);
                img->load_data.buf = realloc(img->load_data.buf, img->load_data.buf_capacity);
                if (img->load_data.buf == NULL) {
                    ABRT(ENOMEM, "Out of memory");
                    img->load_data.buf_capacity = 0; img->load_data.buf_used = 0;
                }
            }
            memcpy(img->load_data.buf + img->load_data.buf_used, payload, g->payload_sz);
            img->load_data.buf_used += g->payload_sz;
            if (!g->more) { img->data_loaded = true; self->loading_image = 0; }
            break;
        case 'f': // file
        case 't': // temporary file
        case 's': // POSIX shared memory
            if (g->payload_sz > 2048) ABRT(EINVAL, "Filename too long");
            snprintf(fname, sizeof(fname)/sizeof(fname[0]), "%.*s", (int)g->payload_sz, payload);
            if (tt == 's') fd = shm_open(fname, O_RDONLY, 0);
            else fd = open(fname, O_CLOEXEC | O_RDONLY);
            if (fd == -1) ABRT(EBADF, "Failed to open file %s for graphics transmission with error: [%d] %s", fname, errno, strerror(errno));
            img->data_loaded = mmap_img_file(self, img, fd, g->data_sz, g->data_offset);
            safe_close(fd, __FILE__, __LINE__);
            if (tt == 't') {
                if (global_state.boss) { call_boss(safe_delete_temp_file, "s", fname); }
                else unlink(fname);
            }
            else if (tt == 's') shm_unlink(fname);
            break;
        default:
            ABRT(EINVAL, "Unknown transmission type: %c", g->transmission_type);
    }
    if (!img->data_loaded) return NULL;
    self->loading_image = 0;
    bool needs_processing = g->compressed || fmt == PNG;
    if (needs_processing) {
        uint8_t *buf; size_t bufsz;
#define IB { if (img->load_data.buf) { buf = img->load_data.buf; bufsz = img->load_data.buf_used; } else { buf = img->load_data.mapped_file; bufsz = img->load_data.mapped_file_sz; } }
        switch(g->compressed) {
            case 'z':
                IB;
                if (!inflate_zlib(self, img, buf, bufsz)) {
                    img->data_loaded = false; return NULL;
                }
                break;
            case 0:
                break;
            default:
                ABRT(EINVAL, "Unknown image compression: %c", g->compressed);
        }
        switch(fmt) {
            case PNG:
                IB;
                if (!inflate_png(self, img, buf, bufsz)) {
                    img->data_loaded = false; return NULL;
                }
                break;
            default: break;
        }
#undef IB
        img->load_data.data = img->load_data.buf;
        if (img->load_data.buf_used < img->load_data.data_sz) {
            ABRT(ENODATA, "Insufficient image data: %zu < %zu", img->load_data.buf_used, img->load_data.data_sz);
        }
        if (img->load_data.mapped_file) {
            munmap(img->load_data.mapped_file, img->load_data.mapped_file_sz);
            img->load_data.mapped_file = NULL; img->load_data.mapped_file_sz = 0;
        }
    } else {
        if (tt == 'd') {
            if (img->load_data.buf_used < img->load_data.data_sz) {
                ABRT(ENODATA, "Insufficient image data: %zu < %zu",  img->load_data.buf_used, img->load_data.data_sz);
            } else img->load_data.data = img->load_data.buf;
        } else {
            if (img->load_data.mapped_file_sz < img->load_data.data_sz) {
                ABRT(ENODATA, "Insufficient image data: %zu < %zu",  img->load_data.mapped_file_sz, img->load_data.data_sz);
            } else img->load_data.data = img->load_data.mapped_file;
        }
    }
    size_t required_sz = (size_t)(img->load_data.is_opaque ? 3 : 4) * img->width * img->height;
    if (img->load_data.data_sz != required_sz) ABRT(EINVAL, "Image dimensions: %ux%u do not match data size: %zu, expected size: %zu", img->width, img->height, img->load_data.data_sz, required_sz);
    if (LIKELY(img->data_loaded && send_to_gpu)) {
        send_image_to_gpu(&img->texture_id, img->load_data.data, img->width, img->height, img->load_data.is_opaque, img->load_data.is_4byte_aligned, false, REPEAT_CLAMP);
        free_load_data(&img->load_data);
        self->used_storage += required_sz;
        img->used_storage = required_sz;
    }
    return img;
#undef MAX_DATA_SZ
#undef ABRT
}