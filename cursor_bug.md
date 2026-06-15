Bug Report: Verisilicon DRM Driver (drivers/gpu/drm/verisilicon)
Bug 1 — vs_drm.c: Missing cursor size initialization in mode config
File: drivers/gpu/drm/verisilicon/vs_drm.c
Function: vs_drm_initialize()
Description:
After calling vs_mode_config_init(drm), the driver fails to set drm->mode_config.cursor_width and drm->mode_config.cursor_height. These fields are left at zero, which causes the DRM framework to report zero cursor dimensions to userspace, effectively advertising no cursor support despite the hardware having a dedicated cursor plane.
Original code:
vs_mode_config_init(drm);

/* Enable connectors polling */
drm_kms_helper_poll_init(drm);
Fixed code:
vs_mode_config_init(drm);
drm->mode_config.cursor_width = dc->identity.max_cursor_size;
drm->mode_config.cursor_height = dc->identity.max_cursor_size;

/* Enable connectors polling */
drm_kms_helper_poll_init(drm);
Impact: Hardware cursor support is silently broken for all userspace clients that check cursor_width/cursor_height before issuing cursor ioctls.
Bug 2 — vs_cursor_plane.c: Potential NULL pointer dereference in atomic_update when plane is not visible
File: drivers/gpu/drm/verisilicon/vs_cursor_plane.c
Function: vs_cursor_plane_atomic_update()
Description:
When the new plane state has !state->visible, the original code calls vs_cursor_plane_atomic_disable(plane, atomic_state) directly. That callback internally retrieves the old plane state via drm_atomic_get_old_plane_state() and then dereferences old_state->crtc:
/* inside vs_cursor_plane_atomic_disable */
struct drm_plane_state *state = drm_atomic_get_old_plane_state(atomic_state, plane);
struct drm_crtc *crtc = state->crtc;
struct vs_crtc *vcrtc = drm_crtc_to_vs_crtc(crtc);  /* NULL dereference if crtc == NULL */
When a plane transitions from disabled (old state: crtc = NULL) to an update where it is not visible, the old state carries crtc = NULL. Passing that into drm_crtc_to_vs_crtc() results in a NULL pointer dereference and a kernel crash.
Original code (atomic_update):
if (!state->visible) {
    vs_cursor_plane_atomic_disable(plane, atomic_state); /* uses old state – crtc may be NULL */
    return;
}

vcrtc = drm_crtc_to_vs_crtc(crtc);
output = vcrtc->id;
dc = vcrtc->dc;
Fix: Extract the hardware disable sequence into a dedicated helper vs_cursor_plane_hw_disable(dc, output) that operates on already-resolved dc/output values (derived from the new state's CRTC, which is guaranteed non-NULL inside atomic_update), and call it from both atomic_disable and the not-visible path in atomic_update:
static void vs_cursor_plane_hw_disable(struct vs_dc *dc, unsigned int output)
{
    regmap_update_bits(dc->regs, VSDC_CURSOR_CONFIG(output),
                       VSDC_CURSOR_CONFIG_FMT_MASK,
                       VSDC_CURSOR_CONFIG_FMT_OFF);
    vs_cursor_plane_commit(dc, output);
}

/* in atomic_update */
vcrtc = drm_crtc_to_vs_crtc(crtc);
output = vcrtc->id;
dc = vcrtc->dc;

if (!state->visible) {
    vs_cursor_plane_hw_disable(dc, output); /* safe: derived from new state */
    return;
}
Impact: Kernel NULL pointer dereference (oops/panic) when a cursor plane is enabled for the first time with coordinates that make it entirely off-screen, or when the DRM core calls atomic_update with a not-visible state transition from a previously-disabled plane.