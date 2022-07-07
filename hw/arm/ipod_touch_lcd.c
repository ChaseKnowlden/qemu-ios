#include "hw/arm/ipod_touch_lcd.h"
#include "ui/pixel_ops.h"
#include "ui/console.h"
#include "hw/display/framebuffer.h"

static uint64_t s5l8900_lcd_read(void *opaque, hwaddr addr, unsigned size)
{
    // fprintf(stderr, "%s: read from location 0x%08x\n", __func__, addr);

    IPodTouchLCDState *s = (IPodTouchLCDState *)opaque;
    switch(addr)
    {
        case 0x4:
            return s->lcd_con;
        case 0x8:
            return s->lcd_con2;

        case 0x14:
            return s->unknown1;
        case 0x18:
            return s->unknown2;

        case 0x20:
            return s->wnd_con;

        case 0x200:
            return s->vid_con0;
        case 0x204:
            return s->vid_con1;

        case 0x20C:
            return s->vidt_con0;
        case 0x210:
            return s->vidt_con1;
        case 0x214:
            return s->vidt_con2;
        case 0x218:
            return s->vidt_con3;

        case 0x58:
            return s->w1_hspan;
        case 0x5c:
            return s->w1_display_depth_info;
        case 0x60:
            return s->w1_framebuffer_base;
        case 0x64:
            return s->w1_display_resolution_info;
        case 0x68:
            return s->w1_qlen;

        case 0x70:
            return s->w2_hspan;
        case 0x74:
            return s->w2_display_depth_info;
        case 0x78:
            return s->w2_framebuffer_base;
        case 0x7c:
            return s->w2_display_resolution_info;
        case 0x80:
            return s->w2_qlen;
        default:
            // hw_error("%s: read invalid location 0x%08x.\n", __func__, addr);
            break;
    }
    return 0;
}

static void s5l8900_lcd_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    IPodTouchLCDState *s = (IPodTouchLCDState *)opaque;
    // fprintf(stderr, "%s: writing 0x%08x to 0x%08x\n", __func__, val, addr);

    switch(addr) {
        case 0x4:
            s->lcd_con = val;
            break;
        case 0x8:
            s->lcd_con2 = val;
            break;

        case 0x14:
            s->unknown1 = val;
            if(val & 1) {
                qemu_irq_raise(s->irq);
            }
            break;
        case 0x18:
            s->unknown2 = val;
            qemu_irq_lower(s->irq);
            break;

        case 0x20:
            s->wnd_con = val;
            break;

        case 0x200:
            s->vid_con0 = val;
            break;
        case 0x204:
            s->vid_con1 = val;
            break;

        case 0x20C:
            s->vidt_con0 = val;
            break;
        case 0x210:
            s->vidt_con1 = val;
            break;
        case 0x214:
            s->vidt_con2 = val;
            break;
        case 0x218:
            s->vidt_con3 = val;
            break;

        case 0x58:
            s->w1_hspan = val;
            break;
        case 0x5c:
            s->w1_display_depth_info = val;
            break;
        case 0x60:
            s->w1_framebuffer_base = val;
            break;
        case 0x64:
            s->w1_display_resolution_info = val;
            break;
        case 0x68:
            s->w1_qlen = val;
            break;

        case 0x70:
            s->w2_hspan = val;
            break;
        case 0x74:
            s->w2_display_depth_info = val;
            break;
        case 0x78:
            s->w2_framebuffer_base = val;
            break;
        case 0x7c:
            s->w2_display_resolution_info = val;
            break;
        case 0x80:
            s->w2_qlen = val;
            break;
    }
}

static void lcd_invalidate(void *opaque)
{
    IPodTouchLCDState *s = opaque;
    s->invalidate = 1;
}

static void draw_line32_32(void *opaque, uint8_t *d, const uint8_t *s, int width, int deststep)
{
    uint8_t r, g, b;

    do {
        //v = lduw_le_p((void *) s);
        //printf("V: %d\n", *s);
        b = s[0];
        g = s[1];
        r = s[2];
        //printf("R: %d, G: %d, B: %d\n", r, g, b);
        ((uint32_t *) d)[0] = rgb_to_pixel32(r, g, b);
        s += 4;
        d += 4;
    } while (-- width != 0);
}

static void lcd_refresh(void *opaque)
{
    //fprintf(stderr, "%s: refreshing LCD screen\n", __func__);

    IPodTouchLCDState *lcd = (IPodTouchLCDState *) opaque;
    DisplaySurface *surface = qemu_console_surface(lcd->con);
    drawfn draw_line;
    int src_width, dest_width;
    int height, first, last;
    int width, linesize;

    if (!lcd || !lcd->con || !surface_bits_per_pixel(surface))
        return;

    dest_width = 4;
    draw_line = draw_line32_32;

    /* Resolution */
    first = last = 0;
    width = 320;
    height = 480;
    lcd->invalidate = 1;

    src_width =  4 * width;
    linesize = surface_stride(surface);

    if(lcd->invalidate) {
        framebuffer_update_memory_section(&lcd->fbsection, lcd->sysmem, lcd->w1_framebuffer_base, height, 4 * width);
    }

    framebuffer_update_display(surface, &lcd->fbsection,
                               width, height,
                               src_width,       /* Length of source line, in bytes.  */
                               linesize,        /* Bytes between adjacent horizontal output pixels.  */
                               dest_width,      /* Bytes between adjacent vertical output pixels.  */
                               lcd->invalidate,
                               draw_line, NULL,
                               &first, &last);
    if (first >= 0) {
        dpy_gfx_update(lcd->con, 0, first, width, last - first + 1);
    }
    lcd->invalidate = 0;
}

static const MemoryRegionOps lcd_ops = {
    .read = s5l8900_lcd_read,
    .write = s5l8900_lcd_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const GraphicHwOps s5l8900_gfx_ops = {
    .invalidate  = lcd_invalidate,
    .gfx_update  = lcd_refresh,
};

static void ipod_touch_lcd_mouse_event(void *opaque, int x, int y, int z, int buttons_state)
{
    //printf("CLICKY %d %d %d %d\n", x, y, z, buttons_state);
}

static void s5l8900_lcd_realize(DeviceState *dev, Error **errp)
{
    IPodTouchLCDState *s = IPOD_TOUCH_LCD(dev);
    s->con = graphic_console_init(dev, 0, &s5l8900_gfx_ops, s);
    qemu_console_resize(s->con, 320, 480);

    // add mouse handler
    qemu_add_mouse_event_handler(ipod_touch_lcd_mouse_event, s, 1, "iPod Touch Touchscreen");
}

static void s5l8900_lcd_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    DeviceState *dev = DEVICE(sbd);
    IPodTouchLCDState *s = IPOD_TOUCH_LCD(dev);

    memory_region_init_io(&s->iomem, obj, &lcd_ops, s, "lcd", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void s5l8900_lcd_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = s5l8900_lcd_realize;
}

static const TypeInfo ipod_touch_lcd_info = {
    .name          = TYPE_IPOD_TOUCH_LCD,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IPodTouchLCDState),
    .instance_init = s5l8900_lcd_init,
    .class_init    = s5l8900_lcd_class_init,
};

static void ipod_touch_machine_types(void)
{
    type_register_static(&ipod_touch_lcd_info);
}

type_init(ipod_touch_machine_types)