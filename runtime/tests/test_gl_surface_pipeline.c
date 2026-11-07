/* Exercise production initialization, not the raw full-surface reference. */
#define PSX_GL_TEST_REFERENCE_TARGET 0
#define main legacy_fixture_main
#include "test_gl_render_thread.c"
#undef main
static int region_pipeline(int scale) {
    if(SDL_Init(SDL_INIT_VIDEO)!=0)return 77;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *win=SDL_CreateWindow("Region pipeline hidden test",0,0,320,240,
        SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!win)return 77;
    gr_set_backend(GR_BACKEND_OPENGL);gr_init(vram);gr_set_scale(scale);
    if(!gl_renderer_init_context(win))return 77;
    gl_renderer_set_pgxp_depth(1);gl_renderer_set_pgxp_color_perspective(1);
    gl_renderer_set_seam_snap(1);s_pgxp_seam=1;
    check(s_region_pool.coverage.rects && s_region_pool.count==1 && !s_primary_surface.fbo,
        "production starts native-only without a full-VRAM HR allocation");
    check(s_out_scale==scale && !s_hiw,"requested scale does not depend on full-VRAM dimensions");
    gr_fill_rect(0,0,1024,512,0x7c00);flush_cpu_upload();
    check(s_region_pool.count==1,"native VRAM fills allocate no enhanced surfaces");
    glb_wide_configure(426,53);
    for(int page=0;page<2;++page) {
        int y=page*240;gr_set_draw_area(0,y,319,y+239);gr_set_draw_offset(0,0);
        glb_wide_set_target(0);
        gr_set_precise_triangle(1,48*65536,(y+48)*65536,
            112*65536,(y+48)*65536,48*65536,(y+112)*65536);
        gr_set_depth_triangle(1,2048,2048,2048);
        gr_draw_gouraud_triangle(48,y+48,31,112,y+48,31,48,y+112,31);
        flush_flat_batch();
        check(s_hr_width==426 && s_hr_height==240 && direct_wide_target(),
            "production draws directly to visible wide allocation");
        uint8_t pixel[4];p_glBindFramebuffer(PSXGL_READ_FRAMEBUFFER,s_hr_fbo);
        glReadPixels(113*scale,60*scale,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        check(pixel[0]==248 && !pixel[1] && !pixel[2],"production PGXP triangle pixels");
        check(present_wide_fbo_impl(0,y,240,0)==1,"production cropped scanout");
    }
    check(s_region_pool.count==3,"two visible display allocations only");
    glb_wide_disable_target();
    gr_set_draw_area(640,256,767,383);
    gr_set_precise_triangle(1,656*65536,272*65536,720*65536,272*65536,656*65536,336*65536);
    gr_set_depth_triangle(1,2048,2048,2048);
    gr_draw_gouraud_triangle(656,272,0x03e0,720,272,0x03e0,656,336,0x03e0);
    flush_flat_batch();
    check(s_region_pool.count==7,"offscreen draw allocates four intersected tiles");
    gr_copy_rect(656,272,160,60,8,8);
    uint16_t locked=0x801f;gr_vram_transfer_in(164,64,1,1,&locked);flush_cpu_upload();
    gr_set_mask_bits(0,1);gr_copy_rect(657,273,164,64,1,1);
    gr_set_mask_bits(1,0);gr_copy_rect(657,273,165,64,1,1);
    gr_set_mask_bits(0,0);
    check(present_wide_fbo_impl(0,0,240,0)==1,"display survives offscreen drawing and copy");
    uint32_t *capture=malloc((size_t)426*240*scale*scale*4);
    check(glb_render_wide_display(capture,426*scale*4,0,0,240)==426*240*scale*scale,
        "diagnostic readback uses cropped display coordinates");
    check(capture[(size_t)61*scale*426*scale+214*scale]==0xff00f800,
        "offscreen enhanced copy reaches the front display");
    check(capture[(size_t)64*scale*426*scale+217*scale]==0xfff80000,
        "mask check protects an uploaded destination in another allocation");
    GlRasterSurface *front=surface_pool_display_find(&s_region_pool,0,0,240);
    p_glBindFramebuffer(PSXGL_READ_FRAMEBUFFER,front->fbo);
    uint8_t stencil=0;glReadPixels((165-front->x)*scale,64*scale,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&stencil);
    check(stencil==1 && (s_vram[64*1024+165]&0x8000),"copy mask-set updates GPU and presentation shadow");
    free(capture);
    present_vram_impl(0,0,320,240,0,1);
    check(glGetError()==GL_NO_ERROR,"production region pipeline GL state");
    gl_renderer_shutdown();SDL_DestroyWindow(win);SDL_Quit();
    return failures?1:0;
}
int main(void) {
    const int scales[]={1,4,9,18};
    for(unsigned i=0;i<4;++i){int result=region_pipeline(scales[i]);if(result)return result;}
    return 0;
}
