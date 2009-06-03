/*------------------------------------------------------------------------
 *  Copyright 2007-2009 (c) Jeff Brown <spadix@users.sourceforge.net>
 *
 *  This file is part of the ZBar Bar Code Reader.
 *
 *  The ZBar Bar Code Reader is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU Lesser Public License as
 *  published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  The ZBar Bar Code Reader is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with the ZBar Bar Code Reader; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 *
 *  http://sourceforge.net/projects/zbar
 *------------------------------------------------------------------------*/

#include "video.h"
#include "image.h"


#ifdef HAVE_LIBJPEG
extern struct jpeg_decompress_struct *_zbar_jpeg_decomp_create();
extern void _zbar_jpeg_decomp_destroy(struct jpeg_decompress_struct *cinfo);
#endif

static void _zbar_video_recycle_image (zbar_image_t *img)
{
    zbar_video_t *vdo = img->src;
    assert(vdo);
    assert(img->srcidx >= 0);
    (void)video_lock(vdo);
    if(vdo->images[img->srcidx] != img)
        vdo->images[img->srcidx] = img;
    if(vdo->active)
        vdo->nq(vdo, img);
    else
        (void)video_unlock(vdo);
}

static void _zbar_video_recycle_shadow (zbar_image_t *img)
{
    zbar_video_t *vdo = img->src;
    assert(vdo);
    assert(img->srcidx == -1);
    (void)video_lock(vdo);
    img->next = vdo->shadow_image;
    vdo->shadow_image = img;
    (void)video_unlock(vdo);
}

zbar_video_t *zbar_video_create ()
{
    zbar_video_t *vdo = calloc(1, sizeof(zbar_video_t));
    if(!vdo)
        return(NULL);
    err_init(&vdo->err, ZBAR_MOD_VIDEO);
    vdo->fd = -1;

#ifdef HAVE_LIBPTHREAD
    if(pthread_mutex_init(&vdo->qlock, NULL)) {
        free(vdo);
        return(NULL);
    }
#endif

    /* pre-allocate images */
    vdo->num_images = ZBAR_VIDEO_IMAGES_MAX;
    vdo->images = calloc(ZBAR_VIDEO_IMAGES_MAX, sizeof(zbar_image_t*));
    if(!vdo->images) {
        zbar_video_destroy(vdo);
        return(NULL);
    }

    int i;
    for(i = 0; i < ZBAR_VIDEO_IMAGES_MAX; i++) {
        zbar_image_t *img = vdo->images[i] = zbar_image_create();
        if(!img) {
            zbar_video_destroy(vdo);
            return(NULL);
        }
        img->refcnt = 0;
        img->cleanup = _zbar_video_recycle_image;
        img->srcidx = i;
        img->src = vdo;
    }

    return(vdo);
}

void zbar_video_destroy (zbar_video_t *vdo)
{
    if(vdo->fd >= 0)
        zbar_video_open(vdo, NULL);
    if(vdo->images) {
        int i;
        for(i = 0; i < ZBAR_VIDEO_IMAGES_MAX; i++)
            if(vdo->images[i])
                free(vdo->images[i]);
        free(vdo->images);
    }
    while(vdo->shadow_image) {
        zbar_image_t *img = vdo->shadow_image;
        vdo->shadow_image = img->next;
        free((void*)img->data);
        img->data = NULL;
        free(img);
    }
    if(vdo->buf)
        free(vdo->buf);
    if(vdo->formats)
        free(vdo->formats);
    err_cleanup(&vdo->err);
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_destroy(&vdo->qlock);
#endif
#ifdef HAVE_LIBJPEG
    if(vdo->jpeg_img) {
        zbar_image_destroy(vdo->jpeg_img);
        vdo->jpeg_img = NULL;
    }
    if(vdo->jpeg) {
        _zbar_jpeg_decomp_destroy(vdo->jpeg);
        vdo->jpeg = NULL;
    }
#endif
    free(vdo);
}

int zbar_video_open (zbar_video_t *vdo,
                     const char *dev)
{
    return(_zbar_video_open(vdo, dev));
}

int zbar_video_get_fd (const zbar_video_t *vdo)
{
    if(vdo->fd >= 0 && vdo->intf == VIDEO_V4L2)
        return(vdo->fd);

    if(vdo->intf != VIDEO_V4L2)
        return(err_capture(vdo, SEV_WARNING, ZBAR_ERR_UNSUPPORTED, __func__,
                           "v4l1 API does not support polling"));

    return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                       "video device not opened"));
}

int zbar_video_request_size (zbar_video_t *vdo,
                             unsigned width,
                             unsigned height)
{
    if(vdo->initialized)
        /* FIXME re-init different format? */
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                           "already initialized, unable to resize"));

    vdo->width = width;
    vdo->height = height;
    zprintf(1, "request size: %d x %d\n", width, height);
    return(0);
}

int zbar_video_request_interface (zbar_video_t *vdo,
                                  int ver)
{
    if(vdo->fd >= 0)
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                         "device already opened, unable to change interface"));
    vdo->intf = (video_interface_t)ver;
    zprintf(1, "request interface version %d\n", vdo->intf);
    return(0);
}

