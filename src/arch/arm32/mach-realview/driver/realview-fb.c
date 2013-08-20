/*
 * driver/realview-fb.c
 *
 * realview framebuffer drivers. prime cell lcd controller (pl110)
 *
 * Copyright(c) 2007-2013 jianjun jiang <jerryjianjun@gmail.com>
 * official site: http://xboot.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <xboot.h>
#include <realview-fb.h>

static void fb_init(struct fb_t * fb)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;

	if(dat->init)
		dat->init(dat);

	writel(dat->regbase + REALVIEW_CLCD_OFFSET_TIM0, (dat->timing.h_bp<<24) | (dat->timing.h_fp<<16) | (dat->timing.h_sw<<8) | ((dat->width/16-1)<<2));
	writel(dat->regbase + REALVIEW_CLCD_OFFSET_TIM1, (dat->timing.v_bp<<24) | (dat->timing.v_fp<<16) | (dat->timing.v_sw<<10) | ((dat->height-1)<<0));
	writel(dat->regbase + REALVIEW_CLCD_OFFSET_TIM2, (1<<26) | ((dat->width/16-1)<<16) | (1<<5) | (1<<0));
	writel(dat->regbase + REALVIEW_CLCD_OFFSET_TIM3, (0<<0));

	writel(dat->regbase + REALVIEW_CLCD_OFFSET_IMSC, 0x0);
	writel(dat->regbase + REALVIEW_CLCD_OFFSET_CNTL, REALVIEW_CNTL_LCDBPP24 | REALVIEW_CNTL_LCDTFT | REALVIEW_CNTL_BGR);
	writel(dat->regbase + REALVIEW_CLCD_OFFSET_CNTL, (readl(dat->regbase + REALVIEW_CLCD_OFFSET_CNTL) | REALVIEW_CNTL_LCDEN | REALVIEW_CNTL_LCDPWR));
}

static void fb_exit(struct fb_t * fb)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;

	if(dat->exit)
		dat->exit(dat);
}

static int fb_xcursor(struct fb_t * fb, int ox)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;

	return dat->xcursor(dat, ox);
}

static int fb_ycursor(struct fb_t * fb, int oy)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;

	return dat->ycursor(dat, oy);
}

static int fb_backlight(struct fb_t * fb, int brightness)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;

	if(dat->backlight)
		return dat->backlight(dat, brightness);
	return 0;
}

struct render_t * fb_create(struct fb_t * fb)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;
	struct render_t * render;
	void * pixels;
	size_t size;

	size = dat->width * dat->height * dat->bytes_per_pixel;
	pixels = memalign(4, size);
	if(!pixels)
		return NULL;
	memset(pixels, 0, size);

	render = malloc(sizeof(struct render_t));
	if(!render)
	{
		free(pixels);
		return NULL;
	}

	render->width = dat->width;
	render->height = dat->height;
	render->pitch = (dat->width * dat->bytes_per_pixel + 0x3) & ~0x3;
	render->format = PIXEL_FORMAT_ARGB32;
	render->pixels = pixels;

	render->clear = sw_render_clear;
	render->snapshot = sw_render_snapshot;
	render->alloc_texture = sw_render_alloc_texture;
	render->alloc_texture_similar = sw_render_alloc_texture_similar;
	render->free_texture = sw_render_free_texture;
	render->fill_texture = sw_render_fill_texture;
	render->blit_texture = sw_render_blit_texture;
	sw_render_create_priv_data(render);

	return render;
}

void fb_destroy(struct fb_t * fb, struct render_t * render)
{
	if(render)
	{
		sw_render_destroy_priv_data(render);
		free(render->pixels);
		free(render);
	}
}

void fb_present(struct fb_t * fb, struct render_t * render)
{
	struct resource_t * res = (struct resource_t *)fb->priv;
	struct realview_fb_data_t * dat = (struct realview_fb_data_t *)res->data;
	void * pixels = render->pixels;

	if(pixels)
	{
		writel(dat->regbase + REALVIEW_CLCD_OFFSET_UBAS, ((u32_t)pixels));
		writel(dat->regbase + REALVIEW_CLCD_OFFSET_LBAS, ((u32_t)pixels + dat->width * dat->height * dat->bytes_per_pixel));
	}
}

static void fb_suspend(struct fb_t * fb)
{
}

static void fb_resume(struct fb_t * fb)
{
}

static bool_t realview_register_framebuffer(struct resource_t * res)
{
	struct fb_t * fb;
	char name[32 + 1];

	fb = malloc(sizeof(struct fb_t));
	if(!fb)
		return FALSE;

	snprintf(name, sizeof(name), "%s.%d", res->name, res->id);
	fb->name = name;
	fb->init = fb_init,
	fb->exit = fb_exit,
	fb->xcursor = fb_xcursor,
	fb->ycursor = fb_ycursor,
	fb->backlight = fb_backlight,
	fb->create = fb_create,
	fb->destroy = fb_destroy,
	fb->present = fb_present,
	fb->suspend = fb_suspend,
	fb->resume = fb_resume,
	fb->priv = res;

	if(!register_framebuffer(fb))
		return FALSE;
	return TRUE;
}

static bool_t realview_unregister_framebuffer(struct resource_t * res)
{
	struct fb_t * fb;
	char name[32 + 1];

	snprintf(name, sizeof(name), "%s.%d", res->name, res->id);

	fb = search_framebuffer(name);
	if(!fb)
		return FALSE;

	return unregister_framebuffer(fb);
}

static __init void realview_fb_init(void)
{
	resource_callback_with_name("fb.pl110", realview_register_framebuffer);
}

static __exit void realview_fb_exit(void)
{
	resource_callback_with_name("fb.pl110", realview_unregister_framebuffer);
}

device_initcall(realview_fb_init);
device_exitcall(realview_fb_exit);