int zbar_video_request_iomode (zbar_video_t *vdo,
                               int iomode)
{
    if(vdo->fd >= 0)
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                         "device already opened, unable to change iomode"));
    if(iomode < 0 || iomode > VIDEO_USERPTR)
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                         "invalid iomode requested"));
    vdo->iomode = iomode;
    return(0);
}

int zbar_video_get_width (const zbar_video_t *vdo)
{
    return(vdo->width);
}

int zbar_video_get_height (const zbar_video_t *vdo)
{
    return(vdo->height);
}

uint32_t zbar_video_get_format (const zbar_video_t *vdo)
{
    return(vdo->format);
}

static inline int video_init_images (zbar_video_t *vdo)
{
    
    assert(vdo->datalen);
    if(vdo->iomode != VIDEO_MMAP) {
        assert(!vdo->buf);
        vdo->buflen = vdo->num_images * vdo->datalen;
        vdo->buf = malloc(vdo->buflen);
        if(!vdo->buf)
            return(err_capture(vdo, SEV_FATAL, ZBAR_ERR_NOMEM, __func__,
                               "unable to allocate image buffers"));
        zprintf(1, "pre-allocated %d %s buffers size=0x%lx\n", vdo->num_images,
                (vdo->iomode == VIDEO_READWRITE) ? "READ" : "USERPTR",
                vdo->buflen);
    }
    int i;
    for(i = 0; i < vdo->num_images; i++) {
        zbar_image_t *img = vdo->images[i];
        img->format = vdo->format;
        img->width = vdo->width;
        img->height = vdo->height;
        if(vdo->iomode != VIDEO_MMAP) {
            img->datalen = vdo->datalen;
            unsigned long offset = i * vdo->datalen;
            img->data = vdo->buf + offset;
            zprintf(2, "    [%02d] @%08lx\n", i, offset);
        }
        else {
            assert(img->data);
            assert(img->datalen);
            assert(img->datalen >= vdo->datalen);
        }
    }
    return(0);
}

int zbar_video_init (zbar_video_t *vdo,
                     unsigned long fmt)
{
    if(vdo->initialized)
        /* FIXME re-init different format? */
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                           "already initialized, re-init unimplemented"));

    if(vdo->init(vdo, fmt))
        return(-1);
    vdo->format = fmt;
    if(video_init_images(vdo))
        return(-1);
#ifdef HAVE_LIBJPEG
    const zbar_format_def_t *vidfmt = _zbar_format_lookup(fmt);
    if(vidfmt->group == ZBAR_FMT_JPEG) {
        /* prepare for decoding */
        if(!vdo->jpeg)
            vdo->jpeg = _zbar_jpeg_decomp_create();
        if(vdo->jpeg_img)
            zbar_image_destroy(vdo->jpeg_img);

        /* create intermediate image for decoder to use*/
        zbar_image_t *img = vdo->jpeg_img = zbar_image_create();
        img->format = fourcc('Y','8','0','0');
        img->width = vdo->width;
        img->height = vdo->height;
        img->datalen = vdo->width * vdo->height;
    }
#endif
    vdo->initialized = 1;
    return(0);
}

int zbar_video_enable (zbar_video_t *vdo,
                       int enable)
{
    if(vdo->active == enable)
        return(0);

    if(enable) {
        if(vdo->fd < 0)
            return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                               "video device not opened"));

        if(!vdo->initialized &&
           zbar_negotiate_format(vdo, NULL))
            return(-1);
    }

    if(video_lock(vdo))
        return(-1);
    vdo->active = enable;
    if(enable)
        return(vdo->start(vdo));
    else
        return(vdo->stop(vdo));
}

zbar_image_t *zbar_video_next_image (zbar_video_t *vdo)
{
    if(video_lock(vdo))
        return(NULL);
    if(!vdo->active) {
        err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                    "video not enabled");
        (void)video_unlock(vdo);
        return(NULL);
    }
    unsigned frame = vdo->frame++;
    zbar_image_t *img = vdo->dq(vdo);
    if(img) {
        img->seq = frame;
        if(vdo->num_images < 2) {
            /* return a *copy* of the video image and immediately recycle
             * the driver's buffer to avoid deadlocking the resources
             */
            zbar_image_t *tmp = img;
            (void)video_lock(vdo);
            img = vdo->shadow_image;
            vdo->shadow_image = (img) ? img->next : NULL;
            (void)video_unlock(vdo);
                
            if(!img) {
                img = zbar_image_create();
                assert(img);
                img->refcnt = 0;
                img->src = vdo;
                /* recycle the shadow images */

                img->format = vdo->format;
                img->width = vdo->width;
                img->height = vdo->height;
                img->datalen = vdo->datalen;
                img->data = malloc(vdo->datalen);
            }
            img->cleanup = _zbar_video_recycle_shadow;
            img->seq = frame;
            memcpy((void*)img->data, tmp->data, img->datalen);
            _zbar_video_recycle_image(tmp);
        }
        else
            img->cleanup = _zbar_video_recycle_image;
        img->refcnt++;
    }
    return(img);
}